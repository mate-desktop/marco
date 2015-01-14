/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter tile-preview marks the area a window will *ehm* snap to */

/*
 * Copyright (C) 2010 Florian Müllner
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>
#include <cairo.h>

#include "tile-preview.h"
#include "core.h"
#include "types.h"
#include "core/screen-private.h"

#define OUTLINE_WIDTH 5  /* frame width in non-composite case */


struct _MetaTilePreview {
  GtkWidget     *preview_window;

#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA       *preview_color;
#else
  GdkColor      *preview_color;
  guchar         preview_alpha;
#endif

  MetaRectangle  tile_rect;

  gboolean       has_alpha: 1;
};

static gboolean
#if GTK_CHECK_VERSION (3, 0, 0)
meta_tile_preview_draw (GtkWidget *widget,
                        cairo_t   *cr,
                        gpointer   user_data)
#else
meta_tile_preview_expose (GtkWidget      *widget,
                          GdkEventExpose *event,
                          gpointer        user_data)
#endif
{
  MetaTilePreview *preview = user_data;
#if !GTK_CHECK_VERSION (3, 0, 0)
  GdkWindow *window;
  cairo_t *cr;

  window = gtk_widget_get_window (widget);
  cr = gdk_cairo_create (window);
#endif

  cairo_set_line_width (cr, 1.0);

  if (preview->has_alpha)
    {
#if GTK_CHECK_VERSION (3, 0, 0)
      GdkRGBA preview_color = *preview->preview_color;
#endif

      /* Fill the preview area with a transparent color */
#if GTK_CHECK_VERSION (3, 0, 0)
      gdk_cairo_set_source_rgba (cr, &preview_color);
#else
      cairo_set_source_rgba (cr,
                             (double)preview->preview_color->red   / 0xFFFF,
                             (double)preview->preview_color->green / 0xFFFF,
                             (double)preview->preview_color->blue  / 0xFFFF,
                             (double)preview->preview_alpha / 0xFF);
#endif

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);

      /* Use the opaque color for the border */
#if GTK_CHECK_VERSION (3, 0, 0)
      gdk_cairo_set_source_rgba (cr, &preview_color);
#else
      gdk_cairo_set_source_color (cr, preview->preview_color);
#endif
    }
  else
    {
#if GTK_CHECK_VERSION (3, 0, 0)
      GdkRGBA white = {1.0, 1.0, 1.0, 1.0};

      gdk_cairo_set_source_rgba (cr, &white);
#else
      GtkStyle *style = gtk_widget_get_style (preview->preview_window);

      gdk_cairo_set_source_color (cr, &style->white);
#endif

      cairo_rectangle (cr,
                       OUTLINE_WIDTH - 0.5, OUTLINE_WIDTH - 0.5,
                       preview->tile_rect.width - 2 * (OUTLINE_WIDTH - 1) - 1,
                       preview->tile_rect.height - 2 * (OUTLINE_WIDTH - 1) - 1);
      cairo_stroke (cr);
    }

  cairo_rectangle (cr,
                   0.5, 0.5,
                   preview->tile_rect.width - 1,
                   preview->tile_rect.height - 1);
  cairo_stroke (cr);

#if !GTK_CHECK_VERSION (3, 0, 0)
  cairo_destroy (cr);
#endif

  return FALSE;
}

static void
on_preview_window_style_set (GtkWidget *widget,
                             GtkStyle  *previous,
                             gpointer   user_data)
{
  MetaTilePreview *preview = user_data;
#if GTK_CHECK_VERSION (3, 0, 0)
  GtkStyleContext *context = gtk_style_context_new ();
  GtkWidgetPath *path = gtk_widget_path_new ();
  guchar alpha = 0xFF;

  gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);
  gtk_style_context_set_path (context, path);

  gtk_style_context_get (context, GTK_STATE_FLAG_SELECTED, "background-color", &preview->preview_color, NULL);
  gtk_style_context_get_style (context, "selection-box-alpha", &alpha, NULL);

  preview->preview_color->alpha = (double)alpha / 0xFF;

  gtk_widget_path_free (path);
  g_object_unref (context);
#else
  GtkStyle *style;

  style = gtk_rc_get_style_by_paths (gtk_widget_get_settings (widget),
                                     "GtkWindow.GtkIconView",
                                     "GtkWindow.GtkIconView",
                                     GTK_TYPE_ICON_VIEW);

  if (style != NULL)
    g_object_ref (style);
  else
    style = gtk_style_new ();

  gtk_style_get (style, GTK_TYPE_ICON_VIEW,
                 "selection-box-color", &preview->preview_color,
                 "selection-box-alpha", &preview->preview_alpha,
                 NULL);
  if (!preview->preview_color)
    {
      GdkColor selection = style->base[GTK_STATE_SELECTED];
      preview->preview_color = gdk_color_copy (&selection);
    }

  g_object_unref (style);
#endif
}

