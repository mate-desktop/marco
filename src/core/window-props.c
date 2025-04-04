/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file window-props.c    MetaWindow property handling
 *
 * A system which can inspect sets of properties of given windows
 * and take appropriate action given their values.
 *
 * Note that all the meta_window_reload_propert* functions require a
 * round trip to the server.
 *
 * The guts of this system are in meta_display_init_window_prop_hooks().
 * Reading this function will give you insight into how this all fits
 * together.
 */

/*
 * Copyright (C) 2001, 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2004, 2005 Elijah Newren
 * Copyright (C) 2009 Thomas Thurman
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

#define _GNU_SOURCE
#define _XOPEN_SOURCE /* for gethostname() */

#include <config.h>
#include <glib/gi18n-lib.h>

#include "window-props.h"
#include "errors.h"
#include "xprops.h"
#include "frame-private.h"
#include "group.h"
#include <X11/Xatom.h>
#include <X11/extensions/XRes.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>

#ifdef HAVE_GTOP
#include <glibtop/procuid.h>
#include <errno.h>
#include <pwd.h>
#endif /* HAVE_GTOP */

#ifndef HOST_NAME_MAX
/* Solaris headers apparently don't define this so do so manually; #326745 */
#define HOST_NAME_MAX 255
#endif

typedef void (* ReloadValueFunc) (MetaWindow    *window,
                                  MetaPropValue *value,
                                  gboolean       initial);

typedef struct MetaWindowPropHooks
{
  Atom              property;
  MetaPropValueType type;
  ReloadValueFunc   reload_func;
} MetaWindowPropHooks;

static MetaWindowPropHooks* find_hooks (MetaDisplay *display,
                                        Atom         property);

void
meta_window_reload_property (MetaWindow *window,
                             Atom        property,
                             gboolean    initial)
{
  meta_window_reload_properties (window, &property, 1, initial);
}

void
meta_window_reload_properties (MetaWindow *window,
                               const Atom *properties,
                               int         n_properties,
                               gboolean    initial)
{
  meta_window_reload_properties_from_xwindow (window,
                                              window->xwindow,
                                              properties,
                                              n_properties,
                                              initial);
}

void
meta_window_reload_property_from_xwindow (MetaWindow *window,
                                          Window      xwindow,
                                          Atom        property,
                                          gboolean    initial)
{
  meta_window_reload_properties_from_xwindow (window, xwindow, &property, 1,
                                              initial);
}

void
meta_window_reload_properties_from_xwindow (MetaWindow *window,
                                            Window      xwindow,
                                            const Atom *properties,
                                            int         n_properties,
                                            gboolean    initial)
{
  int i;
  MetaPropValue *values;

  g_return_if_fail (properties != NULL);
  g_return_if_fail (n_properties > 0);

  values = g_new0 (MetaPropValue, n_properties);

  for (i=0; i<n_properties; i++)
    {
      MetaWindowPropHooks *hooks = find_hooks (window->display,
                                               properties[i]);

      if (!hooks || hooks->type == META_PROP_VALUE_INVALID)
        {
          values[i].type = META_PROP_VALUE_INVALID;
          values[i].atom = None;
        }
      else
        {
          values[i].type = hooks->type;
          values[i].atom = properties[i];
        }
    }

  meta_prop_get_values (window->display, xwindow,
                        values, n_properties);

  for (i=0; i<n_properties; i++)
    {
      MetaWindowPropHooks *hooks = find_hooks (window->display,
                                               properties[i]);

      if (hooks && hooks->reload_func != NULL)
        (* hooks->reload_func) (window, &values[i], initial);
    }

  meta_prop_free_values (values, n_properties);

  g_free (values);
}

static void
reload_wm_client_machine (MetaWindow    *window,
                          MetaPropValue *value,
                          gboolean       initial)
{
  g_free (window->wm_client_machine);
  window->wm_client_machine = NULL;

  if (value->type != META_PROP_VALUE_INVALID)
    window->wm_client_machine = g_strdup (value->v.str);

  meta_verbose ("Window has client machine \"%s\"\n",
                window->wm_client_machine ? window->wm_client_machine : "unset");
}

static void
complain_about_broken_client (MetaWindow    *window,
                              MetaPropValue *value,
                              gboolean       initial)
{
  meta_warning ("Broken client! Window %s changed client leader window or SM client ID\n",
                window->desc);
}

static void
reload_net_wm_window_type (MetaWindow    *window,
                           MetaPropValue *value,
                           gboolean       initial)
{
  meta_window_update_net_wm_type (window);
}

static void
reload_icon (MetaWindow    *window,
             Atom           atom)
{
  meta_icon_cache_property_changed (&window->icon_cache,
                                    window->display,
                                    atom);
  meta_window_queue(window, META_QUEUE_UPDATE_ICON);
}

static void
reload_net_wm_icon (MetaWindow    *window,
                    MetaPropValue *value,
                    gboolean       initial)
{
  reload_icon (window, window->display->atom__NET_WM_ICON);
}

static void
reload_kwm_win_icon (MetaWindow    *window,
                     MetaPropValue *value,
                     gboolean       initial)
{
  reload_icon (window, window->display->atom__KWM_WIN_ICON);
}

static gboolean
gtk_border_equal (GtkBorder *a,
                  GtkBorder *b)
{
  return (a->left == b->left &&
          a->right == b->right &&
          a->top == b->top &&
          a->bottom == b->bottom);
}

static void
meta_window_set_custom_frame_extents (MetaWindow *window,
                                      GtkBorder  *extents)
{
  if (extents)
    {
      if (window->has_custom_frame_extents && gtk_border_equal (&window->custom_frame_extents, extents))
        return;

      window->has_custom_frame_extents = TRUE;
      window->custom_frame_extents = *extents;
    }
  else
    {
      if (!window->has_custom_frame_extents)
        return;

      window->has_custom_frame_extents = FALSE;
      memset (&window->custom_frame_extents, 0, sizeof (window->custom_frame_extents));
    }

  meta_window_queue (window, META_QUEUE_MOVE_RESIZE);
}

static void
reload_gtk_frame_extents (MetaWindow    *window,
                          MetaPropValue *value,
                          gboolean       initial)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      if (value->v.cardinal_list.n_cardinals != 4)
        {
          meta_verbose ("_GTK_FRAME_EXTENTS on %s has %d values instead of 4\n",
                        window->desc, value->v.cardinal_list.n_cardinals);
        }
      else
        {
          GtkBorder extents;
          extents.left   = (int)value->v.cardinal_list.cardinals[0];
          extents.right  = (int)value->v.cardinal_list.cardinals[1];
          extents.top    = (int)value->v.cardinal_list.cardinals[2];
          extents.bottom = (int)value->v.cardinal_list.cardinals[3];
          meta_window_set_custom_frame_extents (window, &extents);
        }
    }
  else
    {
      meta_window_set_custom_frame_extents (window, NULL);
    }
}

