/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco window deletion */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2004 Elijah Newren
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
#define _XOPEN_SOURCE /* for gethostname() and kill() */

#include <config.h>
#include "util.h"
#include "window-private.h"
#include "errors.h"
#include "workspace.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

static void meta_window_present_delete_dialog (MetaWindow *window,
                                               guint32     timestamp);

static void
delete_ping_reply_func (MetaDisplay *display,
                        Window       xwindow,
                        guint32      timestamp,
                        void        *user_data)
{
  meta_topic (META_DEBUG_PING,
              "Got reply to delete ping for %s\n",
              ((MetaWindow*)user_data)->desc);

  /* we do nothing */
}

static void
dialog_exited (GPid     pid,
               int      status,
               gpointer user_data)
{
  MetaWindow *ours = (MetaWindow*) user_data;

  ours->dialog_pid = -1;

  /* exit status of 1 means the user pressed "Force Quit" */
  if (WIFEXITED (status) && WEXITSTATUS (status) == 1)
    meta_window_kill (ours);
}

static void
delete_ping_timeout_func (MetaDisplay *display,
                          Window       xwindow,
                          guint32      timestamp,
                          void        *user_data)
{
  MetaWindow *window = user_data;
  char *window_title;
  gchar *window_content, *tmp;
  GPid dialog_pid;

  meta_topic (META_DEBUG_PING,
              "Got delete ping timeout for %s\n",
              window->desc);

  if (window->dialog_pid >= 0)
    {
      meta_window_present_delete_dialog (window, timestamp);
      return;
    }

  window_title = g_locale_from_utf8 (window->title, -1, NULL, NULL, NULL);

  /* Translators: %s is a window title */
  tmp = g_strdup_printf (_("<tt>%s</tt> is not responding."),
                         window_title);
  window_content = g_strdup_printf (
      "<big><b>%s</b></big>\n\n<i>%s</i>",
      tmp,
      _("You may choose to wait a short while for it to "
        "continue or force the application to quit entirely."));

  g_free (window_title);

  dialog_pid =
    meta_show_dialog ("--question",
                      window_content, NULL,
                      window->screen->screen_name,
                      _("_Wait"), _("_Force Quit"), window->xwindow,
                      NULL, NULL);

  g_free (window_content);
  g_free (tmp);

  window->dialog_pid = dialog_pid;
  g_child_watch_add (dialog_pid, dialog_exited, window);
}

void
meta_window_delete (MetaWindow  *window,
                    guint32      timestamp)
{
  meta_error_trap_push (window->display);
  if (window->delete_window)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Deleting %s with delete_window request\n",
                  window->desc);
      meta_window_send_icccm_message (window,
                                      window->display->atom_WM_DELETE_WINDOW,
                                      timestamp);
    }
  else
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Deleting %s with explicit kill\n",
                  window->desc);
      XKillClient (window->display->xdisplay, window->xwindow);
    }
  meta_error_trap_pop (window->display, FALSE);

  meta_display_ping_window (window->display,
                            window,
                            timestamp,
                            delete_ping_reply_func,
                            delete_ping_timeout_func,
                            window);
}


void
meta_window_kill (MetaWindow *window)
{
  char buf[257];

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Killing %s brutally\n",
              window->desc);

  if (window->wm_client_machine != NULL &&
      window->net_wm_pid > 0)
    {
      if (gethostname (buf, sizeof(buf)-1) == 0)
        {
          if (strcmp (buf, window->wm_client_machine) == 0)
            {
              meta_topic (META_DEBUG_WINDOW_OPS,
                          "Killing %s with kill()\n",
                          window->desc);

              if (kill (window->net_wm_pid, 9) < 0)
                meta_topic (META_DEBUG_WINDOW_OPS,
                            "Failed to signal %s: %s\n",
                            window->desc, strerror (errno));
            }
        }
      else
        {
          meta_warning (_("Failed to get hostname: %s\n"),
                        strerror (errno));
        }
    }

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Disconnecting %s with XKillClient()\n",
              window->desc);
  meta_error_trap_push (window->display);
  XKillClient (window->display->xdisplay, window->xwindow);
  meta_error_trap_pop (window->display, FALSE);
}

void
meta_window_free_delete_dialog (MetaWindow *window)
{
  if (window->dialog_pid >= 0)
    {
      kill (window->dialog_pid, 9);
      window->dialog_pid = -1;
    }
}

static void
meta_window_present_delete_dialog (MetaWindow *window, guint32 timestamp)
{
  meta_topic (META_DEBUG_PING,
              "Presenting existing ping dialog for %s\n",
              window->desc);

  if (window->dialog_pid >= 0)
    {
      GSList *windows;
      GSList *tmp;

      /* Activate transient for window that belongs to
       * marco-dialog
       */

      windows = meta_display_list_windows (window->display);
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;

          if (w->xtransient_for == window->xwindow &&
              w->res_class &&
              g_ascii_strcasecmp (w->res_class, "marco-dialog") == 0)
            {
              meta_window_activate (w, timestamp);
              break;
            }

          tmp = tmp->next;
        }

      g_slist_free (windows);
    }
}
