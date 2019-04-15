/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco window frame manager widget */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2005, 2006 Elijah Newren
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
#include <math.h>
#include "boxes.h"
#include "frames.h"
#include "util.h"
#include "core.h"
#include "menu.h"
#include "fixedtip.h"
#include "theme.h"
#include "prefs.h"
#include "ui.h"

#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif

#include <cairo-xlib.h>

G_DEFINE_TYPE (MetaFrames, meta_frames, GTK_TYPE_INVISIBLE);

#define DEFAULT_INNER_BUTTON_BORDER 3

static void meta_frames_class_init (MetaFramesClass *klass);
static void meta_frames_init       (MetaFrames      *frames);
static void meta_frames_destroy    (GtkWidget       *object);
static void meta_frames_finalize   (GObject         *object);
static void meta_frames_style_set  (GtkWidget       *widget,
                                    GtkStyle        *prev_style);
static void meta_frames_realize    (GtkWidget       *widget);
static void meta_frames_unrealize  (GtkWidget       *widget);

static void meta_frames_update_prelit_control (MetaFrames      *frames,
                                               MetaUIFrame     *frame,
                                               MetaFrameControl control);
static gboolean meta_frames_button_press_event    (GtkWidget           *widget,
                                                   GdkEventButton      *event);
static gboolean meta_frames_button_release_event  (GtkWidget           *widget,
                                                   GdkEventButton      *event);
static gboolean meta_frames_motion_notify_event   (GtkWidget           *widget,
                                                   GdkEventMotion      *event);
static gboolean meta_frames_destroy_event         (GtkWidget           *widget,
                                                   GdkEventAny         *event);
static gboolean meta_frames_draw                  (GtkWidget           *widget,
                                                   cairo_t             *cr);
static gboolean meta_frames_enter_notify_event    (GtkWidget           *widget,
                                                   GdkEventCrossing    *event);
static gboolean meta_frames_leave_notify_event    (GtkWidget           *widget,
                                                   GdkEventCrossing    *event);

static void meta_frames_attach_style (MetaFrames  *frames,
                                      MetaUIFrame *frame);

static void meta_frames_paint_to_drawable (MetaFrames   *frames,
                                           MetaUIFrame  *frame,
                                           cairo_t      *cr);

static void meta_frames_calc_geometry (MetaFrames        *frames,
                                       MetaUIFrame         *frame,
                                       MetaFrameGeometry *fgeom);

static void meta_frames_ensure_layout (MetaFrames      *frames,
                                       MetaUIFrame     *frame);

static MetaUIFrame* meta_frames_lookup_window (MetaFrames *frames,
                                               Window      xwindow);

static void meta_frames_font_changed          (MetaFrames *frames);
static void meta_frames_button_layout_changed (MetaFrames *frames);


static GdkRectangle*    control_rect (MetaFrameControl   control,
                                      MetaFrameGeometry *fgeom);
static MetaFrameControl get_control  (MetaFrames        *frames,
                                      MetaUIFrame       *frame,
                                      int                x,
                                      int                y);
static void clear_tip (MetaFrames *frames);
static void invalidate_all_caches (MetaFrames *frames);
static void invalidate_whole_window (MetaFrames *frames,
                                     MetaUIFrame *frame);

static GObject *
meta_frames_constructor (GType                  gtype,
                         guint                  n_properties,
                         GObjectConstructParam *properties)
{
  GObject *object;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (meta_frames_parent_class);
  object = gobject_class->constructor (gtype, n_properties, properties);

  return object;
}

static void
meta_frames_class_init (MetaFramesClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->constructor = meta_frames_constructor;
  gobject_class->finalize = meta_frames_finalize;
  widget_class->destroy = meta_frames_destroy;
  widget_class->style_set = meta_frames_style_set;
  widget_class->realize = meta_frames_realize;
  widget_class->unrealize = meta_frames_unrealize;

  widget_class->draw = meta_frames_draw;
  widget_class->destroy_event = meta_frames_destroy_event;
  widget_class->button_press_event = meta_frames_button_press_event;
  widget_class->button_release_event = meta_frames_button_release_event;
  widget_class->motion_notify_event = meta_frames_motion_notify_event;
  widget_class->enter_notify_event = meta_frames_enter_notify_event;
  widget_class->leave_notify_event = meta_frames_leave_notify_event;
}

static gint
unsigned_long_equal (gconstpointer v1,
                     gconstpointer v2)
{
  return *((const gulong*) v1) == *((const gulong*) v2);
}

static guint
unsigned_long_hash (gconstpointer v)
{
  gulong val = * (const gulong *) v;

  /* I'm not sure this works so well. */
#if GLIB_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

static void
prefs_changed_callback (MetaPreference pref,
                        void          *data)
{
  switch (pref)
    {
    case META_PREF_TITLEBAR_FONT:
      meta_frames_font_changed (META_FRAMES (data));
      break;
    case META_PREF_BUTTON_LAYOUT:
      meta_frames_button_layout_changed (META_FRAMES (data));
      break;
    default:
      break;
    }
}

static void
meta_frames_init (MetaFrames *frames)
{
  GtkWidgetPath *path;

  frames->text_heights = g_hash_table_new (NULL, NULL);

  frames->frames = g_hash_table_new (unsigned_long_hash, unsigned_long_equal);

  frames->tooltip_timeout = 0;

  frames->expose_delay_count = 0;

  frames->invalidate_cache_timeout_id = 0;
  frames->invalidate_frames = NULL;
  frames->cache = g_hash_table_new (g_direct_hash, g_direct_equal);

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, META_TYPE_FRAMES);
  gtk_widget_path_iter_add_class (path, -1, GTK_STYLE_CLASS_BACKGROUND);
  gtk_widget_path_unref (path);

  meta_prefs_add_listener (prefs_changed_callback, frames);
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;

  listp = data;
  *listp = g_slist_prepend (*listp, value);
}

static void
meta_frames_destroy (GtkWidget *object)
{
  GSList *winlist;
  GSList *tmp;
  MetaFrames *frames;

  frames = META_FRAMES (object);

  clear_tip (frames);

  winlist = NULL;
  g_hash_table_foreach (frames->frames, listify_func, &winlist);

  /* Unmanage all frames */
  for (tmp = winlist; tmp != NULL; tmp = tmp->next)
    {
      MetaUIFrame *frame;

      frame = tmp->data;

      meta_frames_unmanage_window (frames, frame->xwindow);
    }
  g_slist_free (winlist);

  GTK_WIDGET_CLASS (meta_frames_parent_class)->destroy (object);
}

static void
meta_frames_finalize (GObject *object)
{
  MetaFrames *frames;

  frames = META_FRAMES (object);

  meta_prefs_remove_listener (prefs_changed_callback, frames);

  g_hash_table_destroy (frames->text_heights);

  invalidate_all_caches (frames);
  if (frames->invalidate_cache_timeout_id)
    g_source_remove (frames->invalidate_cache_timeout_id);

  g_assert (g_hash_table_size (frames->frames) == 0);
  g_hash_table_destroy (frames->frames);
  g_hash_table_destroy (frames->cache);

  G_OBJECT_CLASS (meta_frames_parent_class)->finalize (object);
}

typedef struct
{
  cairo_rectangle_int_t rect;
  cairo_surface_t *pixmap;
} CachedFramePiece;

typedef struct
{
  /* Caches of the four rendered sides in a MetaFrame.
   * Order: top (titlebar), left, right, bottom.
   */
  CachedFramePiece piece[4];
} CachedPixels;

static CachedPixels *
get_cache (MetaFrames *frames,
           MetaUIFrame *frame)
{
  CachedPixels *pixels;

  pixels = g_hash_table_lookup (frames->cache, frame);

  if (!pixels)
    {
      pixels = g_new0 (CachedPixels, 1);
      g_hash_table_insert (frames->cache, frame, pixels);
    }

  return pixels;
}

static void
invalidate_cache (MetaFrames *frames,
                  MetaUIFrame *frame)
{
  CachedPixels *pixels = get_cache (frames, frame);
  int i;

  for (i = 0; i < 4; i++)
    if (pixels->piece[i].pixmap)
      cairo_surface_destroy (pixels->piece[i].pixmap);

  g_free (pixels);
  g_hash_table_remove (frames->cache, frame);
}

static void
invalidate_all_caches (MetaFrames *frames)
{
  GList *l;

  for (l = frames->invalidate_frames; l; l = l->next)
    {
      MetaUIFrame *frame = l->data;

      invalidate_cache (frames, frame);
    }

  g_list_free (frames->invalidate_frames);
  frames->invalidate_frames = NULL;
}

static gboolean
invalidate_cache_timeout (gpointer data)
{
  MetaFrames *frames = data;

  invalidate_all_caches (frames);
  frames->invalidate_cache_timeout_id = 0;
  return FALSE;
}

static void
queue_recalc_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  invalidate_whole_window (frames, frame);
  meta_core_queue_frame_resize (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                frame->xwindow);
  if (frame->layout)
    {
      /* save title to recreate layout */
      g_free (frame->title);

      frame->title = g_strdup (pango_layout_get_text (frame->layout));

      g_object_unref (G_OBJECT (frame->layout));
      frame->layout = NULL;
    }
}

static void
meta_frames_font_changed (MetaFrames *frames)
{
  if (g_hash_table_size (frames->text_heights) > 0)
    {
      g_hash_table_destroy (frames->text_heights);
      frames->text_heights = g_hash_table_new (NULL, NULL);
    }

  /* Queue a draw/resize on all frames */
  g_hash_table_foreach (frames->frames,
                        queue_recalc_func, frames);

}

