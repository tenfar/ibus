/* vim:set et sts=4: */
/* bus - The Input Bus
 * Copyright (C) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
 * Copyright (C) 2008-2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "server.h"
#include <gio/gio.h>
#include "dbusimpl.h"
#include "ibusimpl.h"
#include "option.h"

static GMainLoop *mainloop = NULL;
static BusDBusImpl *dbus = NULL;
static BusIBusImpl *ibus = NULL;

static void
bus_new_connection_cb (GDBusServer     *server,
                       GDBusConnection *dbus_connection,
                       gpointer         user_data)
{
    g_debug ("new connection");
    BusConnection *connection = bus_connection_new (dbus_connection);

    bus_dbus_impl_new_connection (dbus, connection);

    /* FIXME */
    if (g_object_is_floating (connection)) {
        ibus_object_destroy ((IBusObject *)connection);
        g_object_unref (connection);
    }
}

void
bus_server_start (void)
{
    g_debug ("main thead = %p", g_thread_self ());
    dbus = bus_dbus_impl_get_default ();
    ibus = bus_ibus_impl_get_default ();
    bus_dbus_impl_register_object (dbus, (IBusService *)ibus);

    /* init server */
    GDBusServerFlags flags = G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS;
    gchar *guid = g_dbus_generate_guid ();
    GDBusServer *server =  g_dbus_server_new_sync (g_address,
				        flags, guid, NULL, NULL, NULL);
    g_free (guid);

    g_signal_connect (server, "new-connection", G_CALLBACK (bus_new_connection_cb), NULL);

    g_dbus_server_start (server);

    gchar *address = g_strdup_printf ("%s,guid=%s",
                            g_dbus_server_get_client_address (server),
                            g_dbus_server_get_guid (server));

    /* write address to file */
    g_debug ("address = %s", address);

    /* FIXME */
#if 1
    ibus_write_address (address);
#endif

    g_free (address);

    /* create main loop */
    mainloop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (mainloop);

    /* stop server */
    g_dbus_server_stop (server);

    ibus_object_destroy ((IBusObject *)dbus);
    ibus_object_destroy ((IBusObject *)ibus);

    /* release resources */
    g_object_unref (server);
    g_main_loop_unref (mainloop);
    mainloop = NULL;
}

void
bus_server_quit (void)
{
    if (mainloop)
        g_main_loop_quit (mainloop);
}