static void
reload_struts (MetaWindow    *window,
               MetaPropValue *value,
               gboolean       initial)
{
  meta_window_update_struts (window);
}

static void
reload_wm_window_role (MetaWindow    *window,
                       MetaPropValue *value,
                       gboolean       initial)
{
  meta_window_update_role (window);
}

static pid_t
get_local_pid (MetaWindow *window)
{
  pid_t pid;
  XResClientIdSpec spec;
  long num_ids;
  XResClientIdValue *client_ids;
  long i;

  pid = -1;
  if (window->display->have_xres == FALSE)
    return pid;

  spec.client = window->xwindow;
  spec.mask = XRES_CLIENT_ID_PID_MASK;

  if (XResQueryClientIds (window->display->xdisplay,
                          1,
                          &spec,
                          &num_ids,
                          &client_ids) != Success)
    return pid;

  for (i = 0; i < num_ids; i++)
    {
      pid = XResGetClientPid (&client_ids[i]);

      if (pid != -1)
        break;
    }

  XResClientIdsDestroy (num_ids, client_ids);

  return pid;
}

static void
reload_net_wm_pid (MetaWindow    *window,
                   MetaPropValue *value,
                   gboolean       initial)
{
  pid_t pid;

  pid = get_local_pid (window);

  if (pid != -1)
    {
      meta_verbose ("Using XResGetClientPid instead of _NET_WM_PID\n");

      window->net_wm_pid = pid;
      return;
    }
  if (value->type != META_PROP_VALUE_INVALID)
    {
      gulong cardinal = (int) value->v.cardinal;

      if (cardinal <= 0)
        meta_warning (_("Application set a bogus _NET_WM_PID %lu\n"),
                      cardinal);
      else
        {
          window->net_wm_pid = cardinal;
          meta_verbose ("Window has _NET_WM_PID %d\n",
                        window->net_wm_pid);
        }
    }
}

static void
reload_net_wm_user_time (MetaWindow    *window,
                         MetaPropValue *value,
                         gboolean       initial)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      gulong cardinal = value->v.cardinal;
      meta_window_set_user_time (window, cardinal);
    }
}

static void
reload_net_wm_user_time_window (MetaWindow    *window,
                                MetaPropValue *value,
                                gboolean       initial)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      /* Unregister old NET_WM_USER_TIME_WINDOW */
      if (window->user_time_window != None)
        {
          /* See the comment to the meta_display_register_x_window call below. */
          meta_display_unregister_x_window (window->display,
                                            window->user_time_window);
          /* Don't get events on not-managed windows */
          XSelectInput (window->display->xdisplay,
                        window->user_time_window,
                        NoEventMask);
        }

      /* Obtain the new NET_WM_USER_TIME_WINDOW and register it */
      window->user_time_window = value->v.xwindow;
      if (window->user_time_window != None)
        {
          /* Kind of a hack; display.c:event_callback() ignores events
           * for unknown windows.  We make window->user_time_window
           * known by registering it with window (despite the fact
           * that window->xwindow is already registered with window).
           * This basically means that property notifies to either the
           * window->user_time_window or window->xwindow will be
           * treated identically and will result in functions for
           * window being called to update it.  Maybe we should ignore
           * any property notifies to window->user_time_window other
           * than atom__NET_WM_USER_TIME ones, but I just don't care
           * and it's not specified in the spec anyway.
           */
          meta_display_register_x_window (window->display,
                                          &window->user_time_window,
                                          window);
          /* Just listen for property notify events */
          XSelectInput (window->display->xdisplay,
                        window->user_time_window,
                        PropertyChangeMask);

          /* Manually load the _NET_WM_USER_TIME field from the given window
           * at this time as well.  If the user_time_window ever broadens in
           * scope, we'll probably want to load all relevant properties here.
           */
          meta_window_reload_property_from_xwindow (
            window,
            window->user_time_window,
            window->display->atom__NET_WM_USER_TIME,
            initial);
        }
    }
}

/**
 * Finds who owns a particular process, if we can.
 *
 * \param process  The process's ID.
 * \result         Set to the ID of the user, if we returned true.
 *
 * \result  True if we could tell.
 */
static gboolean
owner_of_process (pid_t process, uid_t *result)
{
#ifdef HAVE_GTOP
  glibtop_proc_uid process_details;

  glibtop_get_proc_uid (&process_details, process);

  if (process_details.flags & (1L << GLIBTOP_PROC_UID_UID))
    {
      *result = process_details.uid;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
#else
  /* I don't know, maybe we could do something hairy like see whether
   * /proc/$PID exists and who owns it, in case they have procfs.
   */
  return FALSE;
#endif /* HAVE_GTOP */
}

#define MAX_TITLE_LENGTH 512

/**
 * Called by set_window_title and set_icon_title to set the value of
 * *target to title. It required and atom is set, it will update the
 * appropriate property.
 *
 * Returns TRUE if a new title was set.
 */
static gboolean
set_title_text (MetaWindow  *window,
                gboolean     previous_was_modified,
                const char  *title,
                Atom         atom,
                char       **target)
{
  char hostname[HOST_NAME_MAX + 1];
  gboolean modified = FALSE;

  if (!target)
    return FALSE;

  g_free (*target);

  if (!title || !*title)
    *target = g_strdup ("");
  else if (g_utf8_strlen (title, MAX_TITLE_LENGTH + 1) > MAX_TITLE_LENGTH)
    {
      *target = meta_g_utf8_strndup (title, MAX_TITLE_LENGTH);
      modified = TRUE;
    }
  /* if WM_CLIENT_MACHINE indicates this machine is on a remote host
   * let's place that hostname in the title */
  else if (window->wm_client_machine &&
           !gethostname (hostname, HOST_NAME_MAX + 1) &&
           strcmp (hostname, window->wm_client_machine))
    {
      /* Translators: the title of a window from another machine */
      *target = g_strdup_printf (_("%s (on %s)"),
                      title, window->wm_client_machine);
      modified = TRUE;
    }
  else if (window->net_wm_pid != -1)
    {
      /* We know the process which owns this window; perhaps we can
       * find out the name of its owner (if it's not us).
       */

      char *found_name = NULL;

      uid_t window_owner = 0;
      gboolean window_owner_known =
              owner_of_process (window->net_wm_pid, &window_owner);

      /* Assume a window with unknown ownership is ours (call it usufruct!) */
      gboolean window_owner_is_us =
              !window_owner_known || window_owner==getuid ();

      if (window_owner_is_us)
        {
          /* we own it, so fall back to the simple case */
          *target = g_strdup (title);
        }
      else
        {
          /* it belongs to window_owner.  So what's their name? */

          if (window_owner==0)
            {
              /* Simple case-- don't bother to look it up.  It's root. */
              *target = g_strdup_printf (_("%s (as superuser)"),
                                         title);
            }
          else
            {
              /* Okay, let's look up the name. */
              struct passwd *pwd;

              errno = 0;
              pwd = getpwuid (window_owner);
              if (errno==0 && pwd!=NULL)
                {
                  found_name = pwd->pw_name;
                }

              if (found_name)
                /* Translators: the title of a window owned by another user
                 * on this machine */
                *target = g_strdup_printf (_("%s (as %s)"),
                                           title,
                                           found_name);
              else
                /* Translators: the title of a window owned by another user
                 * on this machine, whose name we don't know */
                *target = g_strdup_printf (_("%s (as another user)"),
                                           title);
            }
          /* either way we changed it */
          modified = TRUE;

        }
    }
  else
    *target = g_strdup (title);

  if (modified && atom != None)
    meta_prop_set_utf8_string_hint (window->display,
                                    window->xwindow,
                                    atom, *target);

  /* Bug 330671 -- Don't forget to clear _NET_WM_VISIBLE_(ICON_)NAME */
  if (!modified && previous_was_modified)
    {
      meta_error_trap_push (window->display);
      XDeleteProperty (window->display->xdisplay,
                       window->xwindow,
                       atom);
      meta_error_trap_pop (window->display, FALSE);
    }

  return modified;
}

static void
set_window_title (MetaWindow *window,
                  const char *title)
{
  char *str;

  gboolean modified =
    set_title_text (window,
                    window->using_net_wm_visible_name,
                    title,
                    window->display->atom__NET_WM_VISIBLE_NAME,
                    &window->title);
  window->using_net_wm_visible_name = (modified != FALSE);

  /* strndup is a hack since GNU libc has broken %.10s */
  str = g_strndup (window->title, 10);
  g_free (window->desc);
  window->desc = g_strdup_printf ("0x%lx (%s)", window->xwindow, str);
  g_free (str);

  if (window->frame)
    meta_ui_set_frame_title (window->screen->ui,
                             window->frame->xwindow,
                             window->title);
}

static void
reload_net_wm_name (MetaWindow    *window,
                    MetaPropValue *value,
                    gboolean       initial)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      set_window_title (window, value->v.str);
      window->using_net_wm_name = TRUE;

      meta_verbose ("Using _NET_WM_NAME for new title of %s: \"%s\"\n",
                    window->desc, window->title);
    }
  else
    {
      set_window_title (window, NULL);
      window->using_net_wm_name = FALSE;
      if (!initial)
        meta_window_reload_property (window, XA_WM_NAME, FALSE);
    }
}