static void
queue_draw_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  invalidate_whole_window (frames, frame);
}

static void
meta_frames_button_layout_changed (MetaFrames *frames)
{
  g_hash_table_foreach (frames->frames,
                        queue_draw_func, frames);
}

static void
reattach_style_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  meta_frames_attach_style (frames, frame);
}

static void
meta_frames_style_set  (GtkWidget *widget,
                        GtkStyle  *prev_style)
{
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  meta_frames_font_changed (frames);

  g_hash_table_foreach (frames->frames,
                        reattach_style_func, frames);

  GTK_WIDGET_CLASS (meta_frames_parent_class)->style_set (widget, prev_style);
}

static void
meta_frames_ensure_layout (MetaFrames  *frames,
                           MetaUIFrame *frame)
{
  GtkWidget *widget;
  MetaFrameFlags flags;
  MetaFrameType type;
  MetaFrameStyle *style;

  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET(frames)));

  widget = GTK_WIDGET (frames);

  meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_END);

  style = meta_theme_get_frame_style (meta_theme_get_current (),
                                      type, flags);

  if (style != frame->cache_style)
    {
      if (frame->layout)
        {
          /* save title to recreate layout */
          g_free (frame->title);

          frame->title = g_strdup (pango_layout_get_text (frame->layout));

          g_object_unref (G_OBJECT (frame->layout));
          frame->layout = NULL;
        }
    }

  frame->cache_style = style;

  if (frame->layout == NULL)
    {
      gpointer key, value;
      PangoFontDescription *font_desc;
      double scale;
      int size;

      scale = meta_theme_get_title_scale (meta_theme_get_current (),
                                          type,
                                          flags);

      frame->layout = gtk_widget_create_pango_layout (widget, frame->title);

      pango_layout_set_ellipsize (frame->layout, PANGO_ELLIPSIZE_END);
      pango_layout_set_auto_dir (frame->layout, FALSE);

      pango_layout_set_single_paragraph_mode (frame->layout, TRUE);

      font_desc = meta_gtk_widget_get_font_desc (widget, scale,
                                                 meta_prefs_get_titlebar_font ());

      size = pango_font_description_get_size (font_desc);

      if (g_hash_table_lookup_extended (frames->text_heights,
                                        GINT_TO_POINTER (size),
                                        &key, &value))
        {
          frame->text_height = GPOINTER_TO_INT (value);
        }
      else
        {
          frame->text_height =
            meta_pango_font_desc_get_text_height (font_desc,
                                                  gtk_widget_get_pango_context (widget));

          g_hash_table_replace (frames->text_heights,
                                GINT_TO_POINTER (size),
                                GINT_TO_POINTER (frame->text_height));
        }

      pango_layout_set_font_description (frame->layout,
                                         font_desc);

      pango_font_description_free (font_desc);

      /* Save some RAM */
      g_free (frame->title);
      frame->title = NULL;
    }
}

static void
meta_frames_calc_geometry (MetaFrames        *frames,
                           MetaUIFrame       *frame,
                           MetaFrameGeometry *fgeom)
{
  int width, height, scale;
  MetaFrameFlags flags;
  MetaFrameType type;
  MetaButtonLayout button_layout;

  scale = gdk_window_get_scale_factor (frame->window);

  meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                 META_CORE_GET_CLIENT_WIDTH, &width,
                 META_CORE_GET_CLIENT_HEIGHT, &height,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_END);

  meta_frames_ensure_layout (frames, frame);

  meta_prefs_get_button_layout (&button_layout);

  meta_theme_calc_geometry (meta_theme_get_current (),
                            type,
                            frame->text_height,
                            flags,
                            width / scale, height / scale,
                            &button_layout,
                            fgeom);
}

MetaFrames*
meta_frames_new (void)
{
  GdkScreen *screen;

  screen = gdk_display_get_default_screen (gdk_display_get_default ());

  return g_object_new (META_TYPE_FRAMES,
                       "screen", screen,
                       NULL);
}

/* In order to use a style with a window it has to be attached to that
 * window. Actually, the colormaps just have to match, but since GTK+
 * already takes care of making sure that its cheap to attach a style
 * to multiple windows with the same colormap, we can just go ahead
 * and attach separately for each window.
 */
static void
meta_frames_attach_style (MetaFrames  *frames,
                          MetaUIFrame *frame)
{
  if (frame->style != NULL)
    g_object_unref (frame->style);

  frame->style = g_object_ref (gtk_widget_get_style_context (GTK_WIDGET (frames)));
}

void
meta_frames_manage_window (MetaFrames *frames,
                           Window      xwindow,
                           GdkWindow  *window)
{
  MetaUIFrame *frame;

  g_assert (window);

  frame = g_new (MetaUIFrame, 1);

  frame->window = window;

  gdk_window_set_user_data (frame->window, frames);

  frame->style = NULL;
  meta_frames_attach_style (frames, frame);

  /* Don't set event mask here, it's in frame.c */

  frame->xwindow = xwindow;
  frame->cache_style = NULL;
  frame->layout = NULL;
  frame->text_height = -1;
  frame->title = NULL;
  frame->expose_delayed = FALSE;
  frame->shape_applied = FALSE;
  frame->prelit_control = META_FRAME_CONTROL_NONE;

  meta_core_grab_buttons (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);

  g_hash_table_replace (frames->frames, &frame->xwindow, frame);
}

void
meta_frames_unmanage_window (MetaFrames *frames,
                             Window      xwindow)
{
  MetaUIFrame *frame;

  clear_tip (frames);

  frame = g_hash_table_lookup (frames->frames, &xwindow);

  if (frame)
    {
      /* invalidating all caches ensures the frame
       * is not actually referenced anymore
       */
      invalidate_all_caches (frames);

      /* restore the cursor */
      meta_core_set_screen_cursor (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                   frame->xwindow,
                                   META_CURSOR_DEFAULT);

      gdk_window_set_user_data (frame->window, NULL);

      if (frames->last_motion_frame == frame)
        frames->last_motion_frame = NULL;

      g_hash_table_remove (frames->frames, &frame->xwindow);

      g_object_unref (frame->style);

      gdk_window_destroy (frame->window);

      if (frame->layout)
        g_object_unref (G_OBJECT (frame->layout));

      if (frame->title)
        g_free (frame->title);

      g_free (frame);
    }
  else
    meta_warning ("Frame 0x%lx not managed, can't unmanage\n", xwindow);
}

static void
meta_frames_realize (GtkWidget *widget)
{
  if (GTK_WIDGET_CLASS (meta_frames_parent_class)->realize)
    GTK_WIDGET_CLASS (meta_frames_parent_class)->realize (widget);
}

static void
meta_frames_unrealize (GtkWidget *widget)
{
  if (GTK_WIDGET_CLASS (meta_frames_parent_class)->unrealize)
    GTK_WIDGET_CLASS (meta_frames_parent_class)->unrealize (widget);
}

static MetaUIFrame*
meta_frames_lookup_window (MetaFrames *frames,
                           Window      xwindow)
{
  MetaUIFrame *frame;

  frame = g_hash_table_lookup (frames->frames, &xwindow);

  return frame;
}

void
meta_frames_get_borders (MetaFrames       *frames,
                         Window            xwindow,
                         MetaFrameBorders *borders)
{
  MetaFrameFlags flags;
  MetaUIFrame *frame;
  MetaFrameType type;
  gint scale;

  frame = meta_frames_lookup_window (frames, xwindow);
  scale = gdk_window_get_scale_factor (frame->window);

  if (frame == NULL)
    meta_bug ("No such frame 0x%lx\n", xwindow);

  meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_END);

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  meta_frames_ensure_layout (frames, frame);

  /* We can't get the full geometry, because that depends on
   * the client window size and probably we're being called
   * by the core move/resize code to decide on the client
   * window size
   */
  meta_theme_get_frame_borders (meta_theme_get_current (),
                                type,
                                frame->text_height,
                                flags,
                                borders);

  /* Scale frame geometry to ensure proper frame position */
  borders->visible.top *= scale;
  borders->visible.bottom *= scale;
  borders->visible.left *= scale;
  borders->visible.right *= scale;

  borders->total.top *= scale;
  borders->total.bottom *= scale;
  borders->total.left *= scale;
  borders->total.right *= scale;
}

#ifdef HAVE_SHAPE
static void
apply_cairo_region_to_window (Display        *display,
                              Window          xwindow,
                              cairo_region_t *region,
                              int             op)
{
  int n_rects, i;
  XRectangle *rects;

  n_rects = cairo_region_num_rectangles (region);
  rects = g_new (XRectangle, n_rects);

  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (region, i, &rect);

      rects[i].x = rect.x;
      rects[i].y = rect.y;
      rects[i].width = rect.width;
      rects[i].height = rect.height;
    }

  XShapeCombineRectangles (display, xwindow,
                           ShapeBounding, 0, 0, rects, n_rects,
                           ShapeSet, YXBanded);

  g_free (rects);
}
#endif

/* The client rectangle surrounds client window; it subtracts both
 * the visible and invisible borders from the frame window's size.
 */
static void
get_client_rect (MetaFrameGeometry     *fgeom,
                 int                    window_width,
                 int                    window_height,
                 cairo_rectangle_int_t *rect)
{
  rect->x = fgeom->borders.total.left;
  rect->y = fgeom->borders.total.top;
  rect->width = window_width - fgeom->borders.total.right - rect->x;
  rect->height = window_height - fgeom->borders.total.bottom - rect->y;
}

