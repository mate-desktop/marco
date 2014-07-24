/* Marco interface for talking to GTK+ UI module */

/*
 * Copyright (C) 2002 Havoc Pennington
 * stock icon code Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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

#include "prefs.h"
#include "ui.h"
#include "frames.h"
#include "util.h"
#include "menu.h"
#include "core.h"
#include "theme.h"

#include <string.h>
#include <stdlib.h>

#if GTK_CHECK_VERSION (3, 0, 0)
#include <cairo-xlib.h>
#endif

static void meta_ui_accelerator_parse(const char* accel, guint* keysym, guint* keycode, GdkModifierType* keymask);

struct _MetaUI {
	Display* xdisplay;
	Screen* xscreen;
	MetaFrames* frames;

	/* For double-click tracking */
	guint button_click_number;
	Window button_click_window;
	int button_click_x;
	int button_click_y;
	guint32 button_click_time;
};

void meta_ui_init(int* argc, char*** argv)
{
  /* As of 2.91.7, Gdk uses XI2 by default, which conflicts with the
   * direct X calls we use - in particular, events caused by calls to
   * XGrabPointer/XGrabKeyboard are no longer understood by GDK, while
   * GDK will no longer generate the core XEvents we process.
   * So at least for now, enforce the previous behavior.
   */
#if GTK_CHECK_VERSION(2, 91, 7)
  gdk_disable_multidevice ();
#endif

	if (!gtk_init_check (argc, argv))
	{
		meta_fatal ("Unable to open X display %s\n", XDisplayName (NULL));
	}
}

Display* meta_ui_get_display(void)
{
	return GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
}

/* We do some of our event handling in frames.c, which expects
 * GDK events delivered by GTK+.  However, since the transition to
 * client side windows, we can't let GDK see button events, since the
 * client-side tracking of implicit and explicit grabs it does will
 * get confused by our direct use of X grabs in the core code.
 *
 * So we do a very minimal GDK => GTK event conversion here and send on the
 * events we care about, and then filter them out so they don't go
 * through the normal GDK event handling.
 *
 * To reduce the amount of code, the only events fields filled out
 * below are the ones that frames.c uses. If frames.c is modified to
 * use more fields, more fields need to be filled out below.
 */