static void
reload_wm_name (MetaWindow    *window,
                MetaPropValue *value,
                gboolean       initial)
{
  if (window->using_net_wm_name)
    {
      meta_verbose ("Ignoring WM_NAME \"%s\" as _NET_WM_NAME is set\n",
                    value->v.str);
      return;
    }

  if (value->type != META_PROP_VALUE_INVALID)
    {
      set_window_title (window, value->v.str);

      meta_verbose ("Using WM_NAME for new title of %s: \"%s\"\n",
                    window->desc, window->title);
    }
  else
    {
      set_window_title (window, NULL);
    }
}

static void
set_icon_title (MetaWindow *window,
                const char *title)
{
  gboolean modified =
    set_title_text (window,
                    window->using_net_wm_visible_icon_name,
                    title,
                    window->display->atom__NET_WM_VISIBLE_ICON_NAME,
                    &window->icon_name);
  window->using_net_wm_visible_icon_name = (modified != FALSE);
}

static void
reload_net_wm_icon_name (MetaWindow    *window,
                         MetaPropValue *value,
                         gboolean       initial)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      set_icon_title (window, value->v.str);
      window->using_net_wm_icon_name = TRUE;

      meta_verbose ("Using _NET_WM_ICON_NAME for new title of %s: \"%s\"\n",
                    window->desc, window->title);
    }
  else
    {
      set_icon_title (window, NULL);
      window->using_net_wm_icon_name = FALSE;
      if (!initial)
        meta_window_reload_property (window, XA_WM_ICON_NAME, FALSE);
    }
}

static void
reload_wm_icon_name (MetaWindow    *window,
                     MetaPropValue *value,
                     gboolean       initial)
{
  if (window->using_net_wm_icon_name)
    {
      meta_verbose ("Ignoring WM_ICON_NAME \"%s\" as _NET_WM_ICON_NAME is set\n",
                    value->v.str);
      return;
    }

  if (value->type != META_PROP_VALUE_INVALID)
    {
      set_icon_title (window, value->v.str);

      meta_verbose ("Using WM_ICON_NAME for new title of %s: \"%s\"\n",
                    window->desc, window->title);
    }
  else
    {
      set_icon_title (window, NULL);
    }
}

static void
reload_net_wm_state (MetaWindow    *window,
                     MetaPropValue *value,
                     gboolean       initial)
{
  int i;

  /* We know this is only an initial window creation,
   * clients don't change the property.
   */

  if (!initial) {
    /* no, they DON'T change the property */
    meta_verbose ("Ignoring _NET_WM_STATE: we should be the one who set "
                  "the property in the first place\n");
    return;
  }

  window->shaded = FALSE;
  window->maximized_horizontally = FALSE;
  window->maximized_vertically = FALSE;
  window->fullscreen = FALSE;
  window->wm_state_modal = FALSE;
  window->wm_state_skip_taskbar = FALSE;
  window->wm_state_skip_pager = FALSE;
  window->wm_state_above = FALSE;
  window->wm_state_below = FALSE;
  window->wm_state_demands_attention = FALSE;

  if (value->type == META_PROP_VALUE_INVALID)
    return;

  i = 0;
  while (i < value->v.atom_list.n_atoms)
    {
      if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_SHADED)
        window->shaded = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_MAXIMIZED_HORZ)
        window->maximize_horizontally_after_placement = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_MAXIMIZED_VERT)
        window->maximize_vertically_after_placement = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_HIDDEN)
        window->minimize_after_placement = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_MODAL)
        window->wm_state_modal = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_SKIP_TASKBAR)
        window->wm_state_skip_taskbar = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_SKIP_PAGER)
        window->wm_state_skip_pager = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_FULLSCREEN)
        window->fullscreen_after_placement = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_ABOVE)
        window->wm_state_above = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_BELOW)
        window->wm_state_below = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_DEMANDS_ATTENTION)
        window->wm_state_demands_attention = TRUE;
      else if (value->v.atom_list.atoms[i] == window->display->atom__NET_WM_STATE_STICKY)
        window->on_all_workspaces = TRUE;

      ++i;
    }

  meta_verbose ("Reloaded _NET_WM_STATE for %s\n",
                window->desc);

  meta_window_recalc_window_type (window);
}

