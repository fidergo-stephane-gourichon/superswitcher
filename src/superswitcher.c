// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "screen.h"
#include "popup.h"

#ifdef HAVE_DBUS_GLIB
#include "dbus-object.h"
#endif

#ifdef HAVE_XCOMPOSITE
#include "thumbnailer.h"
#endif

//------------------------------------------------------------------------------

// TODO - listen to window manager changes.
gboolean window_manager_uses_viewports = FALSE;

//------------------------------------------------------------------------------

static Window x_root_window = None;
static SSScreen *screen = NULL;
static Popup *popup = NULL;
static int popup_keycode_to_free = -1;
static gboolean also_trigger_on_caps_lock = FALSE;
static gboolean only_trigger_on_caps_lock = FALSE;
static gboolean show_version_and_exit = FALSE;

//------------------------------------------------------------------------------

static GdkFilterReturn
filter_func (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
  XEvent *x_event;
  x_event = (XEvent *) gdk_xevent;

  switch (x_event->type) {
  case KeyPress:
    if (popup == NULL && popup_keycode_to_free == -1) {
      popup_keycode_to_free = x_event->xkey.keycode;
      popup = popup_create (screen);
    } else {
      popup_on_key_press (popup, gdk_x11_get_default_xdisplay (), &x_event->xkey);
    }
    break;
  case KeyRelease:
    if (popup_keycode_to_free == x_event->xkey.keycode) {
      popup_keycode_to_free = -1;
      popup_free (popup);
      popup = NULL;
    }
    break;
  default:
    // No-op.
    break;
  }

  return GDK_FILTER_CONTINUE;
}

//------------------------------------------------------------------------------

static void
grab (int keyval)
{
  Display *display;
  display = gdk_x11_get_default_xdisplay ();
  XGrabKey (display,
            XKeysymToKeycode (display, keyval),
            AnyModifier,
            x_root_window,
            False,
            GrabModeAsync,
            GrabModeAsync);
}

//------------------------------------------------------------------------------

static void
disable_caps_lock_default_behavior (void)
{
  Display *display;
  KeyCode keycode;
  XModifierKeymap *map;
  char *error_msg;

  display = gdk_x11_get_default_xdisplay ();
  keycode = XKeysymToKeycode (display, XK_Caps_Lock);

  map = XGetModifierMapping (display);
  map = XDeleteModifiermapEntry (map, keycode, LockMapIndex);

  error_msg = NULL;
  switch (XSetModifierMapping (display, map)) {
    case MappingSuccess:
      break;
    case MappingBusy:
      error_msg = "since it's busy.";
      break;
    default:
      error_msg = "for some unknown reason.";
      break;
  }
  if (error_msg != NULL) {
    g_printerr ("SuperSwitcher could not disable the Caps Lock key, %s\n",
                error_msg);
  }
  XFreeModifiermap (map);
}

//------------------------------------------------------------------------------

gboolean
superswitcher_hide_popup (void *object, GError **error)
{
  if (popup) {
    popup_free (popup);
    popup = NULL;
  }
  return TRUE;
}

//------------------------------------------------------------------------------

gboolean
superswitcher_show_popup (void *object, GError **error)
{
  if (!popup) {
    popup = popup_create (screen);
  }
  return TRUE;
}

//------------------------------------------------------------------------------

gboolean
superswitcher_toggle_popup (void *object, GError **error)
{
  if (popup) {
    return superswitcher_hide_popup (object, error);
  } else {
    return superswitcher_show_popup (object, error);
  }
}

//------------------------------------------------------------------------------

int
main (int argc, char **argv)
{
  static const GOptionEntry options[] = {
    { "also-trigger-on-caps-lock", 'c', 0, G_OPTION_ARG_NONE,
      &also_trigger_on_caps_lock,
      "Make the Caps Lock key also switch windows (as well as the Super key)",
      NULL },
    { "only-trigger-on-caps-lock", 'C', 0, G_OPTION_ARG_NONE,
      &only_trigger_on_caps_lock,
      "Make only the Caps Lock key switch windows (instead of the Super key)",
      NULL },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version_and_exit,
      "Show the version number and exit", NULL },
#ifdef HAVE_XCOMPOSITE
    { "show-window-thumbnails", 't', 0, G_OPTION_ARG_NONE,
      &show_window_thumbnails,
      "EXPERIMENTAL - Show window thumbnails (instead of icons)", NULL },
#endif
    { NULL }
  };

  GdkWindow *root;
  GOptionContext *context;
  GError *error;

  gtk_init (&argc, &argv);

  context = g_option_context_new ("");
  error = NULL;
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_parse (context, &argc, &argv, &error);

  if (error) {
    g_printerr ("%s\n", error->message);
    g_error_free (error);
    exit (ABNORMAL_EXIT_CODE_UNKNOWN_COMMAND_LINE_OPTION);
  }

  if (show_version_and_exit) {
    // VERSION comes from the Makefile generated by autogen.sh and configure.in.
    printf ("SuperSwitcher version %s\n", VERSION);
    return 0;
  }

#ifdef HAVE_DBUS_GLIB
  // Note that this may exit(...) if another instance is already running.
  init_superswitcher_dbus ();
#endif

#ifdef HAVE_XCOMPOSITE
  if (show_window_thumbnails) {
    show_window_thumbnails = init_composite ();
  }
#endif

  root = gdk_get_default_root_window ();
  x_root_window = GDK_WINDOW_XWINDOW (root);

  gdk_window_add_filter (root, filter_func, NULL);
  if (!only_trigger_on_caps_lock) {
    grab (XK_Super_L);
    grab (XK_Super_R);
  }
  if (also_trigger_on_caps_lock || only_trigger_on_caps_lock) {
    disable_caps_lock_default_behavior ();
    grab (XK_Caps_Lock);
  }

  screen = ss_screen_new (wnck_screen_get_default (), gdk_x11_get_default_xdisplay (), x_root_window);

  gtk_main ();

#ifdef HAVE_XCOMPOSITE
  if (show_window_thumbnails) {
    uninit_composite ();
  }
#endif

  return 0;
}
