/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco popup window thing showing windows you can tab to */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2005 Elijah Newren
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>

#include "util.h"
#include "core.h"
#include "tabpopup.h"
/* FIXME these two includes are 100% broken ...
 */
#include "../core/workspace.h"
#include "../core/frame-private.h"
#include "draw-workspace.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <math.h>

#define OUTSIDE_SELECT_RECT 2
#define INSIDE_SELECT_RECT 2

typedef struct _TabEntry TabEntry;

struct _TabEntry
{
  MetaTabEntryKey  key;
  char            *title;
  GdkPixbuf       *icon, *dimmed_icon;
  GtkWidget       *widget;
  GdkRectangle     rect;
  GdkRectangle     inner_rect;
  guint blank : 1;
};

struct _MetaTabPopup
{
  GtkWidget *window;
  GtkWidget *label;
  GList *current;
  GList *entries;
  TabEntry *current_selected_entry;
  GtkWidget *outline_window;
  gint border;
};

static GtkWidget* selectable_image_new (GdkPixbuf *pixbuf);
static void       select_image         (GtkWidget *widget);
static void       unselect_image       (GtkWidget *widget);

static GtkWidget* selectable_workspace_new (MetaWorkspace *workspace,
                                                           int entry_count);
static void       select_workspace         (GtkWidget *widget);
static void       unselect_workspace       (GtkWidget *widget);

static gboolean
outline_window_draw (GtkWidget *widget,
                     cairo_t   *cr,
                     gpointer   data)
{
  MetaTabPopup *popup;
  TabEntry *te;

  popup = data;

  if (popup->border & BORDER_OUTLINE_WORKSPACE ||
        popup->current_selected_entry == NULL)
  {
    return FALSE;
  }

  te = popup->current_selected_entry;

  cairo_set_line_width (cr, 1.0);
  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

  cairo_rectangle (cr,
                   0.5, 0.5,
                   te->rect.width - 1,
                   te->rect.height - 1);
  cairo_stroke (cr);

  cairo_rectangle (cr,
                   te->inner_rect.x - 0.5, te->inner_rect.y - 0.5,
                   te->inner_rect.width + 1,
                   te->inner_rect.height + 1);
  cairo_stroke (cr);

  return FALSE;
}

static GdkPixbuf*
dimm_icon (GdkPixbuf *pixbuf)
{
  int x, y, pixel_stride, row_stride;
  guchar *row, *pixels;
  int w, h;
  GdkPixbuf *dimmed_pixbuf;

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    {
      dimmed_pixbuf = gdk_pixbuf_copy (pixbuf);
    }
  else
    {
      dimmed_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
    }

  w = gdk_pixbuf_get_width (dimmed_pixbuf);
  h = gdk_pixbuf_get_height (dimmed_pixbuf);

  pixel_stride = 4;

  row = gdk_pixbuf_get_pixels (dimmed_pixbuf);
  row_stride = gdk_pixbuf_get_rowstride (dimmed_pixbuf);

  for (y = 0; y < h; y++)
    {
      pixels = row;
      for (x = 0; x < w; x++)
        {
          pixels[3] /= 2;
          pixels += pixel_stride;
        }
      row += row_stride;
    }
  return dimmed_pixbuf;
}

static TabEntry*
tab_entry_new (const MetaTabEntry *entry,
               gboolean            outline,
               gint                scale)
{
  TabEntry *te;

  te = g_new (TabEntry, 1);
  te->key = entry->key;
  te->title = NULL;
  if (entry->title)
    {
      gchar *str;
      gchar *tmp;
      gchar *formatter = "%s";

      str = meta_g_utf8_strndup (entry->title, 4096);

      if (entry->hidden)
        {
          formatter = "[%s]";
        }

      tmp = g_markup_printf_escaped (formatter, str);
      g_free (str);
      str = tmp;

      if (entry->demands_attention)
        {
          /* Escape the whole line of text then markup the text and
           * copy it back into the original buffer.
           */
          tmp = g_strdup_printf ("<b>%s</b>", str);
          g_free (str);
          str = tmp;
        }

        te->title=g_strdup(str);

      g_free (str);
    }
  te->widget = NULL;
  te->icon = entry->icon;
  te->blank = entry->blank;
  te->dimmed_icon = NULL;
  if (te->icon)
    {
      g_object_ref (G_OBJECT (te->icon));
      if (entry->hidden)
        te->dimmed_icon = dimm_icon (entry->icon);
    }

  if (outline)
    {
      te->rect.x = entry->rect.x / scale;
      te->rect.y = entry->rect.y / scale;
      te->rect.width = entry->rect.width / scale;
      te->rect.height = entry->rect.height / scale;

      te->inner_rect.x = entry->inner_rect.x / scale;
      te->inner_rect.y = entry->inner_rect.y / scale;
      te->inner_rect.width = entry->inner_rect.width / scale;
      te->inner_rect.height = entry->inner_rect.height / scale;
    }
  return te;
}