static void
reload_mwm_hints (MetaWindow    *window,
                  MetaPropValue *value,
                  gboolean       initial)
{
  MotifWmHints *hints;

  window->mwm_decorated = TRUE;
  window->mwm_border_only = FALSE;
  window->mwm_has_close_func = TRUE;
  window->mwm_has_minimize_func = TRUE;
  window->mwm_has_maximize_func = TRUE;
  window->mwm_has_move_func = TRUE;
  window->mwm_has_resize_func = TRUE;

  if (value->type == META_PROP_VALUE_INVALID)
    {
      meta_verbose ("Window %s has no MWM hints\n", window->desc);
      meta_window_recalc_features (window);
      return;
    }

  hints = value->v.motif_hints;

  /* We support those MWM hints deemed non-stupid */

  meta_verbose ("Window %s has MWM hints\n",
                window->desc);

  if (hints->flags & MWM_HINTS_DECORATIONS)
    {
      meta_verbose ("Window %s sets MWM_HINTS_DECORATIONS 0x%lx\n",
          window->desc, hints->decorations);

      if (hints->decorations == 0)
        window->mwm_decorated = FALSE;
      /* some input methods use this */
      else if (hints->decorations == MWM_DECOR_BORDER)
        window->mwm_border_only = TRUE;
    }
  else
    meta_verbose ("Decorations flag unset\n");

  if (hints->flags & MWM_HINTS_FUNCTIONS)
    {
      gboolean toggle_value;

      meta_verbose ("Window %s sets MWM_HINTS_FUNCTIONS 0x%lx\n",
                    window->desc, hints->functions);

      /* If _ALL is specified, then other flags indicate what to turn off;
       * if ALL is not specified, flags are what to turn on.
       * at least, I think so
       */

      if ((hints->functions & MWM_FUNC_ALL) == 0)
        {
          toggle_value = TRUE;

          meta_verbose ("Window %s disables all funcs then reenables some\n",
                        window->desc);
          window->mwm_has_close_func = FALSE;
          window->mwm_has_minimize_func = FALSE;
          window->mwm_has_maximize_func = FALSE;
          window->mwm_has_move_func = FALSE;
          window->mwm_has_resize_func = FALSE;
        }
      else
        {
          meta_verbose ("Window %s enables all funcs then disables some\n",
                        window->desc);
          toggle_value = FALSE;
        }

      if ((hints->functions & MWM_FUNC_CLOSE) != 0)
        {
          meta_verbose ("Window %s toggles close via MWM hints\n",
                        window->desc);
          window->mwm_has_close_func = (toggle_value != FALSE);
        }
      if ((hints->functions & MWM_FUNC_MINIMIZE) != 0)
        {
          meta_verbose ("Window %s toggles minimize via MWM hints\n",
                        window->desc);
          window->mwm_has_minimize_func = (toggle_value != FALSE);
        }
      if ((hints->functions & MWM_FUNC_MAXIMIZE) != 0)
        {
          meta_verbose ("Window %s toggles maximize via MWM hints\n",
                        window->desc);
          window->mwm_has_maximize_func = (toggle_value != FALSE);
        }
      if ((hints->functions & MWM_FUNC_MOVE) != 0)
        {
          meta_verbose ("Window %s toggles move via MWM hints\n",
                        window->desc);
          window->mwm_has_move_func = (toggle_value != FALSE);
        }
      if ((hints->functions & MWM_FUNC_RESIZE) != 0)
        {
          meta_verbose ("Window %s toggles resize via MWM hints\n",
                        window->desc);
          window->mwm_has_resize_func = (toggle_value != FALSE);
        }
    }
  else
    meta_verbose ("Functions flag unset\n");

  meta_window_recalc_features (window);

  /* We do all this anyhow at the end of meta_window_new() */
  if (!window->constructing)
    {
      if (window->decorated)
        meta_window_ensure_frame (window);
      else
        meta_window_destroy_frame (window);

      meta_window_queue (window,
                         META_QUEUE_MOVE_RESIZE |
                         /* because ensure/destroy frame may unmap: */
                         META_QUEUE_CALC_SHOWING);
    }
}

static void
reload_wm_class (MetaWindow    *window,
                 MetaPropValue *value,
                 gboolean       initial)
{
  if (window->res_class)
    g_free (window->res_class);
  if (window->res_name)
    g_free (window->res_name);

  window->res_class = NULL;
  window->res_name = NULL;

  if (value->type != META_PROP_VALUE_INVALID)
    {
      if (value->v.class_hint.res_name)
        window->res_name = g_strdup (value->v.class_hint.res_name);

      if (value->v.class_hint.res_class)
        window->res_class = g_strdup (value->v.class_hint.res_class);
    }

  meta_verbose ("Window %s class: '%s' name: '%s'\n",
      window->desc,
      window->res_class ? window->res_class : "none",
      window->res_name ? window->res_name : "none");
}

static void
reload_net_wm_desktop (MetaWindow    *window,
                       MetaPropValue *value,
                       gboolean       initial)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      window->initial_workspace_set = TRUE;
      window->initial_workspace = value->v.cardinal;
      meta_topic (META_DEBUG_PLACEMENT,
                  "Read initial workspace prop %d for %s\n",
                  window->initial_workspace, window->desc);
    }
}

static void
reload_net_startup_id (MetaWindow    *window,
                       MetaPropValue *value,
                       gboolean       initial)
{
  guint32 timestamp = window->net_wm_user_time;
  MetaWorkspace *workspace = NULL;

  g_free (window->startup_id);

  if (value->type != META_PROP_VALUE_INVALID)
    window->startup_id = g_strdup (value->v.str);
  else
    window->startup_id = NULL;

  /* Update timestamp and workspace on a running window */
  if (!window->constructing)
  {
    window->initial_timestamp_set = 0;
    window->initial_workspace_set = 0;

    if (meta_screen_apply_startup_properties (window->screen, window))
      {

        if (window->initial_timestamp_set)
          timestamp = window->initial_timestamp;
        if (window->initial_workspace_set)
          workspace = meta_screen_get_workspace_by_index (window->screen, window->initial_workspace);

        meta_window_activate_with_workspace (window, timestamp, workspace);
      }
  }

  meta_verbose ("New _NET_STARTUP_ID \"%s\" for %s\n",
                window->startup_id ? window->startup_id : "unset",
                window->desc);
}

static void
reload_update_counter (MetaWindow    *window,
                       MetaPropValue *value,
                       gboolean       initial)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
#ifdef HAVE_XSYNC
      XSyncCounter counter = value->v.xcounter;

      window->sync_request_counter = counter;
      meta_verbose ("Window has _NET_WM_SYNC_REQUEST_COUNTER 0x%lx\n",
                    window->sync_request_counter);
#endif
    }
}

#define FLAG_TOGGLED_ON(old,new,flag) \
 (((old)->flags & (flag)) == 0 &&     \
  ((new)->flags & (flag)) != 0)