static gboolean
maybe_redirect_mouse_event (XEvent *xevent)
{
  GdkDisplay *gdisplay;
  MetaUI *ui;
#if GTK_CHECK_VERSION (3, 0, 0)
  GdkDeviceManager *gmanager;
  GdkDevice *gdevice;
  GdkEvent *gevent;
#else
  GdkEvent gevent;
#endif
  GdkWindow *gdk_window;
  Window window;

  switch (xevent->type)
    {
    case ButtonPress:
    case ButtonRelease:
      window = xevent->xbutton.window;
      break;
    case MotionNotify:
      window = xevent->xmotion.window;
      break;
    case EnterNotify:
    case LeaveNotify:
      window = xevent->xcrossing.window;
      break;
    default:
      return FALSE;
    }

  gdisplay = gdk_x11_lookup_xdisplay (xevent->xany.display);
  ui = g_object_get_data (G_OBJECT (gdisplay), "meta-ui");
  if (!ui)
    return FALSE;

#if GTK_CHECK_VERSION (3, 0, 0)
  gdk_window = gdk_x11_window_lookup_for_display (gdisplay, window);
#else
  gdk_window = gdk_window_lookup_for_display (gdisplay, window);
#endif
  if (gdk_window == NULL)
    return FALSE;

#if GTK_CHECK_VERSION (3, 0, 0)
  gmanager = gdk_display_get_device_manager (gdisplay);
  gdevice = gdk_device_manager_get_client_pointer (gmanager);
#endif

  /* If GDK already thinks it has a grab, we better let it see events; this
   * is the menu-navigation case and events need to get sent to the appropriate
   * (client-side) subwindow for individual menu items.
   */
#if GTK_CHECK_VERSION (3, 0, 0)
  if (gdk_display_device_is_grabbed (gdisplay, gdevice))
#else
  if (gdk_display_pointer_is_grabbed (gdisplay))
#endif
    return FALSE;

#if !GTK_CHECK_VERSION (3, 0, 0)
  memset (&gevent, 0, sizeof (gevent));
#endif

  switch (xevent->type)
    {
    case ButtonPress:
    case ButtonRelease:
      if (xevent->type == ButtonPress)
        {
          GtkSettings *settings = gtk_settings_get_default ();
          int double_click_time;
          int double_click_distance;

          g_object_get (settings,
                        "gtk-double-click-time", &double_click_time,
                        "gtk-double-click-distance", &double_click_distance,
                        NULL);

          if (xevent->xbutton.button == ui->button_click_number &&
              xevent->xbutton.window == ui->button_click_window &&
              xevent->xbutton.time < ui->button_click_time + double_click_time &&
              ABS (xevent->xbutton.x - ui->button_click_x) <= double_click_distance &&
              ABS (xevent->xbutton.y - ui->button_click_y) <= double_click_distance)
            {
#if GTK_CHECK_VERSION (3, 0, 0)
              gevent = gdk_event_new (GDK_2BUTTON_PRESS);
#else
              gevent.button.type = GDK_2BUTTON_PRESS;
#endif

              ui->button_click_number = 0;
            }
          else
            {
#if GTK_CHECK_VERSION (3, 0, 0)
              gevent = gdk_event_new (GDK_BUTTON_PRESS);
#else
              gevent.button.type = GDK_BUTTON_PRESS;
#endif
              ui->button_click_number = xevent->xbutton.button;
              ui->button_click_window = xevent->xbutton.window;
              ui->button_click_time = xevent->xbutton.time;
              ui->button_click_x = xevent->xbutton.x;
              ui->button_click_y = xevent->xbutton.y;
            }
        }
      else
        {
#if GTK_CHECK_VERSION (3, 0, 0)
          gevent = gdk_event_new (GDK_BUTTON_RELEASE);
#else
          gevent.button.type = GDK_BUTTON_RELEASE;
#endif
        }

#if GTK_CHECK_VERSION (3, 0, 0)
      gevent->button.window = g_object_ref (gdk_window);
      gevent->button.button = xevent->xbutton.button;
      gevent->button.time = xevent->xbutton.time;
      gevent->button.x = xevent->xbutton.x;
      gevent->button.y = xevent->xbutton.y;
      gevent->button.x_root = xevent->xbutton.x_root;
      gevent->button.y_root = xevent->xbutton.y_root;
#else
      gevent.button.window = gdk_window;
      gevent.button.button = xevent->xbutton.button;
      gevent.button.time = xevent->xbutton.time;
      gevent.button.x = xevent->xbutton.x;
      gevent.button.y = xevent->xbutton.y;
      gevent.button.x_root = xevent->xbutton.x_root;
      gevent.button.y_root = xevent->xbutton.y_root;
#endif

      break;
    case MotionNotify:
#if GTK_CHECK_VERSION (3, 0, 0)
      gevent = gdk_event_new (GDK_MOTION_NOTIFY);
      gevent->motion.type = GDK_MOTION_NOTIFY;
      gevent->motion.window = g_object_ref (gdk_window);
#else
      gevent.motion.type = GDK_MOTION_NOTIFY;
      gevent.motion.window = gdk_window;
#endif
      break;
    case EnterNotify:
    case LeaveNotify:
#if GTK_CHECK_VERSION (3, 0, 0)
      gevent = gdk_event_new (xevent->type == EnterNotify ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY);
      gevent->crossing.window = g_object_ref (gdk_window);
      gevent->crossing.x = xevent->xcrossing.x;
      gevent->crossing.y = xevent->xcrossing.y;
#else
      gevent.crossing.type = xevent->type == EnterNotify ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY;
      gevent.crossing.window = gdk_window;
      gevent.crossing.x = xevent->xcrossing.x;
      gevent.crossing.y = xevent->xcrossing.y;
#endif
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  /* If we've gotten here, we've filled in the gdk_event and should send it on */
#if GTK_CHECK_VERSION (3, 0, 0)
  gdk_event_set_device (gevent, gdevice);
  gtk_main_do_event (gevent);
  gdk_event_free (gevent);
#else
  gtk_main_do_event (&gevent);
#endif

  return TRUE;
}

typedef struct _EventFunc EventFunc;

struct _EventFunc
{
  MetaEventFunc func;
  gpointer data;
};

static EventFunc *ef = NULL;

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent *event,
             gpointer data)
{
  g_return_val_if_fail (ef != NULL, GDK_FILTER_CONTINUE);

  if ((* ef->func) (xevent, ef->data) ||
      maybe_redirect_mouse_event (xevent))
    return GDK_FILTER_REMOVE;
  else
    return GDK_FILTER_CONTINUE;
}

void
meta_ui_add_event_func (Display       *xdisplay,
                        MetaEventFunc  func,
                        gpointer       data)
{
  g_return_if_fail (ef == NULL);

  ef = g_new (EventFunc, 1);
  ef->func = func;
  ef->data = data;

  gdk_window_add_filter (NULL, filter_func, ef);
}

/* removal is by data due to proxy function */
void
meta_ui_remove_event_func (Display       *xdisplay,
                           MetaEventFunc  func,
                           gpointer       data)
{
  g_return_if_fail (ef != NULL);

  gdk_window_remove_filter (NULL, filter_func, ef);

  g_free (ef);
  ef = NULL;
}