MetaTabPopup*
meta_ui_tab_popup_new (const MetaTabEntry *entries,
                       int                 entry_count,
                       int                 width,
                       gint                border)
{
  MetaTabPopup *popup;
  int i, left, right, top, bottom;
  int height;
  GtkWidget *grid;
  GtkWidget *vbox;
  GList *tmp;
  GtkWidget *frame;
  int max_label_width; /* the actual max width of the labels we create */
  AtkObject *obj;
  GdkScreen *screen;
  int screen_width, scale;

  popup = g_new (MetaTabPopup, 1);

  screen = gdk_display_get_default_screen (gdk_display_get_default ());

  if (border & BORDER_OUTLINE_WINDOW)
    {
      GdkRGBA black = { 0.0, 0.0, 0.0, 1.0 };

      popup->outline_window = gtk_window_new (GTK_WINDOW_POPUP);

      gtk_window_set_screen (GTK_WINDOW (popup->outline_window),
                             screen);

      gtk_widget_set_app_paintable (popup->outline_window, TRUE);
      gtk_widget_realize (popup->outline_window);

      gdk_window_set_background_rgba (gtk_widget_get_window (popup->outline_window),
                                      &black);

      g_signal_connect (G_OBJECT (popup->outline_window), "draw",
                        G_CALLBACK (outline_window_draw), popup);

      gtk_widget_show (popup->outline_window);
    }
  else
    popup->outline_window = NULL;

  popup->window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_window_set_screen (GTK_WINDOW (popup->window),
                         screen);

  gtk_window_set_position (GTK_WINDOW (popup->window),
                           GTK_WIN_POS_CENTER_ALWAYS);
  /* enable resizing, to get never-shrink behavior */
  gtk_window_set_resizable (GTK_WINDOW (popup->window),
                            TRUE);
  popup->current = NULL;
  popup->entries = NULL;
  popup->current_selected_entry = NULL;
  popup->border = border;

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (popup->window));
  screen_width = WidthOfScreen (gdk_x11_screen_get_xscreen (screen));
  for (i = 0; i < entry_count; ++i)
    {
      TabEntry* new_entry = tab_entry_new (&entries[i], border & BORDER_OUTLINE_WINDOW, scale);
      popup->entries = g_list_prepend (popup->entries, new_entry);
    }

  popup->entries = g_list_reverse (popup->entries);

  g_assert (width > 0);
  height = i / width;
  if (i % width)
    height += 1;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  grid = gtk_grid_new ();

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 1);
  gtk_container_add (GTK_CONTAINER (popup->window),
                     frame);
  gtk_container_add (GTK_CONTAINER (frame),
                     vbox);

  gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

  popup->label = gtk_label_new ("");

  /* Set the accessible role of the label to a status bar so it
   * will emit name changed events that can be used by screen
   * readers.
   */
  obj = gtk_widget_get_accessible (popup->label);
  atk_object_set_role (obj, ATK_ROLE_STATUSBAR);

  gtk_widget_set_margin_start (popup->label, 3);
  gtk_widget_set_margin_end (popup->label, 3);
  gtk_widget_set_margin_top (popup->label, 3);
  gtk_widget_set_margin_bottom (popup->label, 3);

  gtk_box_pack_end (GTK_BOX (vbox), popup->label, FALSE, FALSE, 0);

  max_label_width = 0;
  top = 0;
  bottom = 1;
  tmp = popup->entries;

  while (tmp && top < height)
    {
      left = 0;
      right = 1;

      while (tmp && left < width)
        {
          GtkWidget *image;
          GtkRequisition req;

          TabEntry *te;

          te = tmp->data;

          if (te->blank)
            {
              /* just stick a widget here to avoid special cases */
              image = gtk_label_new ("");
            }
          else if (border & BORDER_OUTLINE_TAB)
            {
              if (te->dimmed_icon)
                {
                  image = selectable_image_new (te->dimmed_icon);
                }
              else
                {
                  image = selectable_image_new (te->icon);
                }

              gtk_misc_set_padding (GTK_MISC (image),
                                    INSIDE_SELECT_RECT + OUTSIDE_SELECT_RECT + 1,
                                    INSIDE_SELECT_RECT + OUTSIDE_SELECT_RECT + 1);
              gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
              gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
            }
          else
            {
              image = selectable_workspace_new ((MetaWorkspace *) te->key, entry_count);
            }

          te->widget = image;

          gtk_grid_attach (GTK_GRID (grid), te->widget, left, top, 1, 1);

          /* Efficiency rules! */
          gtk_label_set_markup (GTK_LABEL (popup->label),
                              te->title);
          gtk_widget_get_preferred_size (popup->label, &req, NULL);
          max_label_width = MAX (max_label_width, req.width);

          tmp = tmp->next;

          ++left;
          ++right;
        }

      ++top;
      ++bottom;
    }

  /* remove all the temporary text */
  gtk_label_set_text (GTK_LABEL (popup->label), "");
  /* Make it so that we ellipsize if the text is too long */
  gtk_label_set_ellipsize (GTK_LABEL (popup->label), PANGO_ELLIPSIZE_END);

  /* Limit the window size to no bigger than screen_width/4 */
  if (max_label_width>(screen_width/4))
    {
      max_label_width = screen_width/4;
    }

  max_label_width += 20; /* add random padding */

  gtk_window_set_default_size (GTK_WINDOW (popup->window),
                               max_label_width,
                               -1);

  return popup;
}