#define FLAG_TOGGLED_OFF(old,new,flag) \
 (((old)->flags & (flag)) != 0 &&      \
  ((new)->flags & (flag)) == 0)

#define FLAG_CHANGED(old,new,flag) \
  (FLAG_TOGGLED_ON(old,new,flag) || FLAG_TOGGLED_OFF(old,new,flag))

static void
spew_size_hints_differences (const XSizeHints *old,
                             const XSizeHints *new)
{
  if (FLAG_CHANGED (old, new, USPosition))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: USPosition now %s\n",
                FLAG_TOGGLED_ON (old, new, USPosition) ? "set" : "unset");
  if (FLAG_CHANGED (old, new, USSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: USSize now %s\n",
                FLAG_TOGGLED_ON (old, new, USSize) ? "set" : "unset");
  if (FLAG_CHANGED (old, new, PPosition))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PPosition now %s\n",
                FLAG_TOGGLED_ON (old, new, PPosition) ? "set" : "unset");
  if (FLAG_CHANGED (old, new, PSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PSize now %s\n",
                FLAG_TOGGLED_ON (old, new, PSize) ? "set" : "unset");
  if (FLAG_CHANGED (old, new, PMinSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PMinSize now %s (%d x %d -> %d x %d)\n",
                FLAG_TOGGLED_ON (old, new, PMinSize) ? "set" : "unset",
                old->min_width, old->min_height,
                new->min_width, new->min_height);
  if (FLAG_CHANGED (old, new, PMaxSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PMaxSize now %s (%d x %d -> %d x %d)\n",
                FLAG_TOGGLED_ON (old, new, PMaxSize) ? "set" : "unset",
                old->max_width, old->max_height,
                new->max_width, new->max_height);
  if (FLAG_CHANGED (old, new, PResizeInc))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PResizeInc now %s (width_inc %d -> %d height_inc %d -> %d)\n",
                FLAG_TOGGLED_ON (old, new, PResizeInc) ? "set" : "unset",
                old->width_inc, new->width_inc,
                old->height_inc, new->height_inc);
  if (FLAG_CHANGED (old, new, PAspect))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PAspect now %s (min %d/%d -> %d/%d max %d/%d -> %d/%d)\n",
                FLAG_TOGGLED_ON (old, new, PAspect) ? "set" : "unset",
                old->min_aspect.x, old->min_aspect.y,
                new->min_aspect.x, new->min_aspect.y,
                old->max_aspect.x, old->max_aspect.y,
                new->max_aspect.x, new->max_aspect.y);
  if (FLAG_CHANGED (old, new, PBaseSize))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PBaseSize now %s (%d x %d -> %d x %d)\n",
                FLAG_TOGGLED_ON (old, new, PBaseSize) ? "set" : "unset",
                old->base_width, old->base_height,
                new->base_width, new->base_height);
  if (FLAG_CHANGED (old, new, PWinGravity))
    meta_topic (META_DEBUG_GEOMETRY, "XSizeHints: PWinGravity now %s  (%d -> %d)\n",
                FLAG_TOGGLED_ON (old, new, PWinGravity) ? "set" : "unset",
                old->win_gravity, new->win_gravity);
}