MetaUI*
meta_ui_new (Display *xdisplay,
             Screen  *screen)
{
  GdkDisplay *gdisplay;
  MetaUI *ui;

  ui = g_new0 (MetaUI, 1);
  ui->xdisplay = xdisplay;
  ui->xscreen = screen;

  gdisplay = gdk_x11_lookup_xdisplay (xdisplay);
  g_assert (gdisplay == gdk_display_get_default ());

  g_assert (xdisplay == GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  ui->frames = meta_frames_new (XScreenNumberOfScreen (screen));
#if GTK_CHECK_VERSION (3, 0, 0)
  /* This does not actually show any widget. MetaFrames has been hacked so
   * that showing it doesn't actually do anything. But we need the flags
   * set for GTK to deliver events properly. */
  gtk_widget_show (GTK_WIDGET (ui->frames));
#else
  gtk_widget_realize (GTK_WIDGET (ui->frames));
#endif

  g_object_set_data (G_OBJECT (gdisplay), "meta-ui", ui);

  return ui;
}

void
meta_ui_free (MetaUI *ui)
{
  GdkDisplay *gdisplay;

  gtk_widget_destroy (GTK_WIDGET (ui->frames));

  gdisplay = gdk_x11_lookup_xdisplay (ui->xdisplay);
  g_object_set_data (G_OBJECT (gdisplay), "meta-ui", NULL);

  g_free (ui);
}

void
meta_ui_get_frame_geometry (MetaUI *ui,
                            Window frame_xwindow,
                            int *top_height, int *bottom_height,
                            int *left_width, int *right_width)
{
  meta_frames_get_geometry (ui->frames, frame_xwindow,
                            top_height, bottom_height,
                            left_width, right_width);
}

Window
meta_ui_create_frame_window (MetaUI *ui,
                             Display *xdisplay,
                             Visual *xvisual,
			     gint x,
			     gint y,
			     gint width,
			     gint height,
			     gint screen_no)
{
  GdkDisplay *display = gdk_x11_lookup_xdisplay (xdisplay);
  GdkScreen *screen = gdk_display_get_screen (display, screen_no);
  GdkWindowAttr attrs;
  gint attributes_mask;
  GdkWindow *window;
  GdkVisual *visual;
#if !GTK_CHECK_VERSION (3, 0, 0)
  GdkColormap *cmap = gdk_screen_get_default_colormap (screen);
#endif

  /* Default depth/visual handles clients with weird visuals; they can
   * always be children of the root depth/visual obviously, but
   * e.g. DRI games can't be children of a parent that has the same
   * visual as the client.
   */
  if (!xvisual)
    visual = gdk_screen_get_system_visual (screen);
  else
    {
      visual = gdk_x11_screen_lookup_visual (screen,
                                             XVisualIDFromVisual (xvisual));
#if !GTK_CHECK_VERSION (3, 0, 0)
      cmap = gdk_colormap_new (visual, FALSE);
#endif
    }

  attrs.title = NULL;

  /* frame.c is going to replace the event mask immediately, but
   * we still have to set it here to let GDK know what it is.
   */
  attrs.event_mask =
    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
    GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_FOCUS_CHANGE_MASK;
  attrs.x = x;
  attrs.y = y;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.visual = visual;
#if !GTK_CHECK_VERSION (3, 0, 0)
  attrs.colormap = cmap;
#endif
  attrs.window_type = GDK_WINDOW_CHILD;
  attrs.cursor = NULL;
  attrs.wmclass_name = NULL;
  attrs.wmclass_class = NULL;
  attrs.override_redirect = FALSE;

  attrs.width  = width;
  attrs.height = height;

#if GTK_CHECK_VERSION (3, 0, 0)
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
#else
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
#endif

  window =
    gdk_window_new (gdk_screen_get_root_window(screen),
		    &attrs, attributes_mask);

  gdk_window_resize (window, width, height);

  meta_frames_manage_window (ui->frames, GDK_WINDOW_XID (window), window);

  return GDK_WINDOW_XID (window);
}

void
meta_ui_destroy_frame_window (MetaUI *ui,
			      Window  xwindow)
{
  meta_frames_unmanage_window (ui->frames, xwindow);
}

void
meta_ui_move_resize_frame (MetaUI *ui,
			   Window frame,
			   int x,
			   int y,
			   int width,
			   int height)
{
  meta_frames_move_resize_frame (ui->frames, frame, x, y, width, height);
}

void
meta_ui_map_frame   (MetaUI *ui,
                     Window  xwindow)
{
  GdkWindow *window;

  GdkDisplay *display;

  display = gdk_x11_lookup_xdisplay (ui->xdisplay);
  window = gdk_x11_window_lookup_for_display (display, xwindow);

  if (window)
    gdk_window_show_unraised (window);
}

void
meta_ui_unmap_frame (MetaUI *ui,
                     Window  xwindow)
{
  GdkWindow *window;

  GdkDisplay *display;

  display = gdk_x11_lookup_xdisplay (ui->xdisplay);
  window = gdk_x11_window_lookup_for_display (display, xwindow);

  if (window)
    gdk_window_hide (window);
}

void
meta_ui_unflicker_frame_bg (MetaUI *ui,
                            Window  xwindow,
                            int     target_width,
                            int     target_height)
{
  meta_frames_unflicker_bg (ui->frames, xwindow,
                            target_width, target_height);
}

void
meta_ui_repaint_frame (MetaUI *ui,
                       Window xwindow)
{
  meta_frames_repaint_frame (ui->frames, xwindow);
}

void
meta_ui_reset_frame_bg (MetaUI *ui,
                        Window xwindow)
{
  meta_frames_reset_bg (ui->frames, xwindow);
}

void
meta_ui_apply_frame_shape  (MetaUI  *ui,
                            Window   xwindow,
                            int      new_window_width,
                            int      new_window_height,
                            gboolean window_has_shape)
{
  meta_frames_apply_shapes (ui->frames, xwindow,
                            new_window_width, new_window_height,
                            window_has_shape);
}

void
meta_ui_queue_frame_draw (MetaUI *ui,
                          Window xwindow)
{
  meta_frames_queue_draw (ui->frames, xwindow);
}


void
meta_ui_set_frame_title (MetaUI     *ui,
                         Window      xwindow,
                         const char *title)
{
  meta_frames_set_title (ui->frames, xwindow, title);
}

MetaWindowMenu*
meta_ui_window_menu_new  (MetaUI             *ui,
                          Window              client_xwindow,
                          MetaMenuOp          ops,
                          MetaMenuOp          insensitive,
                          unsigned long       active_workspace,
                          int                 n_workspaces,
                          MetaWindowMenuFunc  func,
                          gpointer            data)
{
  return meta_window_menu_new (ui->frames,
                               ops, insensitive,
                               client_xwindow,
                               active_workspace,
                               n_workspaces,
                               func, data);
}

void
meta_ui_window_menu_popup (MetaWindowMenu     *menu,
                           int                 root_x,
                           int                 root_y,
                           int                 button,
                           guint32             timestamp)
{
  meta_window_menu_popup (menu, root_x, root_y, button, timestamp);
}

void
meta_ui_window_menu_free (MetaWindowMenu *menu)
{
  meta_window_menu_free (menu);
}

#if !GTK_CHECK_VERSION (3, 0, 0)
struct _MetaImageWindow
{
  GtkWidget *window;
  GdkPixmap *pixmap;
};

MetaImageWindow*
meta_image_window_new (Display *xdisplay,
                       int      screen_number,
                       int      max_width,
                       int      max_height)
{
  MetaImageWindow *iw;
  GdkDisplay *gdisplay;
  GdkScreen *gscreen;

  iw = g_new (MetaImageWindow, 1);
  iw->window = gtk_window_new (GTK_WINDOW_POPUP);

  gdisplay = gdk_x11_lookup_xdisplay (xdisplay);
  gscreen = gdk_display_get_screen (gdisplay, screen_number);

  gtk_window_set_screen (GTK_WINDOW (iw->window), gscreen);

  gtk_widget_realize (iw->window);
  iw->pixmap = gdk_pixmap_new (gtk_widget_get_window (iw->window),
                               max_width, max_height,
                               -1);

  gtk_widget_set_size_request (iw->window, 1, 1);
  gtk_widget_set_double_buffered (iw->window, FALSE);
  gtk_widget_set_app_paintable (iw->window, TRUE);

  return iw;
}

void
meta_image_window_free (MetaImageWindow *iw)
{
  gtk_widget_destroy (iw->window);
  g_object_unref (G_OBJECT (iw->pixmap));
  g_free (iw);
}

void
meta_image_window_set_showing  (MetaImageWindow *iw,
                                gboolean         showing)
{
  if (showing)
    gtk_widget_show_all (iw->window);
  else
    {
      gtk_widget_hide (iw->window);
      meta_core_increment_event_serial (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
    }
}

void
meta_image_window_set (MetaImageWindow *iw,
                       GdkPixbuf       *pixbuf,
                       int              x,
                       int              y)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  cairo_t *cr;
#endif

  /* We use a back pixmap to avoid having to handle exposes, because
   * it's really too slow for large clients being minimized, etc.
   * and this way flicker is genuinely zero.
   */

#if !GTK_CHECK_VERSION (3, 0, 0)
  gdk_draw_pixbuf (iw->pixmap,
                   gtk_widget_get_style (iw->window)->black_gc,
                   pixbuf,
                   0, 0,
                   0, 0,
                   gdk_pixbuf_get_width (pixbuf),
                   gdk_pixbuf_get_height (pixbuf),
                   GDK_RGB_DITHER_NORMAL,
                   0, 0);
#else
  cr = gdk_cairo_create (iw->pixmap);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
#endif

  gdk_window_set_back_pixmap (gtk_widget_get_window (iw->window),
                              iw->pixmap,
                              FALSE);

  gdk_window_move_resize (gtk_widget_get_window (iw->window),
                          x, y,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf));

  gdk_window_clear (gtk_widget_get_window (iw->window));
}
#endif

