/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco gradient test program */

/*
 * Copyright (C) 2002 Havoc Pennington
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
 * 02110-1301, USA.  */

#include "gradient.h"
#include <gtk/gtk.h>

typedef void (* RenderGradientFunc) (
#if !GTK_CHECK_VERSION (3, 0, 0)
                                     GdkDrawable *drawable,
#endif
                                     cairo_t     *cr,
                                     int          width,
                                     int          height);

static void
#if GTK_CHECK_VERSION (3, 0, 0)
draw_checkerboard (cairo_t *cr,
#else
draw_checkerboard (GdkDrawable *drawable,
#endif
                   int          width,
                   int          height)
{
  gint i, j, xcount, ycount;
#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA color1, color2;
#else
  GdkColor color1, color2;
  cairo_t *cr;
#endif

#define CHECK_SIZE 10
#define SPACING 2

#if GTK_CHECK_VERSION (3, 0, 0)
  color1.red = 30000. / 65535.;
  color1.green = 30000. / 65535.;
  color1.blue = 30000. / 65535.;
  color1.alpha = 1.0;

  color2.red = 50000. / 65535.;
  color2.green = 50000. / 65535.;
  color2.blue = 50000. / 65535.;
  color2.alpha = 1.0;
#else
  color1.red = 30000;
  color1.green = 30000;
  color1.blue = 30000;

  color2.red = 50000;
  color2.green = 50000;
  color2.blue = 50000;

  cr = gdk_cairo_create (drawable);
#endif

  xcount = 0;
  i = SPACING;
  while (i < width)
    {
      j = SPACING;
      ycount = xcount % 2; /* start with even/odd depending on row */
      while (j < height)
	{
	  if (ycount % 2)
#if GTK_CHECK_VERSION (3, 0, 0)
	    gdk_cairo_set_source_rgba (cr, &color1);
#else
	    gdk_cairo_set_source_color (cr, &color1);
#endif
	  else
#if GTK_CHECK_VERSION (3, 0, 0)
	    gdk_cairo_set_source_rgba (cr, &color2);
#else
	    gdk_cairo_set_source_color (cr, &color2);
#endif

	  /* If we're outside event->area, this will do nothing.
	   * It might be mildly more efficient if we handled
	   * the clipping ourselves, but again we're feeling lazy.
	   */
          cairo_rectangle (cr, i, j, CHECK_SIZE, CHECK_SIZE);
          cairo_fill (cr);

	  j += CHECK_SIZE + SPACING;
	  ++ycount;
	}

      i += CHECK_SIZE + SPACING;
      ++xcount;
    }

#if !GTK_CHECK_VERSION (3, 0, 0)
  cairo_destroy (cr);
#endif
}

static void
render_simple (
#if !GTK_CHECK_VERSION (3, 0, 0)
               GdkDrawable *drawable,
#endif
               cairo_t     *cr,
               int width, int height,
               MetaGradientType type,
               gboolean    with_alpha)
{
  GdkPixbuf *pixbuf;
#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA from, to;

  gdk_rgba_parse (&from, "blue");
  gdk_rgba_parse (&to, "green");
#else
  GdkColor from, to;

  gdk_color_parse ("blue", &from);
  gdk_color_parse ("green", &to);
#endif

  pixbuf = meta_gradient_create_simple (width, height,
                                        &from, &to,
                                        type);

  if (with_alpha)
    {
      const unsigned char alphas[] = { 0xff, 0xaa, 0x2f, 0x0, 0xcc, 0xff, 0xff };

      if (!gdk_pixbuf_get_has_alpha (pixbuf))
        {
          GdkPixbuf *new_pixbuf;

          new_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
          g_object_unref (G_OBJECT (pixbuf));
          pixbuf = new_pixbuf;
        }

      meta_gradient_add_alpha (pixbuf,
                               alphas, G_N_ELEMENTS (alphas),
                               META_GRADIENT_HORIZONTAL);

#if GTK_CHECK_VERSION (3, 0, 0)
      draw_checkerboard (cr, width, height);
#else
      draw_checkerboard (drawable, width, height);
#endif
    }

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  g_object_unref (G_OBJECT (pixbuf));
}

static void
render_vertical_func (
#if !GTK_CHECK_VERSION (3, 0, 0)
                      GdkDrawable *drawable,
#endif
                      cairo_t *cr,
                      int width, int height)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  render_simple (cr, width, height, META_GRADIENT_VERTICAL, FALSE);
#else
  render_simple (drawable, cr, width, height, META_GRADIENT_VERTICAL, FALSE);
#endif
}