MetaTilePreview *
meta_tile_preview_new (int      screen_number)
{
  MetaTilePreview *preview;
#if !GTK_CHECK_VERSION (3, 0, 0)
  GdkColormap *rgba_colormap;
#endif
  GdkScreen *screen;

  screen = gdk_display_get_screen (gdk_display_get_default (), screen_number);
#if !GTK_CHECK_VERSION (3, 0, 0)
  rgba_colormap = gdk_screen_get_rgba_colormap (screen);
#endif

  preview = g_new (MetaTilePreview, 1);

  preview->preview_window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_window_set_screen (GTK_WINDOW (preview->preview_window), screen);
  gtk_widget_set_app_paintable (preview->preview_window, TRUE);

  preview->preview_color = NULL;
#if !GTK_CHECK_VERSION (3, 0, 0)
  preview->preview_alpha = 0xFF;
#endif

  preview->tile_rect.x = preview->tile_rect.y = 0;
  preview->tile_rect.width = preview->tile_rect.height = 0;

#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_widget_set_visual (preview->preview_window,
                         gdk_screen_get_rgba_visual (screen));
#else
  gtk_widget_set_colormap (preview->preview_window, rgba_colormap);
#endif

  g_signal_connect (preview->preview_window, "style-set",
                    G_CALLBACK (on_preview_window_style_set), preview);

  gtk_widget_realize (preview->preview_window);
#if !GTK_CHECK_VERSION (3, 0, 0)
  gdk_window_set_back_pixmap (gtk_widget_get_window (preview->preview_window),
                              NULL, FALSE);
#endif

#if GTK_CHECK_VERSION (3, 0, 0)
  g_signal_connect (preview->preview_window, "draw",
                    G_CALLBACK (meta_tile_preview_draw), preview);
#else
  g_signal_connect (preview->preview_window, "expose-event",
                    G_CALLBACK (meta_tile_preview_expose), preview);
#endif

  return preview;
}

void
meta_tile_preview_free (MetaTilePreview *preview)
{
  gtk_widget_destroy (preview->preview_window);

  if (preview->preview_color)
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_rgba_free (preview->preview_color);
#else
    gdk_color_free (preview->preview_color);
#endif

  g_free (preview);
}

void
meta_tile_preview_show (MetaTilePreview *preview,
                        MetaRectangle   *tile_rect,
                        MetaScreen      *screen)
{
  GdkWindow *window;
  GdkRectangle old_rect;

  if (gtk_widget_get_visible (preview->preview_window)
      && preview->tile_rect.x == tile_rect->x
      && preview->tile_rect.y == tile_rect->y
      && preview->tile_rect.width == tile_rect->width
      && preview->tile_rect.height == tile_rect->height)
    return; /* nothing to do */

  window = gtk_widget_get_window (preview->preview_window);
#if GTK_CHECK_VERSION (3, 0, 0)
  meta_core_lower_beneath_focus_window (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
#else
  meta_core_lower_beneath_focus_window (gdk_display,
#endif
                                        GDK_WINDOW_XID (window),
                                        gtk_get_current_event_time ());

  old_rect.x = old_rect.y = 0;
  old_rect.width = preview->tile_rect.width;
  old_rect.height = preview->tile_rect.height;

  gdk_window_invalidate_rect (window, &old_rect, FALSE);

  gtk_widget_show (preview->preview_window);

  preview->tile_rect = *tile_rect;

  gdk_window_move_resize (window,
                          preview->tile_rect.x, preview->tile_rect.y,
                          preview->tile_rect.width, preview->tile_rect.height);
#if HAVE_COMPOSITE_EXTENSIONS
  preview->has_alpha = meta_screen_is_cm_selected (screen);
#else
  preview->has_alpha = FALSE;
#endif

  if (!preview->has_alpha)
    {
#if GTK_CHECK_VERSION (3, 0, 0)
      cairo_rectangle_int_t outer_rect, inner_rect;
      cairo_region_t *outer_region, *inner_region;
      GdkRGBA black = {.0, .0, .0, 1.0};

      gdk_window_set_background_rgba (window, &black);
#else
      GdkRectangle outer_rect, inner_rect;
      GdkRegion *outer_region, *inner_region;
      GdkColor black;

      black = gtk_widget_get_style (preview->preview_window)->black;
      gdk_window_set_background (window, &black);
#endif

      outer_rect.x = outer_rect.y = 0;
      outer_rect.width = preview->tile_rect.width;
      outer_rect.height = preview->tile_rect.height;

      inner_rect.x = OUTLINE_WIDTH;
      inner_rect.y = OUTLINE_WIDTH;
      inner_rect.width = outer_rect.width - 2 * OUTLINE_WIDTH;
      inner_rect.height = outer_rect.height - 2 * OUTLINE_WIDTH;

#if GTK_CHECK_VERSION (3, 0, 0)
      outer_region = cairo_region_create_rectangle (&outer_rect);
      inner_region = cairo_region_create_rectangle (&inner_rect);

      cairo_region_subtract (outer_region, inner_region);
      cairo_region_destroy (inner_region);

      gtk_widget_shape_combine_region (preview->preview_window, outer_region);
      cairo_region_destroy (outer_region);
#else
      outer_region = gdk_region_rectangle (&outer_rect);
      inner_region = gdk_region_rectangle (&inner_rect);

      gdk_region_subtract (outer_region, inner_region);
      gdk_region_destroy (inner_region);

      gdk_window_shape_combine_region (window, outer_region, 0, 0);
      gdk_region_destroy (outer_region);
#endif
    } else {
      gdk_window_shape_combine_region (window, NULL, 0, 0);
    }
}

void
meta_tile_preview_hide (MetaTilePreview *preview)
{
  gtk_widget_hide (preview->preview_window);
}