#if !GTK_CHECK_VERSION (3, 0, 0)
static GdkColormap*
get_cmap (GdkPixmap *pixmap)
{
  GdkColormap *cmap;

  cmap = gdk_drawable_get_colormap (pixmap);
  if (cmap)
    g_object_ref (G_OBJECT (cmap));

  if (cmap == NULL)
    {
      if (gdk_drawable_get_depth (pixmap) == 1)
        {
          meta_verbose ("Using NULL colormap for snapshotting bitmap\n");
          cmap = NULL;
        }
      else
        {
          meta_verbose ("Using system cmap to snapshot pixmap\n");
          cmap = gdk_screen_get_system_colormap (gdk_drawable_get_screen (pixmap));

          g_object_ref (G_OBJECT (cmap));
        }
    }

  /* Be sure we aren't going to blow up due to visual mismatch */
  if (cmap &&
      (gdk_colormap_get_visual (cmap)->depth !=
       gdk_drawable_get_depth (pixmap)))
    {
      cmap = NULL;
      meta_verbose ("Switching back to NULL cmap because of depth mismatch\n");
    }

  return cmap;
}
#endif

#if GTK_CHECK_VERSION (3, 0, 0)
GdkPixbuf*
meta_gdk_pixbuf_get_from_pixmap (GdkPixbuf   *dest,
                                 Pixmap       xpixmap,
                                 int          src_x,
                                 int          src_y,
                                 int          dest_x,
                                 int          dest_y,
                                 int          width,
                                 int          height)
{
  cairo_surface_t *surface;
  Display *display;
  Window root_return;
  int x_ret, y_ret;
  unsigned int w_ret, h_ret, bw_ret, depth_ret;
  XWindowAttributes attrs;
  GdkPixbuf *retval;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  if (!XGetGeometry (display, xpixmap, &root_return,
                     &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
    return NULL;

  if (depth_ret == 1)
    {
      surface = cairo_xlib_surface_create_for_bitmap (display,
                                                      xpixmap,
                                                      GDK_SCREEN_XSCREEN (gdk_screen_get_default ()),
                                                      w_ret,
                                                      h_ret);
    }
  else
    {
      if (!XGetWindowAttributes (display, root_return, &attrs))
        return NULL;

      surface = cairo_xlib_surface_create (display,
                                           xpixmap,
                                           attrs.visual,
                                           w_ret, h_ret);
    }

  retval = gdk_pixbuf_get_from_surface (surface,
                                        src_x,
                                        src_y,
                                        width,
                                        height);
  cairo_surface_destroy (surface);

  return retval;
}
#else
GdkPixbuf*
meta_gdk_pixbuf_get_from_pixmap (GdkPixbuf   *dest,
                                 Pixmap       xpixmap,
                                 int          src_x,
                                 int          src_y,
                                 int          dest_x,
                                 int          dest_y,
                                 int          width,
                                 int          height)
{
  GdkDrawable *drawable;
  GdkPixbuf *retval;
#if !GTK_CHECK_VERSION (3, 0, 0)
  GdkColormap *cmap;
#endif

  retval = NULL;
#if !GTK_CHECK_VERSION (3, 0, 0)
  cmap = NULL;
#endif

  drawable = gdk_x11_window_lookup_for_display (gdk_display_get_default (), xpixmap);

  if (drawable)
    g_object_ref (G_OBJECT (drawable));
  else
    drawable = gdk_pixmap_foreign_new (xpixmap);

  if (drawable)
    {
      cmap = get_cmap (drawable);

      retval = gdk_pixbuf_get_from_drawable (dest,
                                             drawable,
                                             cmap,
                                             src_x, src_y,
                                             dest_x, dest_y,
                                             width, height);
    }
#if !GTK_CHECK_VERSION (3, 0, 0)
  if (cmap)
    g_object_unref (G_OBJECT (cmap));
#endif
  if (drawable)
    g_object_unref (G_OBJECT (drawable));

  return retval;
}
#endif

void
meta_ui_push_delay_exposes (MetaUI *ui)
{
  meta_frames_push_delay_exposes (ui->frames);
}

void
meta_ui_pop_delay_exposes  (MetaUI *ui)
{
  meta_frames_pop_delay_exposes (ui->frames);
}

GdkPixbuf*
meta_ui_get_default_window_icon (MetaUI *ui)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      GtkIconTheme *theme;
      gboolean icon_exists;

      theme = gtk_icon_theme_get_default ();

      icon_exists = gtk_icon_theme_has_icon (theme, META_DEFAULT_ICON_NAME);

      if (icon_exists)
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   META_DEFAULT_ICON_NAME,
                                                   META_ICON_WIDTH,
                                                   0,
                                                   NULL);
      else
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   "gtk-missing-image",
                                                   META_ICON_WIDTH,
                                                   0,
                                                   NULL);

      g_assert (default_icon);
    }

  g_object_ref (G_OBJECT (default_icon));

  return default_icon;
}