/* The visible frame rectangle surrounds the visible portion of the
 * frame window; it subtracts only the invisible borders from the frame
 * window's size.
 */
static void
get_visible_frame_rect (MetaFrameGeometry     *fgeom,
                        int                    window_width,
                        int                    window_height,
                        cairo_rectangle_int_t *rect)
{
  rect->x = fgeom->borders.invisible.left;
  rect->y = fgeom->borders.invisible.top;
  rect->width = window_width - fgeom->borders.invisible.right - rect->x;
  rect->height = window_height - fgeom->borders.invisible.bottom - rect->y;
}

static cairo_region_t *
get_visible_region (MetaFrames        *frames,
                    MetaUIFrame       *frame,
                    MetaFrameGeometry *fgeom,
                    int                window_width,
                    int                window_height)
{
  cairo_region_t *corners_region;
  cairo_region_t *visible_region;
  cairo_rectangle_int_t rect;
  cairo_rectangle_int_t frame_rect;
  gint scale;

  corners_region = cairo_region_create ();
  get_visible_frame_rect (fgeom, window_width, window_height, &frame_rect);
  scale = gdk_window_get_scale_factor (frame->window);

  if (fgeom->top_left_corner_rounded_radius != 0)
    {
      const int corner = fgeom->top_left_corner_rounded_radius * scale;
      const float radius = sqrt(corner) + corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x;
          rect.y = frame_rect.y + i;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->top_right_corner_rounded_radius != 0)
    {
      const int corner = fgeom->top_right_corner_rounded_radius * scale;
      const float radius = sqrt(corner) + corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x + frame_rect.width - width;
          rect.y = frame_rect.y + i;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->bottom_left_corner_rounded_radius != 0)
    {
      const int corner = fgeom->bottom_left_corner_rounded_radius * scale;
      const float radius = sqrt(corner) + corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x;
          rect.y = frame_rect.y + frame_rect.height - i - 1;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->bottom_right_corner_rounded_radius != 0)
    {
      const int corner = fgeom->bottom_right_corner_rounded_radius * scale;
      const float radius = sqrt(corner) + corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x + frame_rect.width + width;
          rect.y = frame_rect.y + frame_rect.height - i - 1;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  visible_region = cairo_region_create_rectangle (&frame_rect);
  cairo_region_subtract (visible_region, corners_region);
  cairo_region_destroy (corners_region);

  return visible_region;
}

static cairo_region_t *
get_client_region (MetaFrameGeometry *fgeom,
                   int                window_width,
                   int                window_height)
{
  cairo_rectangle_int_t rect;

  rect.x = fgeom->borders.total.left;
  rect.y = fgeom->borders.total.top;
  rect.width = window_width - fgeom->borders.total.right - rect.x;
  rect.height = window_height - fgeom->borders.total.bottom - rect.y;

  return cairo_region_create_rectangle (&rect);
}

void
meta_frames_apply_shapes (MetaFrames *frames,
                          Window      xwindow,
                          int         new_window_width,
                          int         new_window_height,
                          gboolean    window_has_shape)
{
#ifdef HAVE_SHAPE
  /* Apply shapes as if window had new_window_width, new_window_height */
  MetaUIFrame *frame;
  MetaFrameGeometry fgeom;
  cairo_region_t *window_region;
  Display *display;

  frame = meta_frames_lookup_window (frames, xwindow);
  g_return_if_fail (frame != NULL);

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  meta_frames_calc_geometry (frames, frame, &fgeom);

  if (!(fgeom.top_left_corner_rounded_radius != 0 ||
        fgeom.top_right_corner_rounded_radius != 0 ||
        fgeom.bottom_left_corner_rounded_radius != 0 ||
        fgeom.bottom_right_corner_rounded_radius != 0 ||
        window_has_shape))
    {
      if (frame->shape_applied)
        {
          meta_topic (META_DEBUG_SHAPES,
                      "Unsetting shape mask on frame 0x%lx\n",
                      frame->xwindow);

          XShapeCombineMask (display, frame->xwindow,
                             ShapeBounding, 0, 0, None, ShapeSet);
          frame->shape_applied = FALSE;
        }
      else
        {
          meta_topic (META_DEBUG_SHAPES,
                      "Frame 0x%lx still doesn't need a shape mask\n",
                      frame->xwindow);
        }

      return; /* nothing to do */
    }

  window_region = get_visible_region (frames,
                                      frame,
                                      &fgeom,
                                      new_window_width,
                                      new_window_height);

  if (window_has_shape)
    {
      /* The client window is oclock or something and has a shape
       * mask. To avoid a round trip to get its shape region, we
       * create a fake window that's never mapped, build up our shape
       * on that, then combine. Wasting the window is assumed cheaper
       * than a round trip, but who really knows for sure.
       */
      XSetWindowAttributes attrs;
      Window shape_window;
      Window client_window;
      cairo_region_t *client_region;
      GdkScreen *screen;
      int screen_number;

      meta_topic (META_DEBUG_SHAPES,
                  "Frame 0x%lx needs to incorporate client shape\n",
                  frame->xwindow);

      screen = gtk_widget_get_screen (GTK_WIDGET (frames));
      screen_number = gdk_x11_screen_get_screen_number (screen);

      attrs.override_redirect = True;

      shape_window = XCreateWindow (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                    RootWindow (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), screen_number),
                                    -5000, -5000,
                                    new_window_width,
                                    new_window_height,
                                    0,
                                    CopyFromParent,
                                    CopyFromParent,
                                    (Visual *)CopyFromParent,
                                    CWOverrideRedirect,
                                    &attrs);

      /* Copy the client's shape to the temporary shape_window */
      meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                     META_CORE_GET_CLIENT_XWINDOW, &client_window,
                     META_CORE_GET_END);

      XShapeCombineShape (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), shape_window, ShapeBounding,
                          fgeom.borders.total.left,
                          fgeom.borders.total.top,
                          client_window,
                          ShapeBounding,
                          ShapeSet);

      /* Punch the client area out of the normal frame shape,
       * then union it with the shape_window's existing shape
       */
      client_region = get_client_region (&fgeom,
                                         new_window_width,
                                         new_window_height);

      cairo_region_subtract (window_region, client_region);

      cairo_region_destroy (client_region);

      apply_cairo_region_to_window (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), shape_window,
                                    window_region, ShapeUnion);

      /* Now copy shape_window shape to the real frame */
      XShapeCombineShape (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow, ShapeBounding,
                          0, 0,
                          shape_window,
                          ShapeBounding,
                          ShapeSet);

      XDestroyWindow (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), shape_window);
    }
  else
    {
      /* No shape on the client, so just do simple stuff */

      meta_topic (META_DEBUG_SHAPES,
                  "Frame 0x%lx has shaped corners\n",
                  frame->xwindow);

      apply_cairo_region_to_window (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                                    window_region, ShapeSet);
    }

  frame->shape_applied = TRUE;

  cairo_region_destroy (window_region);
#endif /* HAVE_SHAPE */
}

cairo_region_t *
meta_frames_get_frame_bounds (MetaFrames *frames,
                              Window      xwindow,
                              int         window_width,
                              int         window_height)
{
  MetaUIFrame *frame;
  MetaFrameGeometry fgeom;

  frame = meta_frames_lookup_window (frames, xwindow);
  g_return_val_if_fail (frame != NULL, NULL);

  meta_frames_calc_geometry (frames, frame, &fgeom);

  return get_visible_region (frames,
                             frame,
                             &fgeom,
                             window_width,
                             window_height);
}

void
meta_frames_move_resize_frame (MetaFrames *frames,
                               Window      xwindow,
                               int         x,
                               int         y,
                               int         width,
                               int         height)
{
  MetaUIFrame *frame = meta_frames_lookup_window (frames, xwindow);
  int old_width, old_height;
  gint scale;

  old_width = gdk_window_get_width (frame->window);
  old_height = gdk_window_get_height (frame->window);

  scale = gdk_window_get_scale_factor (frame->window);

  gdk_window_move_resize (frame->window, x / scale, y / scale, width / scale, height / scale);

  if (old_width != width || old_height != height)
    invalidate_whole_window (frames, frame);
}

void
meta_frames_queue_draw (MetaFrames *frames,
                        Window      xwindow)
{
  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  invalidate_whole_window (frames, frame);
}

void
meta_frames_set_title (MetaFrames *frames,
                       Window      xwindow,
                       const char *title)
{
  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  g_assert (frame);

  g_free (frame->title);
  frame->title = g_strdup (title);

  if (frame->layout)
    {
      g_object_unref (frame->layout);
      frame->layout = NULL;
    }

  invalidate_whole_window (frames, frame);
}

void
meta_frames_repaint_frame (MetaFrames *frames,
                           Window      xwindow)
{
  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  g_assert (frame);

  /* repaint everything, so the other frame don't
   * lag behind if they are exposed
   */
  gdk_window_process_all_updates ();
}

