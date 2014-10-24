/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Draw a workspace */

/* This file should not be modified to depend on other files in
 * libwnck or marco, since it's used in both of them
 */

/*
 * Copyright (C) 2002 Red Hat Inc.
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

#include "draw-workspace.h"

#if GTK_CHECK_VERSION (3, 0, 0)
#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-desktop-utils.h>
#endif


static void
get_window_rect (const WnckWindowDisplayInfo *win,
                 int                    screen_width,
                 int                    screen_height,
                 const GdkRectangle    *workspace_rect,
                 GdkRectangle          *rect)
{
  double width_ratio, height_ratio;
  int x, y, width, height;

  width_ratio = (double) workspace_rect->width / (double) screen_width;
  height_ratio = (double) workspace_rect->height / (double) screen_height;

  x = win->x;
  y = win->y;
  width = win->width;
  height = win->height;

  x *= width_ratio;
  y *= height_ratio;
  width *= width_ratio;
  height *= height_ratio;

  x += workspace_rect->x;
  y += workspace_rect->y;

  if (width < 3)
    width = 3;
  if (height < 3)
    height = 3;

  rect->x = x;
  rect->y = y;
  rect->width = width;
  rect->height = height;
}

static void
draw_window (GtkWidget                   *widget,
             #if GTK_CHECK_VERSION(3, 0, 0)
             cairo_t                     *cr,
             #else
             GdkDrawable                 *drawable,
             #endif
             const WnckWindowDisplayInfo *win,
             const GdkRectangle          *winrect,
             GtkStateType                state)
{
  #if !GTK_CHECK_VERSION(3, 0, 0)
  cairo_t *cr;
  #endif

  GdkPixbuf *icon;
  int icon_x, icon_y, icon_w, icon_h;
  gboolean is_active;
#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA color;
  GtkStyleContext *style;
#else
  GdkColor *color;
  GtkStyle *style;
#endif

  is_active = win->is_active;

  #if GTK_CHECK_VERSION(3, 0, 0)
  cairo_save (cr);
  #else
  cr = gdk_cairo_create (drawable);
  #endif

  cairo_rectangle (cr, winrect->x, winrect->y, winrect->width, winrect->height);
  cairo_clip (cr);

#if GTK_CHECK_VERSION (3, 0, 0)
  style = gtk_widget_get_style_context (widget);
  if (is_active)
    mate_desktop_gtk_style_get_light_color (style, state, &color);
  else
    gtk_style_context_get_background_color (style, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);
#else
  style = gtk_widget_get_style (widget);
  if (is_active)
    color = &style->light[state];
  else
    color = &style->bg[state];
  cairo_set_source_rgb (cr,
                        color->red / 65535.,
                        color->green / 65535.,
                        color->blue / 65535.);
#endif

  cairo_rectangle (cr,
                   winrect->x + 1, winrect->y + 1,
                   MAX (0, winrect->width - 2), MAX (0, winrect->height - 2));
  cairo_fill (cr);


  icon = win->icon;

  icon_w = icon_h = 0;

  if (icon)
    {
      icon_w = gdk_pixbuf_get_width (icon);
      icon_h = gdk_pixbuf_get_height (icon);

      /* If the icon is too big, fall back to mini icon.
       * We don't arbitrarily scale the icon, because it's
       * just too slow on my Athlon 850.
       */
      if (icon_w > (winrect->width - 2) ||
          icon_h > (winrect->height - 2))
        {
          icon = win->mini_icon;
          if (icon)
            {
              icon_w = gdk_pixbuf_get_width (icon);
              icon_h = gdk_pixbuf_get_height (icon);

              /* Give up. */
              if (icon_w > (winrect->width - 2) ||
                  icon_h > (winrect->height - 2))
                icon = NULL;
            }
        }
    }

  if (icon)
    {
      icon_x = winrect->x + (winrect->width - icon_w) / 2;
      icon_y = winrect->y + (winrect->height - icon_h) / 2;

      cairo_save (cr);
      gdk_cairo_set_source_pixbuf (cr, icon, icon_x, icon_y);
      cairo_rectangle (cr, icon_x, icon_y, icon_w, icon_h);
      cairo_clip (cr);
      cairo_paint (cr);
      cairo_restore (cr);
    }