GdkPixbuf*
meta_ui_get_default_mini_icon (MetaUI *ui)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      GtkIconTheme *theme;
      gboolean icon_exists;

      theme = gtk_icon_theme_get_default ();

      icon_exists = gtk_icon_theme_has_icon (theme, META_DEFAULT_ICON_NAME);

      if (icon_exists)
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   META_DEFAULT_ICON_NAME,
                                                   META_MINI_ICON_WIDTH,
                                                   0,
                                                   NULL);
      else
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   "gtk-missing-image",
                                                   META_MINI_ICON_WIDTH,
                                                   0,
                                                   NULL);

      g_assert (default_icon);
    }

  g_object_ref (G_OBJECT (default_icon));

  return default_icon;
}

gboolean
meta_ui_window_should_not_cause_focus (Display *xdisplay,
                                       Window   xwindow)
{
  GdkWindow *window;

  GdkDisplay *display;

  display = gdk_x11_lookup_xdisplay (xdisplay);
  window = gdk_x11_window_lookup_for_display (display, xwindow);

  /* we shouldn't cause focus if we're an override redirect
   * toplevel which is not foreign
   */
  if (window && gdk_window_get_window_type (window) == GDK_WINDOW_TEMP)
    return TRUE;
  else
    return FALSE;
}

