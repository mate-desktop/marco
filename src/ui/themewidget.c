/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco theme widget (displays themed draw operations) */

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
 * 02110-1301, USA.
 */

#include "themewidget.h"
#include <math.h>

static void meta_area_class_init   (MetaAreaClass  *klass);
static void meta_area_init         (MetaArea       *area);
static void meta_area_size_request (GtkWidget      *widget,
                                    GtkRequisition *req);
#if GTK_CHECK_VERSION(3, 0, 0)
static gboolean meta_area_draw       (GtkWidget      *widget,
                                      cairo_t        *cr);
static void meta_area_get_preferred_height (GtkWidget *widget,
                                            gint      *minimal,
                                            gint      *natural);
static void meta_area_get_preferred_width (GtkWidget *widget,
                                           gint      *minimal,
                                           gint      *natural);
#else
static gint meta_area_expose       (GtkWidget      *widget,
                                    GdkEventExpose *event);
#endif
static void meta_area_finalize     (GObject        *object);


#if GTK_CHECK_VERSION(3, 0, 0)

G_DEFINE_TYPE (MetaArea, meta_area, GTK_TYPE_MISC);

#else

static GtkMiscClass *parent_class;

GType
meta_area_get_type (void)
{
  static GType area_type = 0;

  if (!area_type)
    {
      static const GtkTypeInfo area_info =
      {
	"MetaArea",
	sizeof (MetaArea),
	sizeof (MetaAreaClass),
	(GtkClassInitFunc) meta_area_class_init,
	(GtkObjectInitFunc) meta_area_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      area_type = gtk_type_unique (GTK_TYPE_MISC, &area_info);
    }

  return area_type;
}

#endif

static void
meta_area_class_init (MetaAreaClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
  parent_class = g_type_class_peek (gtk_misc_get_type ());

  gobject_class->finalize = meta_area_finalize;

  #if GTK_CHECK_VERSION(3, 0, 0)
  widget_class->draw = meta_area_draw;
  widget_class->get_preferred_width = meta_area_get_preferred_width;
  widget_class->get_preferred_height = meta_area_get_preferred_height;
  #else
  widget_class->expose_event = meta_area_expose;
  widget_class->size_request = meta_area_size_request;
  #endif
}

static void
meta_area_init (MetaArea *area)
{
  #if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_has_window (GTK_WIDGET(area), FALSE);
  #else
  GTK_WIDGET_SET_FLAGS (area, GTK_NO_WINDOW);
  #endif
}

GtkWidget*
meta_area_new (void)
{
  MetaArea *area;

  #if GTK_CHECK_VERSION(3, 0, 0)
  area = g_object_new (META_TYPE_AREA, NULL);
  #else
  area = gtk_type_new (META_TYPE_AREA);
  #endif

  return GTK_WIDGET (area);
}

static void
meta_area_finalize (GObject *object)
{
  MetaArea *area;

  area = META_AREA (object);

  if (area->dnotify)
    (* area->dnotify) (area->user_data);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#if GTK_CHECK_VERSION(3, 0, 0)

static gboolean
meta_area_draw (GtkWidget       *widget,
                cairo_t         *cr)
{
  MetaArea *area;
  GtkMisc *misc;
  gint x, y;
  gfloat xalign, yalign;
  gint xpad, ypad;
  GtkAllocation allocation;
  GtkRequisition req;

  g_return_val_if_fail (META_IS_AREA (widget), FALSE);

  if (gtk_widget_is_drawable (widget))
    {
      area = META_AREA (widget);
      misc = GTK_MISC (widget);

      gtk_widget_get_allocation(widget, &allocation);
      gtk_widget_get_requisition(widget, &req);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
        gtk_misc_get_alignment(misc, &xalign, &yalign);
      else
      {
        gtk_misc_get_alignment(misc, &xalign, &yalign);
        xalign = 1.0 - xalign;
      }

      gtk_misc_get_padding(misc, &xpad, &ypad);


      x = floor (allocation.x + xpad
         + ((allocation.width - req.width) * xalign)
         + 0.5);
      y = floor (allocation.y + ypad
         + ((allocation.height - req.height) * yalign)
         + 0.5);

      if (area->draw_func)
        {
          (* area->draw_func) (area, cr,
                                 area->user_data);
        }
    }

  return FALSE;
}

#else

static gint
meta_area_expose (GtkWidget      *widget,
                  GdkEventExpose *event)
{
  MetaArea *area;
  GtkMisc *misc;
  gint x, y;
  gfloat xalign;

  g_return_val_if_fail (META_IS_AREA (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      area = META_AREA (widget);
      misc = GTK_MISC (widget);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = misc->xalign;
      else
	xalign = 1.0 - misc->xalign;

      x = floor (widget->allocation.x + misc->xpad
		 + ((widget->allocation.width - widget->requisition.width) * xalign)
		 + 0.5);
      y = floor (widget->allocation.y + misc->ypad
		 + ((widget->allocation.height - widget->requisition.height) * misc->yalign)
		 + 0.5);

      if (area->expose_func)
        {
          (* area->expose_func) (area, event, x, y,
                                 area->user_data);
        }
    }

  return FALSE;
}

#endif

static void
meta_area_size_request (GtkWidget      *widget,
                        GtkRequisition *req)
{
  MetaArea *area;

  area = META_AREA (widget);

  req->width = 0;
  req->height = 0;

  if (area->size_func)
    {
      (* area->size_func) (area, &req->width, &req->height,
                           area->user_data);
    }
}

#if GTK_CHECK_VERSION(3, 0, 0)

static void
meta_area_get_preferred_width (GtkWidget *widget,
                               gint      *minimal,
                               gint      *natural)
{
  GtkRequisition requisition;

  meta_area_size_request (widget, &requisition);

  *minimal = *natural = requisition.width;
}

static void
meta_area_get_preferred_height (GtkWidget *widget,
                               gint      *minimal,
                               gint      *natural)
{
  GtkRequisition requisition;

  meta_area_size_request (widget, &requisition);

  *minimal = *natural = requisition.height;
}

#endif

void
meta_area_setup (MetaArea           *area,
                 MetaAreaSizeFunc    size_func,
                 #if GTK_CHECK_VERSION(3, 0, 0)
                 MetaAreaDrawFunc    draw_func,
                 #else
                 MetaAreaExposeFunc  expose_func,
                 #endif
                 void               *user_data,
                 GDestroyNotify      dnotify)
{
  if (area->dnotify)
    (* area->dnotify) (area->user_data);

  area->size_func = size_func;
  #if GTK_CHECK_VERSION(3, 0, 0)
  area->draw_func = draw_func;
  #else
  area->expose_func = expose_func;
  #endif
  area->user_data = user_data;
  area->dnotify = dnotify;

  gtk_widget_queue_resize (GTK_WIDGET (area));
}