static void
show_tip_now (MetaFrames *frames)
{
  const char *tiptext;
  MetaUIFrame *frame;
  int x, y, root_x, root_y;
  Window root, child;
  guint mask;
  MetaFrameControl control;

  frame = frames->last_motion_frame;
  if (frame == NULL)
    return;

  XQueryPointer (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                 frame->xwindow,
                 &root, &child,
                 &root_x, &root_y,
                 &x, &y,
                 &mask);

  control = get_control (frames, frame, x, y);

  tiptext = NULL;
  switch (control)
    {
    case META_FRAME_CONTROL_TITLE:
      break;
    case META_FRAME_CONTROL_DELETE:
      tiptext = _("Close Window");
      break;
    case META_FRAME_CONTROL_MENU:
      tiptext = _("Window Menu");
      break;
    case META_FRAME_CONTROL_APPMENU:
      tiptext = _("Window App Menu");
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      tiptext = _("Minimize Window");
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      tiptext = _("Maximize Window");
      break;
    case META_FRAME_CONTROL_UNMAXIMIZE:
      tiptext = _("Restore Window");
      break;
    case META_FRAME_CONTROL_SHADE:
      tiptext = _("Roll Up Window");
      break;
    case META_FRAME_CONTROL_UNSHADE:
      tiptext = _("Unroll Window");
      break;
    case META_FRAME_CONTROL_ABOVE:
      tiptext = _("Keep Window On Top");
      break;
    case META_FRAME_CONTROL_UNABOVE:
      tiptext = _("Remove Window From Top");
      break;
    case META_FRAME_CONTROL_STICK:
      tiptext = _("Always On Visible Workspace");
      break;
    case META_FRAME_CONTROL_UNSTICK:
      tiptext = _("Put Window On Only One Workspace");
      break;
    case META_FRAME_CONTROL_RESIZE_SE:
      break;
    case META_FRAME_CONTROL_RESIZE_S:
      break;
    case META_FRAME_CONTROL_RESIZE_SW:
      break;
    case META_FRAME_CONTROL_RESIZE_N:
      break;
    case META_FRAME_CONTROL_RESIZE_NE:
      break;
    case META_FRAME_CONTROL_RESIZE_NW:
      break;
    case META_FRAME_CONTROL_RESIZE_W:
      break;
    case META_FRAME_CONTROL_RESIZE_E:
      break;
    case META_FRAME_CONTROL_NONE:
      break;
    case META_FRAME_CONTROL_CLIENT_AREA:
      break;
    }

  if (tiptext)
    {
      MetaFrameGeometry fgeom;
      GdkRectangle *rect;
      int dx, dy, scale;

      meta_frames_calc_geometry (frames, frame, &fgeom);

      rect = control_rect (control, &fgeom);
      scale = gdk_window_get_scale_factor (frame->window);

      /* get conversion delta for root-to-frame coords */
      dx = (root_x - x) / scale;
      dy = (root_y - y) / scale;

      /* Align the tooltip to the button right end if RTL */
      if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
        dx += rect->width;

      meta_fixed_tip_show (rect->x + dx,
                           rect->y + rect->height + 2 + dy,
                           tiptext);
    }
}

static gboolean
tip_timeout_func (gpointer data)
{
  MetaFrames *frames;

  frames = data;

  show_tip_now (frames);

  frames->tooltip_timeout = 0;

  return FALSE;
}

#define TIP_DELAY 450
static void
queue_tip (MetaFrames *frames)
{
  clear_tip (frames);

  frames->tooltip_timeout = g_timeout_add (TIP_DELAY,
                                           tip_timeout_func,
                                           frames);
}

static void
clear_tip (MetaFrames *frames)
{
  if (frames->tooltip_timeout)
    {
      g_source_remove (frames->tooltip_timeout);
      frames->tooltip_timeout = 0;
    }
  meta_fixed_tip_hide ();
}

static void
redraw_control (MetaFrames *frames,
                MetaUIFrame *frame,
                MetaFrameControl control)
{
  MetaFrameGeometry fgeom;
  GdkRectangle *rect;

  meta_frames_calc_geometry (frames, frame, &fgeom);

  rect = control_rect (control, &fgeom);

  gdk_window_invalidate_rect (frame->window, rect, FALSE);
  invalidate_cache (frames, frame);
}

static gboolean
meta_frame_titlebar_event (MetaUIFrame    *frame,
                           GdkEventButton *event,
                           int            action)
{
  MetaFrameFlags flags;

  switch (action)
    {
    case META_ACTION_TITLEBAR_TOGGLE_SHADE:
      {
        meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_SHADE)
          {
            if (flags & META_FRAME_SHADED)
              meta_core_unshade (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                 frame->xwindow,
                                 event->time);
            else
              meta_core_shade (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               frame->xwindow,
                               event->time);
          }
      }
      break;

    case META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE:
      {
        meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
          }
      }
      break;

    case META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE_HORIZONTALLY:
      {
        meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize_horizontally (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
          }
      }
      break;

    case META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE_VERTICALLY:
      {
        meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize_vertically (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
          }
      }
      break;

    case META_ACTION_TITLEBAR_MINIMIZE:
      {
        meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_MINIMIZE)
          {
            meta_core_minimize (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
          }
      }
      break;

    case META_ACTION_TITLEBAR_NONE:
      /* Yaay, a sane user that doesn't use that other weird crap! */
      break;

    case META_ACTION_TITLEBAR_LOWER:
      meta_core_user_lower_and_unfocus (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                        frame->xwindow,
                                        event->time);
      break;

    case META_ACTION_TITLEBAR_MENU:
      meta_core_show_window_menu (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                  frame->xwindow,
                                  event->x_root,
                                  event->y_root,
                                  event->button,
                                  event->time);
      break;

    case META_ACTION_TITLEBAR_LAST:
      break;
    }

  return TRUE;
}

static gboolean
meta_frame_double_click_event (MetaUIFrame    *frame,
                               GdkEventButton *event)
{
  int action = meta_prefs_get_action_double_click_titlebar ();

  return meta_frame_titlebar_event (frame, event, action);
}

static gboolean
meta_frame_middle_click_event (MetaUIFrame    *frame,
                               GdkEventButton *event)
{
  int action = meta_prefs_get_action_middle_click_titlebar();

  return meta_frame_titlebar_event (frame, event, action);
}

static gboolean
meta_frame_right_click_event(MetaUIFrame     *frame,
                             GdkEventButton  *event)
{
  int action = meta_prefs_get_action_right_click_titlebar();

  return meta_frame_titlebar_event (frame, event, action);
}