char*
meta_text_property_to_utf8 (Display             *xdisplay,
                            const XTextProperty *prop)
{
  GdkDisplay *display;
  char **list;
  int count;
  char *retval;

  list = NULL;

  display = gdk_x11_lookup_xdisplay (xdisplay);
  count = gdk_text_property_to_utf8_list_for_display (display,
                                                      gdk_x11_xatom_to_atom_for_display (display, prop->encoding),
                                                      prop->format,
                                                      prop->value,
                                                      prop->nitems,
                                                      &list);

  if (count == 0)
    retval = NULL;
  else
    {
      retval = list[0];
      list[0] = g_strdup (""); /* something to free */
    }

  g_strfreev (list);

  return retval;
}

void
meta_ui_theme_get_frame_borders (MetaUI *ui,
                                 MetaFrameType      type,
                                 MetaFrameFlags     flags,
                                 int               *top_height,
                                 int               *bottom_height,
                                 int               *left_width,
                                 int               *right_width)
{
  int text_height;
#if GTK_CHECK_VERSION (3, 0, 0)
  GtkStyleContext *style = NULL;
  PangoFontDescription *free_font_desc = NULL;
#endif
  PangoContext *context;
  const PangoFontDescription *font_desc;

  if (meta_ui_have_a_theme ())
    {
      context = gtk_widget_get_pango_context (GTK_WIDGET (ui->frames));
      font_desc = meta_prefs_get_titlebar_font ();

      if (!font_desc)
        {
#if GTK_CHECK_VERSION (3, 0, 0)
          GdkDisplay *display = gdk_x11_lookup_xdisplay (ui->xdisplay);
          GdkScreen *screen = gdk_display_get_screen (display, XScreenNumberOfScreen (ui->xscreen));
          GtkWidgetPath *widget_path;

          style = gtk_style_context_new ();
          gtk_style_context_set_screen (style, screen);
          widget_path = gtk_widget_path_new ();
          gtk_widget_path_append_type (widget_path, GTK_TYPE_WINDOW);
          gtk_style_context_set_path (style, widget_path);
          gtk_widget_path_free (widget_path);

          gtk_style_context_get (style, GTK_STATE_FLAG_NORMAL, "font", &free_font_desc, NULL);
          font_desc = (const PangoFontDescription *) free_font_desc;
#else
          GtkStyle *default_style;

          default_style = gtk_widget_get_default_style ();
          font_desc = default_style->font_desc;
#endif
        }

      text_height = meta_pango_font_desc_get_text_height (font_desc, context);

      meta_theme_get_frame_borders (meta_theme_get_current (),
                                    type, text_height, flags,
                                    top_height, bottom_height,
                                    left_width, right_width);

#if GTK_CHECK_VERSION (3, 0, 0)
      if (free_font_desc)
        pango_font_description_free (free_font_desc);
#endif
    }
  else
    {
      *top_height = *bottom_height = *left_width = *right_width = 0;
    }

#if GTK_CHECK_VERSION (3, 0, 0)
  if (style != NULL)
    g_object_unref (style);
#endif
}