void
meta_set_normal_hints (MetaWindow *window,
                       XSizeHints *hints)
{
  int x, y, w, h;
  double minr, maxr;
  /* Some convenience vars */
  int minw, minh, maxw, maxh;   /* min/max width/height                      */
  int basew, baseh, winc, hinc; /* base width/height, width/height increment */

  /* Save the last ConfigureRequest, which we put here.
   * Values here set in the hints are supposed to
   * be ignored.
   */
  x = window->size_hints.x;
  y = window->size_hints.y;
  w = window->size_hints.width;
  h = window->size_hints.height;

  /* as far as I can tell, value->v.size_hints.flags is just to
   * check whether we had old-style normal hints without gravity,
   * base size as returned by XGetNormalHints(), so we don't
   * really use it as we fixup window->size_hints to have those
   * fields if they're missing.
   */

  /*
   * When the window is first created, NULL hints will
   * be passed in which will initialize all of the fields
   * as if flags were zero
   */
  if (hints)
    window->size_hints = *hints;
  else
    window->size_hints.flags = 0;

  /* Put back saved ConfigureRequest. */
  window->size_hints.x = x;
  window->size_hints.y = y;
  window->size_hints.width = w;
  window->size_hints.height = h;

  /* Get base size hints */
  if (window->size_hints.flags & PBaseSize)
    {
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets base size %d x %d\n",
                  window->desc,
                  window->size_hints.base_width,
                  window->size_hints.base_height);
    }
  else if (window->size_hints.flags & PMinSize)
    {
      window->size_hints.base_width = window->size_hints.min_width;
      window->size_hints.base_height = window->size_hints.min_height;
    }
  else
    {
      window->size_hints.base_width = 0;
      window->size_hints.base_height = 0;
    }
  window->size_hints.flags |= PBaseSize;

  /* Get min size hints */
  if (window->size_hints.flags & PMinSize)
    {
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets min size %d x %d\n",
                  window->desc,
                  window->size_hints.min_width,
                  window->size_hints.min_height);
    }
  else if (window->size_hints.flags & PBaseSize)
    {
      window->size_hints.min_width = window->size_hints.base_width;
      window->size_hints.min_height = window->size_hints.base_height;
    }
  else
    {
      window->size_hints.min_width = 0;
      window->size_hints.min_height = 0;
    }
  window->size_hints.flags |= PMinSize;

  /* Get max size hints */
  if (window->size_hints.flags & PMaxSize)
    {
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets max size %d x %d\n",
                  window->desc,
                  window->size_hints.max_width,
                  window->size_hints.max_height);
    }
  else
    {
      window->size_hints.max_width = G_MAXINT;
      window->size_hints.max_height = G_MAXINT;
      window->size_hints.flags |= PMaxSize;
    }

  /* Get resize increment hints */
  if (window->size_hints.flags & PResizeInc)
    {
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets resize width inc: %d height inc: %d\n",
                  window->desc,
                  window->size_hints.width_inc,
                  window->size_hints.height_inc);
    }
  else
    {
      window->size_hints.width_inc = 1;
      window->size_hints.height_inc = 1;
      window->size_hints.flags |= PResizeInc;
    }

  /* Get aspect ratio hints */
  if (window->size_hints.flags & PAspect)
    {
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets min_aspect: %d/%d max_aspect: %d/%d\n",
                  window->desc,
                  window->size_hints.min_aspect.x,
                  window->size_hints.min_aspect.y,
                  window->size_hints.max_aspect.x,
                  window->size_hints.max_aspect.y);
    }
  else
    {
      window->size_hints.min_aspect.x = 1;
      window->size_hints.min_aspect.y = G_MAXINT;
      window->size_hints.max_aspect.x = G_MAXINT;
      window->size_hints.max_aspect.y = 1;
      window->size_hints.flags |= PAspect;
    }

  /* Get gravity hint */
  if (window->size_hints.flags & PWinGravity)
    {
      meta_topic (META_DEBUG_GEOMETRY, "Window %s sets gravity %d\n",
                  window->desc,
                  window->size_hints.win_gravity);
    }
  else
    {
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s doesn't set gravity, using NW\n",
                  window->desc);
      window->size_hints.win_gravity = NorthWestGravity;
      window->size_hints.flags |= PWinGravity;
    }

  /*** Lots of sanity checking ***/

  /* Verify all min & max hints are at least 1 pixel */
  if (window->size_hints.min_width < 1)
    {
      /* someone is on crack */
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets min width to 0, which makes no sense\n",
                  window->desc);
      window->size_hints.min_width = 1;
    }
  if (window->size_hints.max_width < 1)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets max width to 0, which makes no sense\n",
                  window->desc);
      window->size_hints.max_width = 1;
    }
  if (window->size_hints.min_height < 1)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets min height to 0, which makes no sense\n",
                  window->desc);
      window->size_hints.min_height = 1;
    }
  if (window->size_hints.max_height < 1)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets max height to 0, which makes no sense\n",
                  window->desc);
      window->size_hints.max_height = 1;
    }

  /* Verify size increment hints are at least 1 pixel */
  if (window->size_hints.width_inc < 1)
    {
      /* app authors find so many ways to smoke crack */
      window->size_hints.width_inc = 1;
      meta_topic (META_DEBUG_GEOMETRY, "Corrected 0 width_inc to 1\n");
    }
  if (window->size_hints.height_inc < 1)
    {
      /* another cracksmoker */
      window->size_hints.height_inc = 1;
      meta_topic (META_DEBUG_GEOMETRY, "Corrected 0 height_inc to 1\n");
    }
  /* divide by 0 cracksmokers; note that x & y in (min|max)_aspect are
   * numerator & denominator
   */
  if (window->size_hints.min_aspect.y < 1)
    window->size_hints.min_aspect.y = 1;
  if (window->size_hints.max_aspect.y < 1)
    window->size_hints.max_aspect.y = 1;

  minw  = window->size_hints.min_width;  minh  = window->size_hints.min_height;
  maxw  = window->size_hints.max_width;  maxh  = window->size_hints.max_height;
  basew = window->size_hints.base_width; baseh = window->size_hints.base_height;
  winc  = window->size_hints.width_inc;  hinc  = window->size_hints.height_inc;

  /* Make sure min and max size hints are consistent with the base + increment
   * size hints.  If they're not, it's not a real big deal, but it means the
   * effective min and max size are more restrictive than the application
   * specified values.
   */
  if ((minw - basew) % winc != 0)
    {
      /* Take advantage of integer division throwing away the remainder... */
      window->size_hints.min_width = basew + ((minw - basew)/winc + 1)*winc;

      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s has width_inc (%d) that does not evenly divide "
                  "min_width - base_width (%d - %d); thus effective "
                  "min_width is really %d\n",
                  window->desc,
                  winc, minw, basew, window->size_hints.min_width);
      minw = window->size_hints.min_width;
    }
  if (maxw != G_MAXINT && (maxw - basew) % winc != 0)
    {
      /* Take advantage of integer division throwing away the remainder... */
      window->size_hints.max_width = basew + ((maxw - basew)/winc)*winc;

      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s has width_inc (%d) that does not evenly divide "
                  "max_width - base_width (%d - %d); thus effective "
                  "max_width is really %d\n",
                  window->desc,
                  winc, maxw, basew, window->size_hints.max_width);
      maxw = window->size_hints.max_width;
    }
  if ((minh - baseh) % hinc != 0)
    {
      /* Take advantage of integer division throwing away the remainder... */
      window->size_hints.min_height = baseh + ((minh - baseh)/hinc + 1)*hinc;

      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s has height_inc (%d) that does not evenly divide "
                  "min_height - base_height (%d - %d); thus effective "
                  "min_height is really %d\n",
                  window->desc,
                  hinc, minh, baseh, window->size_hints.min_height);
      minh = window->size_hints.min_height;
    }
  if (maxh != G_MAXINT && (maxh - baseh) % hinc != 0)
    {
      /* Take advantage of integer division throwing away the remainder... */
      window->size_hints.max_height = baseh + ((maxh - baseh)/hinc)*hinc;

      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s has height_inc (%d) that does not evenly divide "
                  "max_height - base_height (%d - %d); thus effective "
                  "max_height is really %d\n",
                  window->desc,
                  hinc, maxh, baseh, window->size_hints.max_height);
      maxh = window->size_hints.max_height;
    }

  /* make sure maximum size hints are compatible with minimum size hints; min
   * size hints take precedence.
   */
  if (window->size_hints.max_width < window->size_hints.min_width)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets max width %d less than min width %d, "
                  "disabling resize\n",
                  window->desc,
                  window->size_hints.max_width,
                  window->size_hints.min_width);
      maxw = window->size_hints.max_width = window->size_hints.min_width;
    }
  if (window->size_hints.max_height < window->size_hints.min_height)
    {
      /* another cracksmoker */
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets max height %d less than min height %d, "
                  "disabling resize\n",
                  window->desc,
                  window->size_hints.max_height,
                  window->size_hints.min_height);
      maxh = window->size_hints.max_height = window->size_hints.min_height;
    }

  /* Make sure the aspect ratio hints are sane. */
  minr =         window->size_hints.min_aspect.x /
         (double)window->size_hints.min_aspect.y;
  maxr =         window->size_hints.max_aspect.x /
         (double)window->size_hints.max_aspect.y;
  if (minr > maxr)
    {
      /* another cracksmoker; not even minimally (self) consistent */
      meta_topic (META_DEBUG_GEOMETRY,
                  "Window %s sets min aspect ratio larger than max aspect "
                  "ratio; disabling aspect ratio constraints.\n",
                  window->desc);
      window->size_hints.min_aspect.x = 1;
      window->size_hints.min_aspect.y = G_MAXINT;
      window->size_hints.max_aspect.x = G_MAXINT;
      window->size_hints.max_aspect.y = 1;
    }
  else /* check consistency of aspect ratio hints with other hints */
    {
      if (minh > 0 && minr > (maxw / (double)minh))
        {
          /* another cracksmoker */
          meta_topic (META_DEBUG_GEOMETRY,
                      "Window %s sets min aspect ratio larger than largest "
                      "aspect ratio possible given min/max size constraints; "
                      "disabling min aspect ratio constraint.\n",
                      window->desc);
          window->size_hints.min_aspect.x = 1;
          window->size_hints.min_aspect.y = G_MAXINT;
        }
      if (maxr < (minw / (double)maxh))
        {
          /* another cracksmoker */
          meta_topic (META_DEBUG_GEOMETRY,
                      "Window %s sets max aspect ratio smaller than smallest "
                      "aspect ratio possible given min/max size constraints; "
                      "disabling max aspect ratio constraint.\n",
                      window->desc);
          window->size_hints.max_aspect.x = G_MAXINT;
          window->size_hints.max_aspect.y = 1;
        }
      /* FIXME: Would be nice to check that aspect ratios are
       * consistent with base and size increment constraints.
       */
    }
}