static void
render_horizontal_func (
#if !GTK_CHECK_VERSION (3, 0, 0)
                        GdkDrawable *drawable,
#endif
                        cairo_t *cr,
                        int width, int height)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  render_simple (cr, width, height, META_GRADIENT_HORIZONTAL, FALSE);
#else
  render_simple (drawable, cr, width, height, META_GRADIENT_HORIZONTAL, FALSE);
#endif
}

static void
render_diagonal_func (
#if !GTK_CHECK_VERSION (3, 0, 0)
                      GdkDrawable *drawable,
#endif
                      cairo_t *cr,
                      int width, int height)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  render_simple (cr, width, height, META_GRADIENT_DIAGONAL, FALSE);
#else
  render_simple (drawable, cr, width, height, META_GRADIENT_DIAGONAL, FALSE);
#endif
}

static void
render_diagonal_alpha_func (
#if !GTK_CHECK_VERSION (3, 0, 0)
                            GdkDrawable *drawable,
#endif
                            cairo_t *cr,
                            int width, int height)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  render_simple (cr, width, height, META_GRADIENT_DIAGONAL, TRUE);
#else
  render_simple (drawable, cr, width, height, META_GRADIENT_DIAGONAL, TRUE);
#endif
}

static void
render_multi (
#if !GTK_CHECK_VERSION (3, 0, 0)
              GdkDrawable *drawable,
#endif
              cairo_t     *cr,
              int width, int height,
              MetaGradientType type)
{
  GdkPixbuf *pixbuf;
#define N_COLORS 5

#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA colors[N_COLORS];

  gdk_rgba_parse (&colors[0], "red");
  gdk_rgba_parse (&colors[1], "blue");
  gdk_rgba_parse (&colors[2], "orange");
  gdk_rgba_parse (&colors[3], "pink");
  gdk_rgba_parse (&colors[4], "green");
#else
  GdkColor colors[N_COLORS];

  gdk_color_parse ("red", &colors[0]);
  gdk_color_parse ("blue", &colors[1]);
  gdk_color_parse ("orange", &colors[2]);
  gdk_color_parse ("pink", &colors[3]);
  gdk_color_parse ("green", &colors[4]);
#endif

  pixbuf = meta_gradient_create_multi (width, height,
                                       colors, N_COLORS,
                                       type);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  g_object_unref (G_OBJECT (pixbuf));
#undef N_COLORS
}

static void
render_vertical_multi_func (
#if !GTK_CHECK_VERSION (3, 0, 0)
                            GdkDrawable *drawable,
#endif
                            cairo_t *cr,
                            int width, int height)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  render_multi (cr, width, height, META_GRADIENT_VERTICAL);
#else
  render_multi (drawable, cr, width, height, META_GRADIENT_VERTICAL);
#endif
}

static void
render_horizontal_multi_func (
#if !GTK_CHECK_VERSION (3, 0, 0)
                              GdkDrawable *drawable,
#endif
                              cairo_t *cr,
                              int width, int height)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  render_multi (cr, width, height, META_GRADIENT_HORIZONTAL);
#else
  render_multi (drawable, cr, width, height, META_GRADIENT_HORIZONTAL);
#endif
}

static void
render_diagonal_multi_func (
#if !GTK_CHECK_VERSION (3, 0, 0)
                            GdkDrawable *drawable,
#endif
                            cairo_t *cr,
                            int width, int height)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  render_multi (cr, width, height, META_GRADIENT_DIAGONAL);
#else
  render_multi (drawable, cr, width, height, META_GRADIENT_DIAGONAL);
#endif
}

