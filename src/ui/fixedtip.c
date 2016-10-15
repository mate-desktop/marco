/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco fixed tooltip routine */

/*
 * Copyright (C) 2001 Havoc Pennington
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
#include <gtk/gtk.h>
#include "fixedtip.h"
#include "ui.h"

/**
 * The floating rectangle.  This is a GtkWindow, and it contains
 * the "label" widget, below.
 */
static GtkWidget *tip = NULL;

/**
 * The actual text that gets displayed.
 */
static GtkWidget *label = NULL;

static GdkScreen *screen = NULL;

static gboolean
draw_handler (GtkWidget *widget,
              cairo_t   *cr)
{
  GtkStyleContext *context;
  gint width;
  gint height;

  if (widget == NULL)
    return FALSE;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_class (context, "tooltip");
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  return FALSE;
}

void
meta_fixed_tip_show (int screen_number,
                     int root_x, int root_y,
                     const char *markup_text)
{
  gint w;
  gint h;
  gint mon_num;
  GdkRectangle monitor;
  gint screen_right_edge;

  if (tip == NULL)
    {
      GdkVisual *visual;

      tip = gtk_window_new (GTK_WINDOW_POPUP);

      gtk_window_set_type_hint (GTK_WINDOW(tip), GDK_WINDOW_TYPE_HINT_TOOLTIP);
      gtk_style_context_add_class (gtk_widget_get_style_context (tip),
                                   GTK_STYLE_CLASS_TOOLTIP);

      screen = gdk_display_get_screen (gdk_display_get_default (), screen_number);
      visual = gdk_screen_get_rgba_visual (screen);

      gtk_window_set_screen (GTK_WINDOW (tip), screen);

      if (visual != NULL)
        gtk_widget_set_visual (tip, visual);

      gtk_widget_set_app_paintable (tip, TRUE);
      gtk_window_set_resizable (GTK_WINDOW (tip), FALSE);
      g_signal_connect (tip, "draw", G_CALLBACK (draw_handler), NULL);

      label = gtk_label_new (NULL);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
#if GTK_CHECK_VERSION (3, 16, 0)
      gtk_label_set_xalign (GTK_LABEL (label), 0.5);
      gtk_label_set_yalign (GTK_LABEL (label), 0.5);
#else
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
#endif
      gtk_widget_show (label);

      gtk_container_set_border_width (GTK_CONTAINER (tip), 4);
      gtk_container_add (GTK_CONTAINER (tip), label);

      g_signal_connect (tip, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &tip);
    }

  mon_num = gdk_screen_get_monitor_at_point (screen, root_x, root_y);
  gdk_screen_get_monitor_geometry (screen, mon_num, &monitor);
  screen_right_edge = monitor.x + monitor.width;

  gtk_label_set_markup (GTK_LABEL (label), markup_text);

  gtk_window_get_size (GTK_WINDOW (tip), &w, &h);

  if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
      root_x = MAX(0, root_x - w);

  if ((root_x + w) > screen_right_edge)
    root_x -= (root_x + w) - screen_right_edge;

  gtk_window_move (GTK_WINDOW (tip), root_x, root_y);

  gtk_widget_show (tip);
}

void
meta_fixed_tip_hide (void)
{
  if (tip)
    {
      gtk_widget_destroy (tip);
      tip = NULL;
    }
}
