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

#include <unistd.h>
#include "connection.h"
#include "matchrule.h"

// static guint            _signals[LAST_SIGNAL] = { 0 };

/* functions prototype */
static void     bus_connection_destroy      (BusConnection      *connection);
static void     bus_connection_set_dbus_connection
                                            (BusConnection      *connection,
                                             GDBusConnection    *dbus_connection);
static void     bus_connection_dbus_connection_closed_cb
                                            (GDBusConnection    *dbus_connection,
                                             gboolean            remote_peer_vanished,
                                             GError             *error,
                                             BusConnection      *connection);

G_DEFINE_TYPE (BusConnection, bus_connection, IBUS_TYPE_OBJECT)

BusConnection *
bus_connection_new (GDBusConnection *dbus_connection)
{
    g_return_val_if_fail (G_IS_DBUS_CONNECTION (dbus_connection), NULL);
    g_return_val_if_fail (!g_dbus_connection_is_closed (dbus_connection), NULL);

    BusConnection *connection = BUS_CONNECTION (g_object_new (BUS_TYPE_CONNECTION, NULL));

    bus_connection_set_dbus_connection (connection, dbus_connection);

    return connection;
}

static void
bus_connection_class_init (BusConnectionClass *klass)
{
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);

    ibus_object_class->destroy = (IBusObjectDestroyFunc) bus_connection_destroy;
}

static void
bus_connection_init (BusConnection *connection)
{
    connection->unique_name = NULL;
    connection->names = NULL;
}

static void
bus_connection_destroy (BusConnection *connection)
{
    GSList *name;

    IBUS_OBJECT_CLASS(bus_connection_parent_class)->destroy (IBUS_OBJECT (connection));

    if (connection->connection) {
        g_signal_handlers_disconnect_by_func (connection->connection,
                G_CALLBACK (bus_connection_dbus_connection_closed_cb), connection);
        g_object_unref (connection->connection);
        connection->connection = NULL;
    }

    if (connection->unique_name) {
        g_free (connection->unique_name);
        connection->unique_name = NULL;
    }

    g_slist_foreach (connection->names, (GFunc) g_free, NULL);
    g_slist_free (connection->names);
    connection->names = NULL;
}

static void
bus_connection_dbus_connection_closed_cb (GDBusConnection *dbus_connection,
                                          gboolean         remote_peer_vanished,
                                          GError          *error,
                                          BusConnection   *connection)
{
    ibus_object_destroy ((IBusObject *)connection);
}

static void
bus_connection_set_dbus_connection (BusConnection   *connection,
                                    GDBusConnection *dbus_connection)
{
    connection->connection = dbus_connection;
    g_object_ref (connection->connection);

    g_signal_connect (connection->connection, "closed",
                G_CALLBACK (bus_connection_dbus_connection_closed_cb), connection);
}

const gchar *
bus_connection_get_unique_name (BusConnection   *connection)
{
    return connection->unique_name;
}

void
bus_connection_set_unique_name (BusConnection   *connection,
                                const gchar     *name)
{
    g_assert (connection->unique_name == NULL);
    connection->unique_name = g_strdup (name);
}

const GSList *
bus_connection_get_names (BusConnection   *connection)
{
    return connection->names;
}

const gchar *
bus_connection_add_name (BusConnection     *connection,
                          const gchar       *name)
{
    gchar *new_name;

    new_name = g_strdup (name);
    connection->names = g_slist_append (connection->names, new_name);

    return new_name;
}

gboolean
bus_connection_remove_name (BusConnection     *connection,
                             const gchar       *name)
{
    GSList *link;

    link = g_slist_find_custom (connection->names, name, (GCompareFunc) g_strcmp0);

    if (link) {
        g_free (link->data);
        connection->names = g_slist_delete_link (connection->names, link);
        return TRUE;
    }
    return FALSE;
}

gboolean
bus_connection_add_match (BusConnection  *connection,
                          const gchar    *rule)
{
    g_assert (BUS_IS_CONNECTION (connection));
    g_assert (rule != NULL);

    BusMatchRule *p;
    GSList *link;

    p = bus_match_rule_new (rule);
    if (p == NULL)
        return FALSE;

    for (link = connection->rules; link != NULL; link = link->next) {
        if (bus_match_rule_is_equal (p, (BusMatchRule *)link->data)) {
            g_object_unref (p);
            return TRUE;
        }
    }

    connection->rules = g_slist_append (connection->rules, p);
    return TRUE;

}

gboolean
bus_connection_remove_match (BusConnection  *connection,
                             const gchar    *rule)
{
    return FALSE;
}