static void
render_interwoven_func (
#if !GTK_CHECK_VERSION (3, 0, 0)
                        GdkDrawable *drawable,
#endif
                        cairo_t     *cr,
                        int width, int height)
{
  GdkPixbuf *pixbuf;
#define N_COLORS 4

#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA colors[N_COLORS];

  gdk_rgba_parse (&colors[0], "red");
  gdk_rgba_parse (&colors[1], "blue");
  gdk_rgba_parse (&colors[2], "pink");
  gdk_rgba_parse (&colors[3], "green");
#else
  GdkColor colors[N_COLORS];

  gdk_color_parse ("red", &colors[0]);
  gdk_color_parse ("blue", &colors[1]);
  gdk_color_parse ("pink", &colors[2]);
  gdk_color_parse ("green", &colors[3]);
#endif

  pixbuf = meta_gradient_create_interwoven (width, height,
                                            colors, height / 10,
                                            colors + 2, height / 14);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  g_object_unref (G_OBJECT (pixbuf));
}

static gboolean
#if GTK_CHECK_VERSION (3, 0, 0)
draw_callback (GtkWidget *widget,
               cairo_t *cr,
               gpointer data)
#else
expose_callback (GtkWidget *widget,
                 GdkEventExpose *event,
                 gpointer data)
#endif
{
  RenderGradientFunc func = data;
  GtkAllocation allocation;
#if GTK_CHECK_VERSION (3, 0, 0)
  GtkStyleContext *style;
  GdkRGBA color;

  style = gtk_widget_get_style_context (widget);

  gtk_style_context_save (style);
  gtk_style_context_set_state (style, gtk_widget_get_state_flags (widget));
  gtk_style_context_lookup_color (style, "foreground-color", &color);
  gtk_style_context_restore (style);
#else
  GdkWindow *window;
  GtkStyle *style;
  cairo_t *cr;

  style = gtk_widget_get_style (widget);
#endif

  gtk_widget_get_allocation (widget, &allocation);

#if GTK_CHECK_VERSION (3, 0, 0)
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, color.alpha);
#else
  window = gtk_widget_get_window (widget);
  cr = gdk_cairo_create (window);
  gdk_cairo_set_source_color (cr, &style->fg[gtk_widget_get_state (widget)]);
#endif

  (* func) (
#if !GTK_CHECK_VERSION (3, 0, 0)
            window,
#endif
            cr,
            allocation.width,
            allocation.height);

#if !GTK_CHECK_VERSION (3, 0, 0)
  cairo_destroy (cr);
#endif

#if GTK_CHECK_VERSION (3, 0, 0)
  return FALSE;
#else
  return TRUE;
#endif
}

static GtkWidget*
create_gradient_window (const char *title,
                        RenderGradientFunc func)
{
  GtkWidget *window;
  GtkWidget *drawing_area;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (window), title);

  drawing_area = gtk_drawing_area_new ();

  gtk_widget_set_size_request (drawing_area, 1, 1);

  gtk_window_set_default_size (GTK_WINDOW (window), 175, 175);

  g_signal_connect (G_OBJECT (drawing_area),
#if GTK_CHECK_VERSION (3, 0, 0)
                    "draw",
                    G_CALLBACK (draw_callback),
#else
                    "expose_event",
                    G_CALLBACK (expose_callback),
#endif
                    func);

  gtk_container_add (GTK_CONTAINER (window), drawing_area);

  gtk_widget_show_all (window);

  return window;
}

static void
meta_gradient_test (void)
{
  create_gradient_window ("Simple vertical",
                          render_vertical_func);

  create_gradient_window ("Simple horizontal",
                          render_horizontal_func);

  create_gradient_window ("Simple diagonal",
                          render_diagonal_func);

  create_gradient_window ("Multi vertical",
                          render_vertical_multi_func);

  create_gradient_window ("Multi horizontal",
                          render_horizontal_multi_func);

  create_gradient_window ("Multi diagonal",
                          render_diagonal_multi_func);

  create_gradient_window ("Interwoven",
                          render_interwoven_func);

  create_gradient_window ("Simple diagonal with horizontal multi alpha",
                          render_diagonal_alpha_func);

}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  meta_gradient_test ();

  gtk_main ();

  return 0;
}