static gboolean
meta_frames_button_press_event (GtkWidget      *widget,
                                GdkEventButton *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaFrameControl control;

  frames = META_FRAMES (widget);

  /* Remember that the display may have already done something with this event.
   * If so there's probably a GrabOp in effect.
   */

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  control = get_control (frames, frame, event->x, event->y);

  /* focus on click, even if click was on client area */
  if (event->button == 1 &&
      !(control == META_FRAME_CONTROL_MINIMIZE ||
        control == META_FRAME_CONTROL_DELETE ||
        control == META_FRAME_CONTROL_MAXIMIZE))
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing window with frame 0x%lx due to button 1 press\n",
                  frame->xwindow);
      meta_core_user_focus (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                            frame->xwindow,
                            event->time);
    }

  /* don't do the rest of this if on client area */
  if (control == META_FRAME_CONTROL_CLIENT_AREA)
    return FALSE; /* not on the frame, just passed through from client */

  /* We want to shade even if we have a GrabOp, since we'll have a move grab
   * if we double click the titlebar.
   */
  if (control == META_FRAME_CONTROL_TITLE &&
      event->button == 1 &&
      event->type == GDK_2BUTTON_PRESS)
    {
      meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
      return meta_frame_double_click_event (frame, event);
    }

  if (meta_core_get_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ())) !=
      META_GRAB_OP_NONE)
    return FALSE; /* already up to something */

  if ((event->button == 1 &&
      (control == META_FRAME_CONTROL_MINIMIZE ||
       control == META_FRAME_CONTROL_DELETE ||
       control == META_FRAME_CONTROL_SHADE ||
       control == META_FRAME_CONTROL_UNSHADE ||
       control == META_FRAME_CONTROL_ABOVE ||
       control == META_FRAME_CONTROL_UNABOVE ||
       control == META_FRAME_CONTROL_STICK ||
       control == META_FRAME_CONTROL_UNSTICK ||
       control == META_FRAME_CONTROL_MENU)) ||
      (control == META_FRAME_CONTROL_MAXIMIZE ||
       control == META_FRAME_CONTROL_UNMAXIMIZE))
    {
      MetaGrabOp op;

      switch (control)
        {
        case META_FRAME_CONTROL_MINIMIZE:
          op = META_GRAB_OP_CLICKING_MINIMIZE;
          break;
        case META_FRAME_CONTROL_MAXIMIZE:
          op = META_GRAB_OP_CLICKING_MAXIMIZE + event->button - 1;
          op = op > META_GRAB_OP_CLICKING_MAXIMIZE_HORIZONTAL ? META_GRAB_OP_CLICKING_MAXIMIZE : op;
          break;
        case META_FRAME_CONTROL_UNMAXIMIZE:
          op = META_GRAB_OP_CLICKING_UNMAXIMIZE;
          break;
        case META_FRAME_CONTROL_DELETE:
          op = META_GRAB_OP_CLICKING_DELETE;
          break;
        case META_FRAME_CONTROL_MENU:
          op = META_GRAB_OP_CLICKING_MENU;
          break;
        case META_FRAME_CONTROL_APPMENU:
          op = META_GRAB_OP_CLICKING_APPMENU;
          break;
        case META_FRAME_CONTROL_SHADE:
          op = META_GRAB_OP_CLICKING_SHADE;
          break;
        case META_FRAME_CONTROL_UNSHADE:
          op = META_GRAB_OP_CLICKING_UNSHADE;
          break;
        case META_FRAME_CONTROL_ABOVE:
          op = META_GRAB_OP_CLICKING_ABOVE;
          break;
        case META_FRAME_CONTROL_UNABOVE:
          op = META_GRAB_OP_CLICKING_UNABOVE;
          break;
        case META_FRAME_CONTROL_STICK:
          op = META_GRAB_OP_CLICKING_STICK;
          break;
        case META_FRAME_CONTROL_UNSTICK:
          op = META_GRAB_OP_CLICKING_UNSTICK;
          break;
        default:
          g_assert_not_reached ();
          op = META_GRAB_OP_NONE;
        }

      meta_core_begin_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               frame->xwindow,
                               op,
                               TRUE,
                               TRUE,
                               event->button,
                               0,
                               event->time,
                               event->x_root,
                               event->y_root);

      frame->prelit_control = control;
      redraw_control (frames, frame, control);

      if (op == META_GRAB_OP_CLICKING_MENU)
        {
          MetaFrameGeometry fgeom;
          GdkRectangle *rect;
          int dx, dy;

          meta_frames_calc_geometry (frames, frame, &fgeom);

          rect = control_rect (META_FRAME_CONTROL_MENU, &fgeom);

          /* get delta to convert to root coords */
          dx = event->x_root - event->x;
          dy = event->y_root - event->y;

          /* Align to the right end of the menu rectangle if RTL */
          if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
            dx += rect->width;

          meta_core_show_window_menu (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                      frame->xwindow,
                                      rect->x + dx,
                                      rect->y + rect->height + dy,
                                      event->button,
                                      event->time);
        }
    }
  else if (event->button == 1 &&
           (control == META_FRAME_CONTROL_RESIZE_SE ||
            control == META_FRAME_CONTROL_RESIZE_S ||
            control == META_FRAME_CONTROL_RESIZE_SW ||
            control == META_FRAME_CONTROL_RESIZE_NE ||
            control == META_FRAME_CONTROL_RESIZE_N ||
            control == META_FRAME_CONTROL_RESIZE_NW ||
            control == META_FRAME_CONTROL_RESIZE_E ||
            control == META_FRAME_CONTROL_RESIZE_W))
    {
      MetaGrabOp op;

      op = META_GRAB_OP_NONE;

      switch (control)
        {
        case META_FRAME_CONTROL_RESIZE_SE:
          op = META_GRAB_OP_RESIZING_SE;
          break;
        case META_FRAME_CONTROL_RESIZE_S:
          op = META_GRAB_OP_RESIZING_S;
          break;
        case META_FRAME_CONTROL_RESIZE_SW:
          op = META_GRAB_OP_RESIZING_SW;
          break;
        case META_FRAME_CONTROL_RESIZE_NE:
          op = META_GRAB_OP_RESIZING_NE;
          break;
        case META_FRAME_CONTROL_RESIZE_N:
          op = META_GRAB_OP_RESIZING_N;
          break;
        case META_FRAME_CONTROL_RESIZE_NW:
          op = META_GRAB_OP_RESIZING_NW;
          break;
        case META_FRAME_CONTROL_RESIZE_E:
          op = META_GRAB_OP_RESIZING_E;
          break;
        case META_FRAME_CONTROL_RESIZE_W:
          op = META_GRAB_OP_RESIZING_W;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      meta_core_begin_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               frame->xwindow,
                               op,
                               TRUE,
                               TRUE,
                               event->button,
                               0,
                               event->time,
                               event->x_root,
                               event->y_root);
    }
  else if (control == META_FRAME_CONTROL_TITLE &&
           event->button == 1)
    {
      MetaFrameFlags flags;

      meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                     META_CORE_GET_FRAME_FLAGS, &flags,
                     META_CORE_GET_END);

      if (flags & META_FRAME_ALLOWS_MOVE)
        {
          meta_core_begin_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                   frame->xwindow,
                                   META_GRAB_OP_MOVING,
                                   TRUE,
                                   TRUE,
                                   event->button,
                                   0,
                                   event->time,
                                   event->x_root,
                                   event->y_root);
        }
    }
  else if (event->button == 2)
    {
      return meta_frame_middle_click_event (frame, event);
    }
  else if (event->button == 3)
    {
      return meta_frame_right_click_event (frame, event);
    }

  return TRUE;
}

void
meta_frames_notify_menu_hide (MetaFrames *frames)
{
  if (meta_core_get_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ())) ==
      META_GRAB_OP_CLICKING_MENU)
    {
      Window grab_frame;

      grab_frame = meta_core_get_grab_frame (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

      if (grab_frame != None)
        {
          MetaUIFrame *frame;

          frame = meta_frames_lookup_window (frames, grab_frame);

          if (frame)
            {
              redraw_control (frames, frame,
                              META_FRAME_CONTROL_MENU);
              meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), CurrentTime);
            }
        }
    }
}

