// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "dbus-object.h" // Ubuntu 12.04 did not like this, but it is
                         // requires for prototype of
                         // init_superswitcher_dbus
#include <glib.h>
#ifdef HAVE_DBUS_GLIB

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <stdlib.h>

#include "dbus-object.h"
#include "dbus-server-bindings.h"

// SuperSwitcher's DBUS IDs
#define SS_DBUS_SERVICE     "superswitcher.SuperSwitcher"
#define SS_DBUS_PATH        "/superswitcher/SuperSwitcher"
#define SS_DBUS_INTERFACE   "superswitcher.SuperSwitcher"


// We need a GObject object to represent our D-Bus object.  Because the only
// methods that we have exported over D-Bus are stateless and argumentless,
// then we can get away with a simple "dummy" GObject, rather than having to
// instantiate a custom subclass of GObject.
static GObject *obj = NULL;

// Some more D-Bus bookkeeping.  These variables are global because, if they
// were local, we would appear to "leak" memory.
static DBusGConnection *conn = NULL;
static DBusGProxy *proxy = NULL;

//------------------------------------------------------------------------------

gboolean
init_superswitcher_dbus (void)
{
  GError *err = NULL;
  guint32 request_name_ret;

  // OK, let's start with a D-Bus connection to the session bus.
  conn = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
  if (!conn) {
    g_printerr ("Error : %s\n", err->message);
    g_error_free (err);
    return FALSE;
  }
  
  // Get the D-Bus central bureaucracy for this connection...
  proxy = dbus_g_proxy_new_for_name (conn,
                                     DBUS_SERVICE_DBUS,
                                     DBUS_PATH_DBUS,
                                     DBUS_INTERFACE_DBUS);

  // ...and register that we (this process) provide the service named
  // superswitcher.SuperSwitcher.
  if (!org_freedesktop_DBus_request_name (proxy, SS_DBUS_SERVICE, 0,
                                          &request_name_ret, &err))
  {
    g_printerr ("Error : %s\n", err->message);
    g_error_free (err);
    return FALSE;
  }

  if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
  {
    g_printerr ("Another SuperSwitcher process is already running.\n");
    exit (ABNORMAL_EXIT_CODE_ANOTHER_INSTANCE_IS_RUNNING);
    return FALSE;
  }

  // GObjects now serve things, as told by dbus_glib_superswitcher_object_info,
  // including superswitcher.SuperSwitcher's ShowPopup method.
  dbus_g_object_type_install_info (G_TYPE_OBJECT,
                                   &dbus_glib_superswitcher_object_info);

  // Now register a GObject (which, thanks to the above, serves what we want)
  // as a D-Bus object.
  obj = g_object_new (G_TYPE_OBJECT, NULL);
  dbus_g_connection_register_g_object (conn, SS_DBUS_PATH, G_OBJECT (obj));

  return TRUE;
}

#endif  // #ifdef HAVE_DBUS_GLIB