static void
reload_normal_hints (MetaWindow    *window,
                     MetaPropValue *value,
                     gboolean       initial)
{
  if (value->type != META_PROP_VALUE_INVALID)
    {
      XSizeHints old_hints;

      meta_topic (META_DEBUG_GEOMETRY, "Updating WM_NORMAL_HINTS for %s\n", window->desc);

      old_hints = window->size_hints;

      meta_set_normal_hints (window, value->v.size_hints.hints);

      spew_size_hints_differences (&old_hints, &window->size_hints);

      meta_window_recalc_features (window);

      if (!initial)
        meta_window_queue(window, META_QUEUE_MOVE_RESIZE);
    }
}

static void
reload_wm_protocols (MetaWindow    *window,
                     MetaPropValue *value,
                     gboolean       initial)
{
  int i;

  window->take_focus = FALSE;
  window->delete_window = FALSE;
  window->net_wm_ping = FALSE;

  if (value->type == META_PROP_VALUE_INVALID)
    return;

  i = 0;
  while (i < value->v.atom_list.n_atoms)
    {
      if (value->v.atom_list.atoms[i] ==
          window->display->atom_WM_TAKE_FOCUS)
        window->take_focus = TRUE;
      else if (value->v.atom_list.atoms[i] ==
               window->display->atom_WM_DELETE_WINDOW)
        window->delete_window = TRUE;
      else if (value->v.atom_list.atoms[i] ==
               window->display->atom__NET_WM_PING)
        window->net_wm_ping = TRUE;
      ++i;
    }

  meta_verbose ("New _NET_STARTUP_ID \"%s\" for %s\n",
                window->startup_id ? window->startup_id : "unset",
                window->desc);
}

static void
reload_wm_hints (MetaWindow    *window,
                 MetaPropValue *value,
                 gboolean       initial)
{
  Window old_group_leader;

  old_group_leader = window->xgroup_leader;

  /* Fill in defaults */
  window->input = TRUE;
  window->initially_iconic = FALSE;
  window->xgroup_leader = None;
  window->wm_hints_pixmap = None;
  window->wm_hints_mask = None;

  if (value->type != META_PROP_VALUE_INVALID)
    {
      const XWMHints *hints = value->v.wm_hints;

      if (hints->flags & InputHint)
        window->input = hints->input;

      if (hints->flags & StateHint)
        window->initially_iconic = (hints->initial_state == IconicState);

      if (hints->flags & WindowGroupHint)
        window->xgroup_leader = hints->window_group;

      if (hints->flags & IconPixmapHint)
        window->wm_hints_pixmap = hints->icon_pixmap;

      if (hints->flags & IconMaskHint)
        window->wm_hints_mask = hints->icon_mask;

      meta_verbose ("Read WM_HINTS input: %d iconic: %d group leader: 0x%lx pixmap: 0x%lx mask: 0x%lx\n",
                    window->input, window->initially_iconic,
                    window->xgroup_leader,
                    window->wm_hints_pixmap,
                    window->wm_hints_mask);
    }

  if (window->xgroup_leader != old_group_leader)
    {
      meta_verbose ("Window %s changed its group leader to 0x%lx\n",
                    window->desc, window->xgroup_leader);

      meta_window_group_leader_changed (window);
    }

  meta_icon_cache_property_changed (&window->icon_cache,
                                    window->display,
                                    XA_WM_HINTS);

  meta_window_queue (window, META_QUEUE_UPDATE_ICON | META_QUEUE_MOVE_RESIZE);
}

static void
reload_transient_for (MetaWindow    *window,
                      MetaPropValue *value,
                      gboolean       initial)
{
  window->xtransient_for = None;

  if (value->type != META_PROP_VALUE_INVALID)
    window->xtransient_for = value->v.xwindow;

  /* Make sure transient_for is valid */
  if (window->xtransient_for != None &&
      meta_display_lookup_x_window (window->display,
                                    window->xtransient_for) == NULL)
    {
      meta_warning (_("Invalid WM_TRANSIENT_FOR window 0x%lx specified "
                      "for %s.\n"),
                    window->xtransient_for, window->desc);
      window->xtransient_for = None;
    }

  window->transient_parent_is_root_window =
    window->xtransient_for == window->screen->xroot;

  if (window->xtransient_for != None)
    meta_verbose ("Window %s transient for 0x%lx (root = %d)\n", window->desc,
        window->xtransient_for, window->transient_parent_is_root_window);
  else
    meta_verbose ("Window %s is not transient\n", window->desc);

  /* may now be a dialog */
  meta_window_recalc_window_type (window);

  /* update stacking constraints */
  meta_stack_update_transient (window->screen->stack, window);

  /* possibly change its group. We treat being a window's transient as
   * equivalent to making it your group leader, to work around shortcomings
   * in programs such as xmms-- see #328211.
   */
  if (window->xtransient_for != None &&
      window->xgroup_leader != None &&
      window->xtransient_for != window->xgroup_leader)
    meta_window_group_leader_changed (window);

  if (!window->constructing)
    meta_window_queue (window, META_QUEUE_MOVE_RESIZE);
}

static void
reload_gtk_theme_variant (MetaWindow    *window,
                          MetaPropValue *value,
                          gboolean       initial)
{
  char *requested_variant = NULL;
  char *current_variant = window->gtk_theme_variant;

  if (value->type != META_PROP_VALUE_INVALID)
    {
      requested_variant = value->v.str;
      meta_verbose ("Requested \"%s\" theme variant for window %s.\n",
                    requested_variant, window->desc);
    }

  if (g_strcmp0 (requested_variant, current_variant) != 0)
    {
      g_free (current_variant);

      window->gtk_theme_variant = g_strdup (requested_variant);
      if (window->frame)
        meta_ui_update_frame_style (window->screen->ui, window->frame->xwindow);
    }
}

static void
reload_gtk_application_id (MetaWindow    *window,
                           MetaPropValue *value,
                           gboolean       initial)
{
  char *requested = NULL;
  char *current = window->gtk_application_id;

  if (value->type != META_PROP_VALUE_INVALID)
    {
      requested = value->v.str;
      meta_verbose ("Requested \"%s\" gtk-application-id for window %s.\n",
                    requested, window->desc);
    }

  if (g_strcmp0 (requested, current) != 0)
    {
      g_free (current);

      window->gtk_application_id = g_strdup (requested);
    }
}