static gboolean
meta_frames_button_release_event    (GtkWidget           *widget,
                                     GdkEventButton      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaGrabOp op;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  op = meta_core_get_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

  if (op == META_GRAB_OP_NONE)
    return FALSE;

  /* We only handle the releases we handled the presses for (things
   * involving frame controls). Window ops that don't require a
   * frame are handled in the Xlib part of the code, display.c/window.c
   */
  if (frame->xwindow == meta_core_get_grab_frame (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ())) &&
      ((int) event->button) == meta_core_get_grab_button (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ())))
    {
      MetaFrameControl control;

      control = get_control (frames, frame, event->x, event->y);

      switch (op)
        {
        case META_GRAB_OP_CLICKING_MINIMIZE:
          if (control == META_FRAME_CONTROL_MINIMIZE)
            meta_core_minimize (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_MAXIMIZE:
        case META_GRAB_OP_CLICKING_MAXIMIZE_VERTICAL:
        case META_GRAB_OP_CLICKING_MAXIMIZE_HORIZONTAL:
          if (control == META_FRAME_CONTROL_MAXIMIZE)
          {
            /* Focus the window on the maximize */
            meta_core_user_focus (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                            frame->xwindow,
                            event->time);

            if (op == META_GRAB_OP_CLICKING_MAXIMIZE)
               meta_core_maximize (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
            if (op == META_GRAB_OP_CLICKING_MAXIMIZE_VERTICAL)
              meta_core_toggle_maximize_vertically (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
            if (op == META_GRAB_OP_CLICKING_MAXIMIZE_HORIZONTAL)
              meta_core_toggle_maximize_horizontally (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
          }
          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_UNMAXIMIZE:
        case META_GRAB_OP_CLICKING_UNMAXIMIZE_VERTICAL:
        case META_GRAB_OP_CLICKING_UNMAXIMIZE_HORIZONTAL:
          if (control == META_FRAME_CONTROL_UNMAXIMIZE) {
            if (op == META_GRAB_OP_CLICKING_UNMAXIMIZE)
              meta_core_unmaximize (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
            if (op == META_GRAB_OP_CLICKING_UNMAXIMIZE_VERTICAL)
              meta_core_toggle_maximize_vertically (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
            if (op == META_GRAB_OP_CLICKING_UNMAXIMIZE_HORIZONTAL)
              meta_core_toggle_maximize_horizontally (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);
          }

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_DELETE:
          if (control == META_FRAME_CONTROL_DELETE)
            meta_core_delete (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow, event->time);

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_MENU:
        case META_GRAB_OP_CLICKING_APPMENU:
          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_SHADE:
          if (control == META_FRAME_CONTROL_SHADE)
            meta_core_shade (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow, event->time);

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_UNSHADE:
          if (control == META_FRAME_CONTROL_UNSHADE)
            meta_core_unshade (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow, event->time);

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_ABOVE:
          if (control == META_FRAME_CONTROL_ABOVE)
            meta_core_make_above (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_UNABOVE:
          if (control == META_FRAME_CONTROL_UNABOVE)
            meta_core_unmake_above (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_STICK:
          if (control == META_FRAME_CONTROL_STICK)
            meta_core_stick (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        case META_GRAB_OP_CLICKING_UNSTICK:
          if (control == META_FRAME_CONTROL_UNSTICK)
            meta_core_unstick (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow);

          meta_core_end_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), event->time);
          break;

        default:
          break;
        }

      /* Update the prelit control regardless of what button the mouse
       * was released over; needed so that the new button can become
       * prelit so to let the user know that it can now be pressed.
       * :)
       */
      meta_frames_update_prelit_control (frames, frame, control);
    }

  return TRUE;
}

static void
meta_frames_update_prelit_control (MetaFrames      *frames,
                                   MetaUIFrame     *frame,
                                   MetaFrameControl control)
{
  MetaFrameControl old_control;
  MetaCursor cursor;


  meta_verbose ("Updating prelit control from %u to %u\n",
                frame->prelit_control, control);

  cursor = META_CURSOR_DEFAULT;

  switch (control)
    {
    case META_FRAME_CONTROL_CLIENT_AREA:
      break;
    case META_FRAME_CONTROL_NONE:
      break;
    case META_FRAME_CONTROL_TITLE:
      break;
    case META_FRAME_CONTROL_DELETE:
      break;
    case META_FRAME_CONTROL_MENU:
      break;
    case META_FRAME_CONTROL_APPMENU:
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      break;
    case META_FRAME_CONTROL_UNMAXIMIZE:
      break;
    case META_FRAME_CONTROL_SHADE:
      break;
    case META_FRAME_CONTROL_UNSHADE:
      break;
    case META_FRAME_CONTROL_ABOVE:
      break;
    case META_FRAME_CONTROL_UNABOVE:
      break;
    case META_FRAME_CONTROL_STICK:
      break;
    case META_FRAME_CONTROL_UNSTICK:
      break;
    case META_FRAME_CONTROL_RESIZE_SE:
      cursor = META_CURSOR_SE_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_S:
      cursor = META_CURSOR_SOUTH_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_SW:
      cursor = META_CURSOR_SW_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_N:
      cursor = META_CURSOR_NORTH_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_NE:
      cursor = META_CURSOR_NE_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_NW:
      cursor = META_CURSOR_NW_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_W:
      cursor = META_CURSOR_WEST_RESIZE;
      break;
    case META_FRAME_CONTROL_RESIZE_E:
      cursor = META_CURSOR_EAST_RESIZE;
      break;
    }

  /* set/unset the prelight cursor */
  meta_core_set_screen_cursor (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               frame->xwindow,
                               cursor);

  switch (control)
    {
    case META_FRAME_CONTROL_MENU:
    case META_FRAME_CONTROL_APPMENU:
    case META_FRAME_CONTROL_MINIMIZE:
    case META_FRAME_CONTROL_MAXIMIZE:
    case META_FRAME_CONTROL_DELETE:
    case META_FRAME_CONTROL_SHADE:
    case META_FRAME_CONTROL_UNSHADE:
    case META_FRAME_CONTROL_ABOVE:
    case META_FRAME_CONTROL_UNABOVE:
    case META_FRAME_CONTROL_STICK:
    case META_FRAME_CONTROL_UNSTICK:
    case META_FRAME_CONTROL_UNMAXIMIZE:
      /* leave control set */
      break;
    default:
      /* Only prelight buttons */
      control = META_FRAME_CONTROL_NONE;
      break;
    }

  if (control == frame->prelit_control)
    return;

  /* Save the old control so we can unprelight it */
  old_control = frame->prelit_control;

  frame->prelit_control = control;

  redraw_control (frames, frame, old_control);
  redraw_control (frames, frame, control);
}

static gboolean
meta_frames_motion_notify_event     (GtkWidget           *widget,
                                     GdkEventMotion      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaGrabOp grab_op;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  frames->last_motion_frame = frame;

  grab_op = meta_core_get_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

  switch (grab_op)
    {
    case META_GRAB_OP_CLICKING_MENU:
    case META_GRAB_OP_CLICKING_APPMENU:
    case META_GRAB_OP_CLICKING_DELETE:
    case META_GRAB_OP_CLICKING_MINIMIZE:
    case META_GRAB_OP_CLICKING_MAXIMIZE:
    case META_GRAB_OP_CLICKING_MAXIMIZE_VERTICAL:
    case META_GRAB_OP_CLICKING_MAXIMIZE_HORIZONTAL:
    case META_GRAB_OP_CLICKING_UNMAXIMIZE:
    case META_GRAB_OP_CLICKING_UNMAXIMIZE_VERTICAL:
    case META_GRAB_OP_CLICKING_UNMAXIMIZE_HORIZONTAL:
    case META_GRAB_OP_CLICKING_SHADE:
    case META_GRAB_OP_CLICKING_UNSHADE:
    case META_GRAB_OP_CLICKING_ABOVE:
    case META_GRAB_OP_CLICKING_UNABOVE:
    case META_GRAB_OP_CLICKING_STICK:
    case META_GRAB_OP_CLICKING_UNSTICK:
      {
        MetaFrameControl control;
        int x, y, scale;

        gdk_window_get_device_position (frame->window, event->device,
                                        &x, &y, NULL);
        scale = gdk_window_get_scale_factor (frame->window);
        x *= scale;
        y *= scale;

        /* Control is set to none unless it matches
         * the current grab
         */
        control = get_control (frames, frame, x, y);
        if (! ((control == META_FRAME_CONTROL_MENU &&
                grab_op == META_GRAB_OP_CLICKING_MENU) ||
               (control == META_FRAME_CONTROL_APPMENU &&
                grab_op == META_GRAB_OP_CLICKING_APPMENU) ||
               (control == META_FRAME_CONTROL_DELETE &&
                grab_op == META_GRAB_OP_CLICKING_DELETE) ||
               (control == META_FRAME_CONTROL_MINIMIZE &&
                grab_op == META_GRAB_OP_CLICKING_MINIMIZE) ||
               ((control == META_FRAME_CONTROL_MAXIMIZE ||
                 control == META_FRAME_CONTROL_UNMAXIMIZE) &&
                (grab_op == META_GRAB_OP_CLICKING_MAXIMIZE ||
                 grab_op == META_GRAB_OP_CLICKING_MAXIMIZE_VERTICAL ||
                 grab_op == META_GRAB_OP_CLICKING_MAXIMIZE_HORIZONTAL ||
                 grab_op == META_GRAB_OP_CLICKING_UNMAXIMIZE ||
                 grab_op == META_GRAB_OP_CLICKING_UNMAXIMIZE_VERTICAL ||
                 grab_op == META_GRAB_OP_CLICKING_UNMAXIMIZE_HORIZONTAL)) ||
               (control == META_FRAME_CONTROL_SHADE &&
                grab_op == META_GRAB_OP_CLICKING_SHADE) ||
               (control == META_FRAME_CONTROL_UNSHADE &&
                grab_op == META_GRAB_OP_CLICKING_UNSHADE) ||
               (control == META_FRAME_CONTROL_ABOVE &&
                grab_op == META_GRAB_OP_CLICKING_ABOVE) ||
               (control == META_FRAME_CONTROL_UNABOVE &&
                grab_op == META_GRAB_OP_CLICKING_UNABOVE) ||
               (control == META_FRAME_CONTROL_STICK &&
                grab_op == META_GRAB_OP_CLICKING_STICK) ||
               (control == META_FRAME_CONTROL_UNSTICK &&
                grab_op == META_GRAB_OP_CLICKING_UNSTICK)))
           control = META_FRAME_CONTROL_NONE;

        /* Update prelit control and cursor */
        meta_frames_update_prelit_control (frames, frame, control);

        /* No tooltip while in the process of clicking */
      }
      break;
    case META_GRAB_OP_NONE:
      {
        MetaFrameControl control;
        int x, y, scale;

        gdk_window_get_device_position (frame->window, event->device,
                                        &x, &y, NULL);
        scale = gdk_window_get_scale_factor (frame->window);
        x *= scale;
        y *= scale;

        control = get_control (frames, frame, x, y);

        /* Update prelit control and cursor */
        meta_frames_update_prelit_control (frames, frame, control);

        queue_tip (frames);
      }
      break;

    default:
      break;
    }

  return TRUE;
}

static gboolean
meta_frames_destroy_event           (GtkWidget           *widget,
                                     GdkEventAny         *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  return TRUE;
}

static void
setup_bg_cr (cairo_t *cr, GdkWindow *window, int x_offset, int y_offset)
{
  GdkWindow *parent = gdk_window_get_parent (window);
  cairo_pattern_t *bg_pattern;

  bg_pattern = gdk_window_get_background_pattern (window);
  if (bg_pattern == NULL && parent)
    {
      gint window_x, window_y;

      gdk_window_get_position (window, &window_x, &window_y);
      setup_bg_cr (cr, parent, x_offset + window_x, y_offset + window_y);
    }
  else if (bg_pattern)
    {
      cairo_translate (cr, - x_offset, - y_offset);
      cairo_set_source (cr, bg_pattern);
      cairo_translate (cr, x_offset, y_offset);
    }
}

/* Returns a pixmap with a piece of the windows frame painted on it.
*/
static cairo_surface_t *
generate_pixmap (MetaFrames            *frames,
                 MetaUIFrame           *frame,
                 cairo_rectangle_int_t *rect)
{
  cairo_surface_t *result;
  cairo_t *cr;

  /* do not create a pixmap for nonexisting areas */
  if (rect->width <= 0 || rect->height <= 0)
    return NULL;

  result = gdk_window_create_similar_surface (frame->window,
                                              CAIRO_CONTENT_COLOR,
                                              rect->width, rect->height);

  cr = cairo_create (result);
  cairo_translate (cr, -rect->x, -rect->y);

  setup_bg_cr (cr, frame->window, 0, 0);
  cairo_paint (cr);

  meta_frames_paint_to_drawable (frames, frame, cr);

  cairo_destroy (cr);

  return result;
}

static void
populate_cache (MetaFrames *frames,
                MetaUIFrame *frame)
{
  MetaFrameBorders borders;
  int width, height;
  int frame_width, frame_height, screen_width, screen_height;
  gint scale;
  CachedPixels *pixels;
  MetaFrameType frame_type;
  MetaFrameFlags frame_flags;
  int i;

  meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                 META_CORE_GET_FRAME_WIDTH, &frame_width,
                 META_CORE_GET_FRAME_HEIGHT, &frame_height,
                 META_CORE_GET_SCREEN_WIDTH, &screen_width,
                 META_CORE_GET_SCREEN_HEIGHT, &screen_height,
                 META_CORE_GET_CLIENT_WIDTH, &width,
                 META_CORE_GET_CLIENT_HEIGHT, &height,
                 META_CORE_GET_FRAME_TYPE, &frame_type,
                 META_CORE_GET_FRAME_FLAGS, &frame_flags,
                 META_CORE_GET_END);

  /* don't cache extremely large windows */
  if (frame_width > 2 * screen_width ||
      frame_height > 2 * screen_height)
    {
      return;
    }

  meta_theme_get_frame_borders (meta_theme_get_current (),
                                frame_type,
                                frame->text_height,
                                frame_flags,
                                &borders);

  pixels = get_cache (frames, frame);
  scale = gdk_window_get_scale_factor (frame->window);

  /* Setup the rectangles for the four visible frame borders. First top, then
   * left, right and bottom. Top and bottom extend to the invisible borders
   * while left and right snugly fit in between:
   * -----
   * | |
   * -----
   */

  /* width and height refer to the client window's
   * size without any border added. */

  /* top */
  pixels->piece[0].rect.x = borders.invisible.left / scale;
  pixels->piece[0].rect.y = borders.invisible.top / scale;
  pixels->piece[0].rect.width = (width + borders.visible.left + borders.visible.right) * scale;
  pixels->piece[0].rect.height = borders.visible.top * scale;

  /* left */
  pixels->piece[1].rect.x = borders.invisible.left / scale;
  pixels->piece[1].rect.y = borders.total.top / scale;
  pixels->piece[1].rect.width = borders.visible.left * scale;
  pixels->piece[1].rect.height = height * scale;

  /* right */
  pixels->piece[2].rect.x = (borders.total.left + width) / scale;
  pixels->piece[2].rect.y = borders.total.top / scale;
  pixels->piece[2].rect.width = borders.visible.right * scale;
  pixels->piece[2].rect.height = height * scale;

  /* bottom */
  pixels->piece[3].rect.x = borders.invisible.left / scale;
  pixels->piece[3].rect.y = (borders.total.top + height) / scale;
  pixels->piece[3].rect.width = (width + borders.visible.left + borders.visible.right) * scale;
  pixels->piece[3].rect.height = borders.visible.bottom * scale;

  for (i = 0; i < 4; i++)
    {
      CachedFramePiece *piece = &pixels->piece[i];
      if (!piece->pixmap)
        piece->pixmap = generate_pixmap (frames, frame, &piece->rect);
    }

  if (frames->invalidate_cache_timeout_id)
    g_source_remove (frames->invalidate_cache_timeout_id);

  frames->invalidate_cache_timeout_id = g_timeout_add (1000, invalidate_cache_timeout, frames);

  if (!g_list_find (frames->invalidate_frames, frame))
    frames->invalidate_frames =
      g_list_prepend (frames->invalidate_frames, frame);
}

static void
clip_to_screen (cairo_region_t *region, MetaUIFrame *frame)
{
  GdkRectangle frame_area;
  GdkRectangle screen_area = { 0, 0, 0, 0 };
  cairo_region_t *tmp_region;

  /* Chop off stuff outside the screen; this optimization
   * is crucial to handle huge client windows,
   * like "xterm -geometry 1000x1000"
   */
  meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                 META_CORE_GET_FRAME_X, &frame_area.x,
                 META_CORE_GET_FRAME_Y, &frame_area.y,
                 META_CORE_GET_FRAME_WIDTH, &frame_area.width,
                 META_CORE_GET_FRAME_HEIGHT, &frame_area.height,
                 META_CORE_GET_SCREEN_WIDTH, &screen_area.width,
                 META_CORE_GET_SCREEN_HEIGHT, &screen_area.height,
                 META_CORE_GET_END);

  cairo_region_translate (region, frame_area.x, frame_area.y);

  tmp_region = cairo_region_create_rectangle (&frame_area);
  cairo_region_intersect (region, tmp_region);
  cairo_region_destroy (tmp_region);

  cairo_region_translate (region, - frame_area.x, - frame_area.y);
}

static void
cached_pixels_draw (CachedPixels   *pixels,
                    cairo_t        *cr,
                    cairo_region_t *region)
{
  cairo_region_t *region_piece;
  int i;

  for (i = 0; i < 4; i++)
    {
      CachedFramePiece *piece;
      piece = &pixels->piece[i];

      if (piece->pixmap)
        {
          cairo_set_source_surface (cr, piece->pixmap,
                                    piece->rect.x, piece->rect.y);
          cairo_paint (cr);

          region_piece = cairo_region_create_rectangle (&piece->rect);
          cairo_region_subtract (region, region_piece);
          cairo_region_destroy (region_piece);
        }
    }
}

static void
subtract_client_area (cairo_region_t *region, MetaUIFrame *frame)
{
  GdkRectangle area;
  MetaFrameFlags flags;
  MetaFrameType type;
  MetaFrameBorders borders;
  cairo_region_t *tmp_region;
  Display *display;
  gint scale;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  scale = gdk_window_get_scale_factor (frame->window);

  meta_core_get (display, frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_CLIENT_WIDTH, &area.width,
                 META_CORE_GET_CLIENT_HEIGHT, &area.height,
                 META_CORE_GET_END);

  meta_theme_get_frame_borders (meta_theme_get_current (),
                                type, frame->text_height, flags,
                                &borders);

  area.width /= scale;
  area.height /= scale;
  area.x = borders.total.left / scale;
  area.y = borders.total.top / scale;

  tmp_region = cairo_region_create_rectangle (&area);
  cairo_region_subtract (region, tmp_region);
  cairo_region_destroy (tmp_region);
}

static MetaUIFrame *
find_frame_to_draw (MetaFrames *frames,
                    cairo_t    *cr)
{
  GHashTableIter iter;
  MetaUIFrame *frame;

  g_hash_table_iter_init (&iter, frames->frames);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &frame))
    if (gtk_cairo_should_draw_window (cr, frame->window))
      return frame;

  return NULL;
}

static gboolean
meta_frames_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  CachedPixels *pixels;
  cairo_region_t *region;
  cairo_rectangle_int_t clip;
  int i, n_areas;

  frames = META_FRAMES (widget);
  gdk_cairo_get_clip_rectangle (cr, &clip);

  frame = find_frame_to_draw (frames, cr);

  if (frame == NULL)
    return FALSE;

  if (frames->expose_delay_count > 0)
    {
      /* Redraw this entire frame later */
      frame->expose_delayed = TRUE;
      return TRUE;
    }

  populate_cache (frames, frame);

  region = cairo_region_create_rectangle (&clip);

  pixels = get_cache (frames, frame);

  cached_pixels_draw (pixels, cr, region);

  clip_to_screen (region, frame);
  subtract_client_area (region, frame);

  n_areas = cairo_region_num_rectangles (region);

  for (i = 0; i < n_areas; i++)
    {
      cairo_rectangle_int_t area;

      cairo_region_get_rectangle (region, i, &area);

      cairo_save (cr);

      cairo_rectangle (cr, area.x, area.y, area.width, area.height);
      cairo_clip (cr);

      cairo_push_group (cr);

      meta_frames_paint_to_drawable (frames, frame, cr);

      cairo_pop_group_to_source (cr);
      cairo_paint (cr);

      cairo_restore (cr);
    }

  cairo_region_destroy (region);

  return TRUE;
}

/* How far off the screen edge the window decorations should
 * be drawn. Used only in meta_frames_paint_to_drawable, below.
 */
#define DECORATING_BORDER 100

static void
meta_frames_paint_to_drawable (MetaFrames   *frames,
                               MetaUIFrame  *frame,
                               cairo_t      *cr)
{
  MetaFrameFlags flags;
  MetaFrameType type;
  GdkPixbuf *mini_icon;
  GdkPixbuf *icon;
  int w, h, scale;
  MetaButtonState button_states[META_BUTTON_TYPE_LAST];
  Window grab_frame;
  int i;
  MetaButtonLayout button_layout;
  MetaGrabOp grab_op;

  for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
    button_states[i] = META_BUTTON_STATE_NORMAL;

  grab_frame = meta_core_get_grab_frame (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  grab_op = meta_core_get_grab_op (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  if (grab_frame != frame->xwindow)
    grab_op = META_GRAB_OP_NONE;

  /* Set prelight state */
  switch (frame->prelit_control)
    {
    case META_FRAME_CONTROL_MENU:
      if (grab_op == META_GRAB_OP_CLICKING_MENU)
        button_states[META_BUTTON_TYPE_MENU] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_MENU] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_APPMENU:
      if (grab_op == META_GRAB_OP_CLICKING_MENU)
        button_states[META_BUTTON_TYPE_APPMENU] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_APPMENU] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      if (grab_op == META_GRAB_OP_CLICKING_MINIMIZE)
        button_states[META_BUTTON_TYPE_MINIMIZE] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_MINIMIZE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      if (grab_op == META_GRAB_OP_CLICKING_MAXIMIZE || grab_op == META_GRAB_OP_CLICKING_MAXIMIZE_VERTICAL ||
	  grab_op == META_GRAB_OP_CLICKING_MAXIMIZE_HORIZONTAL)
        button_states[META_BUTTON_TYPE_MAXIMIZE] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_MAXIMIZE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_UNMAXIMIZE:
      if (grab_op == META_GRAB_OP_CLICKING_UNMAXIMIZE || grab_op == META_GRAB_OP_CLICKING_UNMAXIMIZE_VERTICAL ||
          grab_op == META_GRAB_OP_CLICKING_UNMAXIMIZE_HORIZONTAL)
        button_states[META_BUTTON_TYPE_MAXIMIZE] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_MAXIMIZE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_SHADE:
      if (grab_op == META_GRAB_OP_CLICKING_SHADE)
        button_states[META_BUTTON_TYPE_SHADE] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_SHADE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_UNSHADE:
      if (grab_op == META_GRAB_OP_CLICKING_UNSHADE)
        button_states[META_BUTTON_TYPE_UNSHADE] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_UNSHADE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_ABOVE:
      if (grab_op == META_GRAB_OP_CLICKING_ABOVE)
        button_states[META_BUTTON_TYPE_ABOVE] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_ABOVE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_UNABOVE:
      if (grab_op == META_GRAB_OP_CLICKING_UNABOVE)
        button_states[META_BUTTON_TYPE_UNABOVE] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_UNABOVE] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_STICK:
      if (grab_op == META_GRAB_OP_CLICKING_STICK)
        button_states[META_BUTTON_TYPE_STICK] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_STICK] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_UNSTICK:
      if (grab_op == META_GRAB_OP_CLICKING_UNSTICK)
        button_states[META_BUTTON_TYPE_UNSTICK] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_UNSTICK] = META_BUTTON_STATE_PRELIGHT;
      break;
    case META_FRAME_CONTROL_DELETE:
      if (grab_op == META_GRAB_OP_CLICKING_DELETE)
        button_states[META_BUTTON_TYPE_CLOSE] = META_BUTTON_STATE_PRESSED;
      else
        button_states[META_BUTTON_TYPE_CLOSE] = META_BUTTON_STATE_PRELIGHT;
      break;
    default:
      break;
    }

  meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_MINI_ICON, &mini_icon,
                 META_CORE_GET_ICON, &icon,
                 META_CORE_GET_CLIENT_WIDTH, &w,
                 META_CORE_GET_CLIENT_HEIGHT, &h,
                 META_CORE_GET_END);

  meta_frames_ensure_layout (frames, frame);

  meta_prefs_get_button_layout (&button_layout);

  scale = gdk_window_get_scale_factor (frame->window);
  meta_theme_draw_frame_with_style (meta_theme_get_current (),
                                    frame->style,
                                    cr,
                                    type,
                                    flags,
                                    w / scale, h / scale,
                                    frame->layout,
                                    frame->text_height,
                                    &button_layout,
                                    button_states,
                                    mini_icon, icon);
}