void
meta_ui_set_current_theme (const char *name,
                           gboolean    force_reload)
{
  meta_theme_set_current (name, force_reload);
  meta_invalidate_default_icons ();
}

gboolean
meta_ui_have_a_theme (void)
{
  return meta_theme_get_current () != NULL;
}

static void
meta_ui_accelerator_parse (const char      *accel,
                           guint           *keysym,
                           guint           *keycode,
                           GdkModifierType *keymask)
{
  if (accel[0] == '0' && accel[1] == 'x')
    {
      *keysym = 0;
      *keycode = (guint) strtoul (accel, NULL, 16);
      *keymask = 0;
    }
  else
    gtk_accelerator_parse (accel, keysym, keymask);
}

gboolean
meta_ui_parse_accelerator (const char          *accel,
                           unsigned int        *keysym,
                           unsigned int        *keycode,
                           MetaVirtualModifier *mask)
{
  GdkModifierType gdk_mask = 0;
  guint gdk_sym = 0;
  guint gdk_code = 0;

  *keysym = 0;
  *keycode = 0;
  *mask = 0;

  if (!accel[0] || strcmp (accel, "disabled") == 0)
    return TRUE;

  meta_ui_accelerator_parse (accel, &gdk_sym, &gdk_code, &gdk_mask);
  if (gdk_mask == 0 && gdk_sym == 0 && gdk_code == 0)
    return FALSE;

  if (gdk_sym == None && gdk_code == 0)
    return FALSE;

  if (gdk_mask & GDK_RELEASE_MASK) /* we don't allow this */
    return FALSE;

  *keysym = gdk_sym;
  *keycode = gdk_code;

  if (gdk_mask & GDK_SHIFT_MASK)
    *mask |= META_VIRTUAL_SHIFT_MASK;
  if (gdk_mask & GDK_CONTROL_MASK)
    *mask |= META_VIRTUAL_CONTROL_MASK;
  if (gdk_mask & GDK_MOD1_MASK)
    *mask |= META_VIRTUAL_ALT_MASK;
  if (gdk_mask & GDK_MOD2_MASK)
    *mask |= META_VIRTUAL_MOD2_MASK;
  if (gdk_mask & GDK_MOD3_MASK)
    *mask |= META_VIRTUAL_MOD3_MASK;
  if (gdk_mask & GDK_MOD4_MASK)
    *mask |= META_VIRTUAL_MOD4_MASK;
  if (gdk_mask & GDK_MOD5_MASK)
    *mask |= META_VIRTUAL_MOD5_MASK;
  if (gdk_mask & GDK_SUPER_MASK)
    *mask |= META_VIRTUAL_SUPER_MASK;
  if (gdk_mask & GDK_HYPER_MASK)
    *mask |= META_VIRTUAL_HYPER_MASK;
  if (gdk_mask & GDK_META_MASK)
    *mask |= META_VIRTUAL_META_MASK;

  return TRUE;
}

/* Caller responsible for freeing return string of meta_ui_accelerator_name! */
gchar*
meta_ui_accelerator_name  (unsigned int        keysym,
                           MetaVirtualModifier mask)
{
  GdkModifierType mods = 0;

  if (keysym == 0 && mask == 0)
    {
      return g_strdup ("disabled");
    }

  if (mask & META_VIRTUAL_SHIFT_MASK)
    mods |= GDK_SHIFT_MASK;
  if (mask & META_VIRTUAL_CONTROL_MASK)
    mods |= GDK_CONTROL_MASK;
  if (mask & META_VIRTUAL_ALT_MASK)
    mods |= GDK_MOD1_MASK;
  if (mask & META_VIRTUAL_MOD2_MASK)
    mods |= GDK_MOD2_MASK;
  if (mask & META_VIRTUAL_MOD3_MASK)
    mods |= GDK_MOD3_MASK;
  if (mask & META_VIRTUAL_MOD4_MASK)
    mods |= GDK_MOD4_MASK;
  if (mask & META_VIRTUAL_MOD5_MASK)
    mods |= GDK_MOD5_MASK;
  if (mask & META_VIRTUAL_SUPER_MASK)
    mods |= GDK_SUPER_MASK;
  if (mask & META_VIRTUAL_HYPER_MASK)
    mods |= GDK_HYPER_MASK;
  if (mask & META_VIRTUAL_META_MASK)
    mods |= GDK_META_MASK;

  return gtk_accelerator_name (keysym, mods);

}