#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_style_context_get_color (style, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);
#else
  color = &style->fg[state];

  cairo_set_source_rgb (cr,
                        color->red / 65535.,
                        color->green / 65535.,
                        color->blue / 65535.);
#endif
  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr,
                   winrect->x + 0.5, winrect->y + 0.5,
                   MAX (0, winrect->width - 1), MAX (0, winrect->height - 1));
  cairo_stroke (cr);

  #if GTK_CHECK_VERSION(3, 0, 0)
  cairo_restore(cr);
  #else
  cairo_destroy (cr);
  #endif
}

void
wnck_draw_workspace (GtkWidget                   *widget,
                     #if GTK_CHECK_VERSION(3, 0, 0)
                     cairo_t                     *cr,
                     #else
                     GdkDrawable                 *drawable,
                     #endif
                     int                          x,
                     int                          y,
                     int                          width,
                     int                          height,
                     int                          screen_width,
                     int                          screen_height,
                     GdkPixbuf                   *workspace_background,
                     gboolean                     is_active,
                     const WnckWindowDisplayInfo *windows,
                     int                          n_windows)
{
  int i;
  GdkRectangle workspace_rect;
#if GTK_CHECK_VERSION(3, 0, 0)
  GtkStateFlags state;
  GtkStyleContext *style;
#else
  GtkStateType state;
  cairo_t *cr;
#endif

  workspace_rect.x = x;
  workspace_rect.y = y;
  workspace_rect.width = width;
  workspace_rect.height = height;

  if (is_active)
#if GTK_CHECK_VERSION (3, 0, 0)
    state = GTK_STATE_FLAG_SELECTED;
#else
    state = GTK_STATE_SELECTED;
#endif
  else if (workspace_background)
#if GTK_CHECK_VERSION (3, 0, 0)
    state = GTK_STATE_FLAG_PRELIGHT;
#else
    state = GTK_STATE_PRELIGHT;
#endif
  else
#if GTK_CHECK_VERSION (3, 0, 0)
    state = GTK_STATE_FLAG_NORMAL;
#else
    state = GTK_STATE_NORMAL;
#endif

  #if GTK_CHECK_VERSION(3, 0, 0)
  style = gtk_widget_get_style_context (widget);
  cairo_save (cr);
  #else
  cr = gdk_cairo_create (drawable);
  #endif

  if (workspace_background)
    {
      gdk_cairo_set_source_pixbuf (cr, workspace_background, x, y);
      cairo_paint (cr);
    }
  else
    {
#if GTK_CHECK_VERSION (3, 0, 0)
      GdkRGBA color;

      mate_desktop_gtk_style_get_dark_color (style,state, &color);
      gdk_cairo_set_source_rgba (cr, &color);
#else
      gdk_cairo_set_source_color (cr, &gtk_widget_get_style (widget)->dark[state]);
#endif
      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
    }

  #if !GTK_CHECK_VERSION(3, 0, 0)
  cairo_destroy (cr);
  #endif

  i = 0;
  while (i < n_windows)
    {
      const WnckWindowDisplayInfo *win = &windows[i];
      GdkRectangle winrect;

      get_window_rect (win, screen_width,
                       screen_height, &workspace_rect, &winrect);

      draw_window (widget,
                   #if GTK_CHECK_VERSION(3, 0, 0)
                   cr,
                   #else
                   drawable,
                   #endif
                   win,
                   &winrect,
                   state);

      ++i;
    }

  #if GTK_CHECK_VERSION(3, 0, 0)
  cairo_restore(cr);
  #endif
}