static gboolean
meta_frames_enter_notify_event      (GtkWidget           *widget,
                                     GdkEventCrossing    *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaFrameControl control;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  control = get_control (frames, frame, event->x, event->y);
  meta_frames_update_prelit_control (frames, frame, control);

  return TRUE;
}

static gboolean
meta_frames_leave_notify_event      (GtkWidget           *widget,
                                     GdkEventCrossing    *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  meta_frames_update_prelit_control (frames, frame, META_FRAME_CONTROL_NONE);

  clear_tip (frames);

  return TRUE;
}

static GdkRectangle*
control_rect (MetaFrameControl control,
              MetaFrameGeometry *fgeom)
{
  GdkRectangle *rect;

  rect = NULL;
  switch (control)
    {
    case META_FRAME_CONTROL_TITLE:
      rect = &fgeom->title_rect;
      break;
    case META_FRAME_CONTROL_DELETE:
      rect = &fgeom->close_rect.visible;
      break;
    case META_FRAME_CONTROL_MENU:
      rect = &fgeom->menu_rect.visible;
      break;
    case META_FRAME_CONTROL_APPMENU:
      rect = &fgeom->appmenu_rect.visible;
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      rect = &fgeom->min_rect.visible;
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
    case META_FRAME_CONTROL_UNMAXIMIZE:
      rect = &fgeom->max_rect.visible;
      break;
    case META_FRAME_CONTROL_SHADE:
      rect = &fgeom->shade_rect.visible;
      break;
    case META_FRAME_CONTROL_UNSHADE:
      rect = &fgeom->unshade_rect.visible;
      break;
    case META_FRAME_CONTROL_ABOVE:
      rect = &fgeom->above_rect.visible;
      break;
    case META_FRAME_CONTROL_UNABOVE:
      rect = &fgeom->unabove_rect.visible;
      break;
    case META_FRAME_CONTROL_STICK:
      rect = &fgeom->stick_rect.visible;
      break;
    case META_FRAME_CONTROL_UNSTICK:
      rect = &fgeom->unstick_rect.visible;
      break;
    case META_FRAME_CONTROL_RESIZE_SE:
      break;
    case META_FRAME_CONTROL_RESIZE_S:
      break;
    case META_FRAME_CONTROL_RESIZE_SW:
      break;
    case META_FRAME_CONTROL_RESIZE_N:
      break;
    case META_FRAME_CONTROL_RESIZE_NE:
      break;
    case META_FRAME_CONTROL_RESIZE_NW:
      break;
    case META_FRAME_CONTROL_RESIZE_W:
      break;
    case META_FRAME_CONTROL_RESIZE_E:
      break;
    case META_FRAME_CONTROL_NONE:
      break;
    case META_FRAME_CONTROL_CLIENT_AREA:
      break;
    }

  return rect;
}