static void
free_tab_entry (gpointer data, gpointer user_data)
{
  TabEntry *te;

  te = data;

  g_free (te->title);
  if (te->icon)
    g_object_unref (G_OBJECT (te->icon));
  if (te->dimmed_icon)
    g_object_unref (G_OBJECT (te->dimmed_icon));

  g_free (te);
}

void
meta_ui_tab_popup_free (MetaTabPopup *popup)
{
  meta_verbose ("Destroying tab popup window\n");

  if (popup->outline_window != NULL)
    gtk_widget_destroy (popup->outline_window);
  gtk_widget_destroy (popup->window);

  g_list_foreach (popup->entries, free_tab_entry, NULL);

  g_list_free (popup->entries);

  g_free (popup);
}

void
meta_ui_tab_popup_set_showing (MetaTabPopup *popup,
                               gboolean      showing)
{
  if (showing)
    {
      gtk_widget_show_all (popup->window);
    }
  else
    {
      if (gtk_widget_get_visible (popup->window))
        {
          meta_verbose ("Hiding tab popup window\n");
          gtk_widget_hide (popup->window);
          meta_core_increment_event_serial (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
        }
    }
}

static void
display_entry (MetaTabPopup *popup,
               TabEntry     *te)
{
  if (popup->current_selected_entry)
  {
    if (popup->border & BORDER_OUTLINE_TAB)
      unselect_image (popup->current_selected_entry->widget);
    else
      unselect_workspace (popup->current_selected_entry->widget);
  }

  gtk_label_set_markup (GTK_LABEL (popup->label), te->title);

  if (popup->border & BORDER_OUTLINE_TAB)
    select_image (te->widget);
  else
    select_workspace (te->widget);

  if (popup->border & BORDER_OUTLINE_WINDOW)
    {
      GdkRectangle rect;
      GdkWindow *window;
      cairo_region_t *region;

      window = gtk_widget_get_window (popup->outline_window);
      /* Do stuff behind gtk's back */
      gdk_window_hide (window);
      meta_core_increment_event_serial (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

      rect = te->rect;
      rect.x = 0;
      rect.y = 0;

      gtk_window_move (GTK_WINDOW (popup->outline_window), te->rect.x, te->rect.y);
      gtk_window_resize (GTK_WINDOW (popup->outline_window), te->rect.width, te->rect.height);

      region = cairo_region_create_rectangle (&rect);
      cairo_region_subtract_rectangle (region, &te->inner_rect);

      gdk_window_shape_combine_region (gtk_widget_get_window (popup->outline_window),
                                       region,
                                       0, 0);

      cairo_region_destroy (region);
      gdk_window_show_unraised (window);
    }

  /* Must be before we handle an expose for the outline window */
  popup->current_selected_entry = te;
}

void
meta_ui_tab_popup_forward (MetaTabPopup *popup)
{
  if (popup->current != NULL)
    popup->current = popup->current->next;

  if (popup->current == NULL)
    popup->current = popup->entries;

  if (popup->current != NULL)
    {
      TabEntry *te;

      te = popup->current->data;

      display_entry (popup, te);
    }
}

void
meta_ui_tab_popup_backward (MetaTabPopup *popup)
{
  if (popup->current != NULL)
    popup->current = popup->current->prev;

  if (popup->current == NULL)
    popup->current = g_list_last (popup->entries);

  if (popup->current != NULL)
    {
      TabEntry *te;

      te = popup->current->data;

      display_entry (popup, te);
    }
}

MetaTabEntryKey
meta_ui_tab_popup_get_selected (MetaTabPopup *popup)
{
  if (popup->current)
    {
      TabEntry *te;

      te = popup->current->data;

      return te->key;
    }
  else
    return (MetaTabEntryKey)None;
}

void
meta_ui_tab_popup_select (MetaTabPopup *popup,
                          MetaTabEntryKey key)
{
  GList *tmp;

  /* Note, "key" may not be in the list of entries; other code assumes
   * it's OK to pass in a key that isn't.
   */

  tmp = popup->entries;
  while (tmp != NULL)
    {
      TabEntry *te;

      te = tmp->data;

      if (te->key == key)
        {
          popup->current = tmp;

          display_entry (popup, te);

          return;
        }

      tmp = tmp->next;
    }
}

GtkWidget*
meta_ui_tab_popup_get_widget (MetaTabPopup *popup)
{
  if (popup != NULL) 
    return popup->window; 
  else
    return NULL;
}

void
meta_ui_tab_popup_mouse_press (MetaTabPopup       *popup,
                               gint                x,
                               gint                y)
{
  GList *tmp = popup->entries;
  gboolean found = FALSE;
  while (tmp != NULL && !found)
    {
      TabEntry *te = tmp->data;
      gint wx, wy;
      if (gtk_widget_translate_coordinates(popup->window,
                                           te->widget, 
                                           x, y, 
                                           &wx, &wy))
        {
          GtkAllocation alloc;
          gtk_widget_get_allocation(te->widget, &alloc);
          found = (0 <= wx && wx < alloc.width &&
                   0 <= wy && wy < alloc.height);
          if (found)
            {
              popup->current = tmp;
              display_entry (popup, te);
            }
        }
      tmp = tmp->next;
    }
}


#define META_TYPE_SELECT_IMAGE            (meta_select_image_get_type ())
#define META_SELECT_IMAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_SELECT_IMAGE, MetaSelectImage))

typedef struct _MetaSelectImage       MetaSelectImage;
typedef struct _MetaSelectImageClass  MetaSelectImageClass;

struct _MetaSelectImage
{
  GtkImage parent_instance;
  guint selected : 1;
};

struct _MetaSelectImageClass
{
  GtkImageClass parent_class;
};


static GType meta_select_image_get_type (void) G_GNUC_CONST;

static GtkWidget*
selectable_image_new (GdkPixbuf *pixbuf)
{
  GtkWidget *w;

  w = g_object_new (meta_select_image_get_type (), NULL);
  gtk_image_set_from_pixbuf (GTK_IMAGE (w), pixbuf);

  return w;
}

static void
select_image (GtkWidget *widget)
{
  META_SELECT_IMAGE (widget)->selected = TRUE;
  gtk_widget_queue_draw (widget);
}

static void
unselect_image (GtkWidget *widget)
{
  META_SELECT_IMAGE (widget)->selected = FALSE;
  gtk_widget_queue_draw (widget);
}

static void     meta_select_image_class_init   (MetaSelectImageClass *klass,
                                                void                 *data);

static gboolean meta_select_image_draw         (GtkWidget            *widget,
                                                cairo_t              *cr);

static GtkImageClass *parent_class;

GType
meta_select_image_get_type (void)
{
  static GType image_type = 0;

  if (!image_type)
    {
      static const GTypeInfo image_info =
      {
        sizeof (MetaSelectImageClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) meta_select_image_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (MetaSelectImage),
        16,             /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };

      image_type = g_type_register_static (GTK_TYPE_IMAGE, "MetaSelectImage", &image_info, 0);
    }

  return image_type;
}

static void
meta_select_image_class_init (MetaSelectImageClass *klass, void *data)
{
  GtkWidgetClass *widget_class;

  parent_class = g_type_class_peek (gtk_image_get_type ());

  widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->draw = meta_select_image_draw;
}

static gboolean
meta_select_image_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  if (META_SELECT_IMAGE (widget)->selected)
    {
      GtkMisc *misc;
      GtkRequisition requisition;
      GtkStyleContext *context;
      GdkRGBA color;
      int x, y, w, h;
      gint xpad, ypad;
      gfloat xalign, yalign;

      misc = GTK_MISC (widget);

      gtk_widget_get_preferred_size (widget, &requisition, NULL);
      gtk_misc_get_alignment (misc, &xalign, &yalign);
      gtk_misc_get_padding (misc, &xpad, &ypad);

      x = (allocation.width - (requisition.width - xpad * 2)) * xalign + 0.5;
      y = (allocation.height - (requisition.height - ypad * 2)) * yalign + 0.5;

      x -= INSIDE_SELECT_RECT + 1;
      y -= INSIDE_SELECT_RECT + 1;

      w = requisition.width - OUTSIDE_SELECT_RECT * 2 - 1;
      h = requisition.height - OUTSIDE_SELECT_RECT * 2 - 1;

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_set_state (context,
                                   gtk_widget_get_state_flags (widget));

      gtk_style_context_lookup_color (context, "color", &color);

      cairo_set_line_width (cr, 2.0);
      cairo_set_source_rgb (cr, color.red, color.green, color.blue);

      cairo_rectangle (cr, x, y, w + 1, h + 1);
      cairo_stroke (cr);

      cairo_set_line_width (cr, 1.0);
    }

  return GTK_WIDGET_CLASS (parent_class)->draw (widget, cr);
}

#define META_TYPE_SELECT_WORKSPACE   (meta_select_workspace_get_type ())
#define META_SELECT_WORKSPACE(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_SELECT_WORKSPACE, MetaSelectWorkspace))

typedef struct _MetaSelectWorkspace       MetaSelectWorkspace;
typedef struct _MetaSelectWorkspaceClass  MetaSelectWorkspaceClass;

struct _MetaSelectWorkspace
{
  GtkDrawingArea parent_instance;
  MetaWorkspace *workspace;
  guint selected : 1;
};

struct _MetaSelectWorkspaceClass
{
  GtkDrawingAreaClass parent_class;
};


static GType meta_select_workspace_get_type (void) G_GNUC_CONST;

#define SELECT_OUTLINE_WIDTH 2
#define MINI_WORKSPACE_SCALE 2

static GtkWidget*
selectable_workspace_new (MetaWorkspace *workspace, int entry_count)
{
  GtkWidget *widget;
  const MetaXineramaScreenInfo *current;
  int mini_workspace_width, mini_workspace_height;
  double mini_workspace_ratio;

  widget = g_object_new (meta_select_workspace_get_type (), NULL);

  current = meta_screen_get_current_xinerama (workspace->screen);

  if (workspace->screen->rect.width < workspace->screen->rect.height)
  {
    mini_workspace_ratio = (double) workspace->screen->rect.width / (double) workspace->screen->rect.height;
    mini_workspace_height = (int) ((double) current->rect.height / entry_count - SELECT_OUTLINE_WIDTH * 2);
    mini_workspace_width = (int) ((double) mini_workspace_height * mini_workspace_ratio);
  }
  else
  {
    mini_workspace_ratio = (double) workspace->screen->rect.height / (double) workspace->screen->rect.width;
    mini_workspace_width = (int) ((double) current->rect.width / entry_count - SELECT_OUTLINE_WIDTH * 2);
    mini_workspace_height = (int) ((double) mini_workspace_width * mini_workspace_ratio);
  }

  /* account for select rect */
  gtk_widget_set_size_request (widget,
                               mini_workspace_width / MINI_WORKSPACE_SCALE,
                               mini_workspace_height / MINI_WORKSPACE_SCALE);

  META_SELECT_WORKSPACE (widget)->workspace = workspace;

  return widget;
}

static void
select_workspace (GtkWidget *widget)
{
  META_SELECT_WORKSPACE(widget)->selected = TRUE;
  gtk_widget_queue_draw (widget);
}

static void
unselect_workspace (GtkWidget *widget)
{
  META_SELECT_WORKSPACE (widget)->selected = FALSE;
  gtk_widget_queue_draw (widget);
}

static void meta_select_workspace_class_init (MetaSelectWorkspaceClass *klass,
                                              void                     *data);

static gboolean meta_select_workspace_draw (GtkWidget *widget,
                                            cairo_t   *cr);

GType
meta_select_workspace_get_type (void)
{
  static GType workspace_type = 0;

  if (!workspace_type)
    {
      static const GTypeInfo workspace_info =
      {
        sizeof (MetaSelectWorkspaceClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) meta_select_workspace_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (MetaSelectWorkspace),
        16,             /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };

      workspace_type = g_type_register_static (GTK_TYPE_DRAWING_AREA,
                                               "MetaSelectWorkspace",
                                               &workspace_info,
                                               0);
    }

  return workspace_type;
}

static void
meta_select_workspace_class_init (MetaSelectWorkspaceClass *klass,
                                  void                     *data)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->draw = meta_select_workspace_draw;
}