gboolean
meta_ui_parse_modifier (const char          *accel,
                        MetaVirtualModifier *mask)
{
  GdkModifierType gdk_mask = 0;
  guint gdk_sym = 0;
  guint gdk_code = 0;

  *mask = 0;

  if (accel == NULL || !accel[0] || strcmp (accel, "disabled") == 0)
    return TRUE;

  meta_ui_accelerator_parse (accel, &gdk_sym, &gdk_code, &gdk_mask);
  if (gdk_mask == 0 && gdk_sym == 0 && gdk_code == 0)
    return FALSE;

  if (gdk_sym != None || gdk_code != 0)
    return FALSE;

  if (gdk_mask & GDK_RELEASE_MASK) /* we don't allow this */
    return FALSE;

  if (gdk_mask & GDK_SHIFT_MASK)
    *mask |= META_VIRTUAL_SHIFT_MASK;
  if (gdk_mask & GDK_CONTROL_MASK)
    *mask |= META_VIRTUAL_CONTROL_MASK;
  if (gdk_mask & GDK_MOD1_MASK)
    *mask |= META_VIRTUAL_ALT_MASK;
  if (gdk_mask & GDK_MOD2_MASK)
    *mask |= META_VIRTUAL_MOD2_MASK;
  if (gdk_mask & GDK_MOD3_MASK)
    *mask |= META_VIRTUAL_MOD3_MASK;
  if (gdk_mask & GDK_MOD4_MASK)
    *mask |= META_VIRTUAL_MOD4_MASK;
  if (gdk_mask & GDK_MOD5_MASK)
    *mask |= META_VIRTUAL_MOD5_MASK;
  if (gdk_mask & GDK_SUPER_MASK)
    *mask |= META_VIRTUAL_SUPER_MASK;
  if (gdk_mask & GDK_HYPER_MASK)
    *mask |= META_VIRTUAL_HYPER_MASK;
  if (gdk_mask & GDK_META_MASK)
    *mask |= META_VIRTUAL_META_MASK;

  return TRUE;
}

gboolean
meta_ui_window_is_widget (MetaUI *ui,
                          Window  xwindow)
{
  GdkWindow *window;

  GdkDisplay *display;
  display = gdk_x11_lookup_xdisplay (ui->xdisplay);
  window = gdk_x11_window_lookup_for_display (display, xwindow);

  if (window)
    {
      void *user_data = NULL;
      gdk_window_get_user_data (window, &user_data);
      return user_data != NULL && user_data != ui->frames;
    }
  else
    return FALSE;
}

/* stock icon code Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org> */

typedef struct {
	char* stock_id;
	const guint8* icon_data;
} MetaStockIcon;

int meta_ui_get_drag_threshold(MetaUI* ui)
{
	int threshold = 8;
	GtkSettings* settings = gtk_widget_get_settings(GTK_WIDGET(ui->frames));

	g_object_get(G_OBJECT(settings), "gtk-dnd-drag-threshold", &threshold, NULL);

	return threshold;
}

MetaUIDirection meta_ui_get_direction(void)
{
	if (gtk_widget_get_default_direction() == GTK_TEXT_DIR_RTL)
	{
		return META_UI_DIRECTION_RTL;
	}

	return META_UI_DIRECTION_LTR;
}

#if !GTK_CHECK_VERSION (3, 0, 0)
GdkPixbuf* meta_ui_get_pixbuf_from_pixmap(Pixmap pmap)
{
	GdkPixmap* gpmap;
	GdkScreen* screen;
	GdkPixbuf* pixbuf;
#if !GTK_CHECK_VERSION (3, 0, 0)
	GdkColormap* cmap;
#endif
	int width;
	int height;
	int depth;

	gpmap = gdk_pixmap_foreign_new(pmap);
	screen = gdk_drawable_get_screen(gpmap);

#if GTK_CHECK_VERSION(3, 0, 0)
	width = gdk_window_get_width(GDK_WINDOW(gpmap));
	height = gdk_window_get_height(GDK_WINDOW(gpmap));
#else
	gdk_drawable_get_size(GDK_DRAWABLE(gpmap), &width, &height);
#endif

	depth = gdk_drawable_get_depth(GDK_DRAWABLE(gpmap));

#if !GTK_CHECK_VERSION (3, 0, 0)
	if (depth <= 24)
	{
		cmap = gdk_screen_get_system_colormap(screen);
	}
	else
	{
		cmap = gdk_screen_get_rgba_colormap(screen);
	}
#endif

	pixbuf = gdk_pixbuf_get_from_drawable(NULL, gpmap, cmap, 0, 0, 0, 0, width, height);

	g_object_unref(gpmap);

	return pixbuf;
}
#endif