#define RESIZE_EXTENDS 15
#define TOP_RESIZE_HEIGHT 4
static MetaFrameControl
get_control (MetaFrames *frames,
             MetaUIFrame *frame,
             int x, int y)
{
  MetaFrameGeometry fgeom;
  MetaFrameFlags flags;
  gboolean has_vert, has_horiz;
  GdkRectangle client;
  gint scale;

  meta_frames_calc_geometry (frames, frame, &fgeom);

  scale = gdk_window_get_scale_factor (frame->window);
  x /= scale;
  y /= scale;

  get_client_rect (&fgeom, fgeom.width, fgeom.height, &client);

  if (POINT_IN_RECT (x, y, client))
    return META_FRAME_CONTROL_CLIENT_AREA;

  if (POINT_IN_RECT (x, y, fgeom.close_rect.clickable))
    return META_FRAME_CONTROL_DELETE;

  if (POINT_IN_RECT (x, y, fgeom.min_rect.clickable))
    return META_FRAME_CONTROL_MINIMIZE;

  if (POINT_IN_RECT (x, y, fgeom.menu_rect.clickable))
    return META_FRAME_CONTROL_MENU;

  if (POINT_IN_RECT (x, y, fgeom.appmenu_rect.clickable))
    return META_FRAME_CONTROL_APPMENU;

  meta_core_get (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_END);

  has_vert = (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE) != 0;
  has_horiz = (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE) != 0;

  if (POINT_IN_RECT (x, y, fgeom.title_rect))
    {
      if (has_vert && y <= TOP_RESIZE_HEIGHT * scale)
        return META_FRAME_CONTROL_RESIZE_N;
      else
        return META_FRAME_CONTROL_TITLE;
    }

  if (POINT_IN_RECT (x, y, fgeom.max_rect.clickable))
    {
      if (flags & META_FRAME_MAXIMIZED)
        return META_FRAME_CONTROL_UNMAXIMIZE;
      else
        return META_FRAME_CONTROL_MAXIMIZE;
    }

  if (POINT_IN_RECT (x, y, fgeom.shade_rect.clickable))
    {
      return META_FRAME_CONTROL_SHADE;
    }

  if (POINT_IN_RECT (x, y, fgeom.unshade_rect.clickable))
    {
      return META_FRAME_CONTROL_UNSHADE;
    }

  if (POINT_IN_RECT (x, y, fgeom.above_rect.clickable))
    {
      return META_FRAME_CONTROL_ABOVE;
    }

  if (POINT_IN_RECT (x, y, fgeom.unabove_rect.clickable))
    {
      return META_FRAME_CONTROL_UNABOVE;
    }

  if (POINT_IN_RECT (x, y, fgeom.stick_rect.clickable))
    {
      return META_FRAME_CONTROL_STICK;
    }

  if (POINT_IN_RECT (x, y, fgeom.unstick_rect.clickable))
    {
      return META_FRAME_CONTROL_UNSTICK;
    }

  /* South resize always has priority over north resize,
   * in case of overlap.
   */

  if (y >= (fgeom.height - fgeom.borders.total.bottom - RESIZE_EXTENDS) &&
      x >= (fgeom.width - fgeom.borders.total.right - RESIZE_EXTENDS))
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_SE;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }
  else if (y >= (fgeom.height - fgeom.borders.total.bottom - RESIZE_EXTENDS) &&
           x <= (fgeom.borders.total.left + RESIZE_EXTENDS))
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_SW;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (y < (fgeom.borders.invisible.top + RESIZE_EXTENDS) &&
           x <= (fgeom.borders.total.left + RESIZE_EXTENDS))
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_NW;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_N;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (y < (fgeom.borders.invisible.top + RESIZE_EXTENDS) &&
           x >= (fgeom.width - fgeom.borders.total.right - RESIZE_EXTENDS))
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_NE;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_N;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }
  else if (y <= fgeom.borders.invisible.top + TOP_RESIZE_HEIGHT * scale)
    {
      if (has_vert)
        return META_FRAME_CONTROL_RESIZE_N;
    }
  else if (y >= (fgeom.height - fgeom.borders.total.bottom - RESIZE_EXTENDS))
    {
      if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
    }
  else if (x <= fgeom.borders.total.left + RESIZE_EXTENDS)
    {
      if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (x >= (fgeom.width - fgeom.borders.total.right - RESIZE_EXTENDS))
    {
      if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }

  return META_FRAME_CONTROL_NONE;
}

void
meta_frames_push_delay_exposes (MetaFrames *frames)
{
  if (frames->expose_delay_count == 0)
    {
      /* Make sure we've repainted things */
      gdk_window_process_all_updates ();
      XFlush (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
    }

  frames->expose_delay_count += 1;
}

static void
queue_pending_exposes_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  if (frame->expose_delayed)
    {
      invalidate_whole_window (frames, frame);
      frame->expose_delayed = FALSE;
    }
}

void
meta_frames_pop_delay_exposes  (MetaFrames *frames)
{
  g_return_if_fail (frames->expose_delay_count > 0);

  frames->expose_delay_count -= 1;

  if (frames->expose_delay_count == 0)
    {
      g_hash_table_foreach (frames->frames,
                            queue_pending_exposes_func,
                            frames);
    }
}

static void
invalidate_whole_window (MetaFrames *frames,
                         MetaUIFrame *frame)
{
  gdk_window_invalidate_rect (frame->window, NULL, FALSE);
  invalidate_cache (frames, frame);
}