/**
 * meta_convert_meta_to_wnck() converts a MetaWindow to a
 * WnckWindowDisplayInfo window that is used to build a thumbnail of a
 * workspace.
 **/
static WnckWindowDisplayInfo
meta_convert_meta_to_wnck (MetaWindow *window, MetaScreen *screen)
{
  WnckWindowDisplayInfo wnck_window;
  wnck_window.icon = window->icon;
  wnck_window.mini_icon = window->mini_icon;

  wnck_window.is_active = FALSE;
  if (window == window->display->expected_focus_window)
    wnck_window.is_active = TRUE;

  if (window->frame)
    {
      wnck_window.x = window->frame->rect.x;
      wnck_window.y = window->frame->rect.y;
      wnck_window.width = window->frame->rect.width;
      wnck_window.height = window->frame->rect.height;
    }
  else
    {
      wnck_window.x = window->rect.x;
      wnck_window.y = window->rect.y;
      wnck_window.width = window->rect.width;
      wnck_window.height = window->rect.height;
    }
  return wnck_window;
}

static gboolean
meta_select_workspace_draw (GtkWidget *widget,
                            cairo_t   *cr)
{
  MetaWorkspace *workspace;
  WnckWindowDisplayInfo *windows;
  GtkAllocation allocation;
  int i, n_windows;
  GList *tmp, *list;

  workspace = META_SELECT_WORKSPACE (widget)->workspace;

  list = meta_stack_list_windows (workspace->screen->stack, workspace);
  n_windows = g_list_length (list);
  windows = g_new (WnckWindowDisplayInfo, n_windows);

  tmp = list;
  i = 0;
  while (tmp != NULL)
    {
      MetaWindow *window;
      gboolean ignoreable_sticky;

      window = tmp->data;

      ignoreable_sticky = window->on_all_workspaces &&
                          workspace != workspace->screen->active_workspace;

      if (window->skip_pager ||
          !meta_window_showing_on_its_workspace (window) ||
          window->unmaps_pending ||
          ignoreable_sticky)
        {
          --n_windows;
        }
      else
        {
          windows[i] = meta_convert_meta_to_wnck (window, workspace->screen);
          i++;
        }
      tmp = tmp->next;
    }

  g_list_free (list);

  gtk_widget_get_allocation (widget, &allocation);

  wnck_draw_workspace (widget,
                       cr,
                       SELECT_OUTLINE_WIDTH,
                       SELECT_OUTLINE_WIDTH,
                       allocation.width - SELECT_OUTLINE_WIDTH * 2,
                       allocation.height - SELECT_OUTLINE_WIDTH * 2,
                       workspace->screen->rect.width,
                       workspace->screen->rect.height,
                       NULL,
                       (workspace->screen->active_workspace == workspace),
                       windows,
                       n_windows);

  g_free (windows);

  if (META_SELECT_WORKSPACE (widget)->selected)
    {
      GtkStyleContext *context;
      GdkRGBA color;

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_set_state (context,
                                   gtk_widget_get_state_flags (widget));

      gtk_style_context_lookup_color (context, "color", &color);

      cairo_set_line_width (cr, SELECT_OUTLINE_WIDTH);
      cairo_set_source_rgb (cr, color.red, color.green, color.blue);

      cairo_rectangle (cr,
                       SELECT_OUTLINE_WIDTH / 2.0, SELECT_OUTLINE_WIDTH / 2.0,
                       allocation.width - SELECT_OUTLINE_WIDTH,
                       allocation.height - SELECT_OUTLINE_WIDTH);
      cairo_stroke (cr);
    }

  return TRUE;
}
