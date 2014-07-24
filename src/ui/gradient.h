/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco gradient rendering */

/*
 * Copyright (C) 2001 Havoc Pennington, 99% copied from wrlib in
 * WindowMaker, Copyright (C) 1997-2000 Dan Pascu and Alfredo Kojima
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

#ifndef META_GRADIENT_H
#define META_GRADIENT_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

typedef enum
{
  META_GRADIENT_VERTICAL,
  META_GRADIENT_HORIZONTAL,
  META_GRADIENT_DIAGONAL,
  META_GRADIENT_LAST
} MetaGradientType;

#if GTK_CHECK_VERSION (3, 0 ,0)
GdkPixbuf* meta_gradient_create_simple     (int               width,
                                            int               height,
                                            const GdkRGBA    *from,
                                            const GdkRGBA    *to,
                                            MetaGradientType  style);
GdkPixbuf* meta_gradient_create_multi      (int               width,
                                            int               height,
                                            const GdkRGBA    *colors,
                                            int               n_colors,
                                            MetaGradientType  style);
GdkPixbuf* meta_gradient_create_interwoven (int               width,
                                            int               height,
                                            const GdkRGBA     colors1[2],
                                            int               thickness1,
                                            const GdkRGBA     colors2[2],
                                            int               thickness2);
#else
GdkPixbuf* meta_gradient_create_simple     (int               width,
                                            int               height,
                                            const GdkColor   *from,
                                            const GdkColor   *to,
                                            MetaGradientType  style);
GdkPixbuf* meta_gradient_create_multi      (int               width,
                                            int               height,
                                            const GdkColor   *colors,
                                            int               n_colors,
                                            MetaGradientType  style);
GdkPixbuf* meta_gradient_create_interwoven (int               width,
                                            int               height,
                                            const GdkColor    colors1[2],
                                            int               thickness1,
                                            const GdkColor    colors2[2],
                                            int               thickness2);
#endif


/* Generate an alpha gradient and multiply it with the existing alpha
 * channel of the given pixbuf
 */
void meta_gradient_add_alpha (GdkPixbuf       *pixbuf,
                              const guchar    *alphas,
                              int              n_alphas,
                              MetaGradientType type);


#endif