static void
reload_bamf_desktop_file (MetaWindow    *window,
                          MetaPropValue *value,
                          gboolean       initial)
{
  char *requested = NULL;
  char *current = window->bamf_desktop_file;

  if (value->type != META_PROP_VALUE_INVALID)
    {
      requested = value->v.str;
      meta_verbose ("Requested _BAMF_DESKTOP_FILE \"%s\" for window %s.\n",
                    requested, window->desc);
    }

  if (g_strcmp0 (requested, current) != 0)
    {
      g_free (current);

      window->bamf_desktop_file = g_strdup (requested);
    }
}

/**
 * Initialises the property hooks system.  Each row in the table named "hooks"
 * represents an action to take when a property is found on a newly-created
 * window, or when a property changes its value.
 *
 * The first column shows which atom the row concerns.
 * The second gives the type of the property data.  The property will be
 * queried for its new value, unless the type is given as
 * META_PROP_VALUE_INVALID, in which case nothing will be queried.
 * The third column gives the name of a callback which gets called with the
 * new value.  (If the new value was not retrieved because the second column
 * was META_PROP_VALUE_INVALID, the callback still gets called anyway.)
 * This value may be NULL, in which case no callback will be called.
 */
void
meta_display_init_window_prop_hooks (MetaDisplay *display)
{
  MetaWindowPropHooks hooks[] = {
    { display->atom_WM_STATE,          META_PROP_VALUE_INVALID,  NULL },
    { display->atom_WM_CLIENT_MACHINE, META_PROP_VALUE_STRING,   reload_wm_client_machine },
    { display->atom__NET_WM_PID,       META_PROP_VALUE_CARDINAL, reload_net_wm_pid },
    { display->atom__NET_WM_USER_TIME, META_PROP_VALUE_CARDINAL, reload_net_wm_user_time },
    { display->atom__NET_WM_NAME,      META_PROP_VALUE_UTF8,     reload_net_wm_name },
    { XA_WM_NAME,                      META_PROP_VALUE_TEXT_PROPERTY, reload_wm_name },
    { display->atom__NET_WM_ICON,      META_PROP_VALUE_INVALID,  reload_net_wm_icon },
    { display->atom__KWM_WIN_ICON,     META_PROP_VALUE_INVALID,  reload_kwm_win_icon },
    { display->atom__NET_WM_ICON_NAME, META_PROP_VALUE_UTF8,     reload_net_wm_icon_name },
    { XA_WM_ICON_NAME,                 META_PROP_VALUE_TEXT_PROPERTY, reload_wm_icon_name },
    { display->atom__NET_WM_STATE,     META_PROP_VALUE_ATOM_LIST, reload_net_wm_state },
    { display->atom__MOTIF_WM_HINTS,   META_PROP_VALUE_MOTIF_HINTS, reload_mwm_hints },
    { display->atom__NET_WM_ICON_GEOMETRY, META_PROP_VALUE_INVALID, NULL },
    { XA_WM_CLASS,                     META_PROP_VALUE_CLASS_HINT, reload_wm_class },
    { display->atom_WM_CLIENT_LEADER,  META_PROP_VALUE_INVALID, complain_about_broken_client },
    { display->atom_SM_CLIENT_ID,      META_PROP_VALUE_INVALID, complain_about_broken_client },
    { display->atom_WM_WINDOW_ROLE,    META_PROP_VALUE_INVALID, reload_wm_window_role },
    { display->atom__NET_WM_WINDOW_TYPE, META_PROP_VALUE_INVALID, reload_net_wm_window_type },
    { display->atom__NET_WM_DESKTOP,   META_PROP_VALUE_CARDINAL, reload_net_wm_desktop },
    { display->atom__NET_WM_STRUT,         META_PROP_VALUE_INVALID, reload_struts },
    { display->atom__NET_WM_STRUT_PARTIAL, META_PROP_VALUE_INVALID, reload_struts },
    { display->atom__GNOME_WM_STRUT_AREA, META_PROP_VALUE_INVALID, reload_struts },
    { display->atom__NET_STARTUP_ID,  META_PROP_VALUE_UTF8,     reload_net_startup_id },
    { display->atom__NET_WM_SYNC_REQUEST_COUNTER, META_PROP_VALUE_SYNC_COUNTER, reload_update_counter },
    { XA_WM_NORMAL_HINTS,              META_PROP_VALUE_SIZE_HINTS, reload_normal_hints },
    { display->atom_WM_PROTOCOLS,      META_PROP_VALUE_ATOM_LIST, reload_wm_protocols },
    { XA_WM_HINTS,                     META_PROP_VALUE_WM_HINTS,  reload_wm_hints },
    { XA_WM_TRANSIENT_FOR,             META_PROP_VALUE_WINDOW,    reload_transient_for },
    { display->atom__NET_WM_USER_TIME_WINDOW, META_PROP_VALUE_WINDOW, reload_net_wm_user_time_window },
    { display->atom__GTK_THEME_VARIANT, META_PROP_VALUE_UTF8, reload_gtk_theme_variant, },
    { display->atom__GTK_FRAME_EXTENTS, META_PROP_VALUE_CARDINAL_LIST, reload_gtk_frame_extents },
    { display->atom__GTK_APPLICATION_ID, META_PROP_VALUE_UTF8, reload_gtk_application_id },
    { display->atom__BAMF_DESKTOP_FILE, META_PROP_VALUE_STRING, reload_bamf_desktop_file },
    { 0 },
  };

  MetaWindowPropHooks *table, *cursor;

#if GLIB_CHECK_VERSION (2, 68, 0)
  table = g_memdup2 (hooks, sizeof (hooks));
#else
  table = g_memdup (hooks, sizeof (hooks));
#endif
  cursor = table;

  g_assert (display->prop_hooks == NULL);

  display->prop_hooks_table = (gpointer) table;
  display->prop_hooks = g_hash_table_new (NULL, NULL);

  while (cursor->property)
    {
      /* Atoms are safe to use with GINT_TO_POINTER because it's safe with
       * anything 32 bits or less, and atoms are 32 bits with the top three
       * bits clear.  (Scheifler & Gettys, 2e, p372)
       */
      g_hash_table_insert (display->prop_hooks,
                           GINT_TO_POINTER (cursor->property),
                           cursor);
      cursor++;
    }
}

void
meta_display_free_window_prop_hooks (MetaDisplay *display)
{
  g_hash_table_unref (display->prop_hooks);
  display->prop_hooks = NULL;

  g_free (display->prop_hooks_table);
  display->prop_hooks_table = NULL;
}

/**
 * Finds the hooks for a particular property.
 */
static MetaWindowPropHooks*
find_hooks (MetaDisplay *display,
            Atom         property)
{
  return g_hash_table_lookup (display->prop_hooks,
                              GINT_TO_POINTER (property));
}
