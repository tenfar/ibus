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
#ifndef __CONNECTION_H_
#define __CONNECTION_H_

#include <ibus.h>

/*
 * Type macros.
 */

/* define GOBJECT macros */
#define BUS_TYPE_CONNECTION             \
    (bus_connection_get_type ())
#define BUS_CONNECTION(obj)             \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), BUS_TYPE_CONNECTION, BusConnection))
#define BUS_CONNECTION_CLASS(klass)     \
    (G_TYPE_CHECK_CLASS_CAST ((klass), BUS_TYPE_CONNECTION, BusConnectionClass))
#define BUS_IS_CONNECTION(obj)          \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BUS_TYPE_CONNECTION))
#define BUS_IS_CONNECTION_CLASS(klass)  \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), BUS_TYPE_CONNECTION))
#define BUS_CONNECTION_GET_CLASS(obj)   \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), BUS_TYPE_CONNECTION, BusConnectionClass))

G_BEGIN_DECLS

typedef struct _BusConnection BusConnection;
typedef struct _BusConnectionClass BusConnectionClass;

struct _BusConnection {
    IBusObject parent;

    /* instance members */
    GDBusConnection *connection;
    gchar *unique_name;
    /* list for well known names */
    GSList  *names;
    GSList  *rules;
};

struct _BusConnectionClass {
  IBusObjectClass parent;

  /* class members */
};

BusConnection   *bus_connection_lookup              (GDBusConnection    *connection);
const gchar     *bus_connection_get_unique_name     (BusConnection      *connection);
void             bus_connection_set_unique_name     (BusConnection      *connection,
                                                     const gchar        *name);
const GSList     *bus_connection_get_names          (BusConnection      *connection);
const gchar     *bus_connection_add_name            (BusConnection      *connection,
                                                     const gchar        *name);
gboolean         bus_connection_remove_name         (BusConnection      *connection,
                                                     const gchar        *name);
GDBusConnection *bus_connection_get_dbus_connection (BusConnection      *connection);

G_END_DECLS
#endif

