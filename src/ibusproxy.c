/* vim:set et sts=4: */
/* ibus - The Input Bus
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

#include "ibusobject.h"
#include "ibusproxy.h"

#include "ibusinternal.h"

#define IBUS_PROXY_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), IBUS_TYPE_PROXY, IBusProxyPrivate))

enum {
    DESTROY,
    LAST_SIGNAL,
};

static guint            proxy_signals[LAST_SIGNAL] = { 0 };

/* functions prototype */
static void      ibus_proxy_constructed     (GObject            *object);
static void      ibus_proxy_dispose         (GObject            *object);
static void      ibus_proxy_real_destroy    (IBusProxy          *proxy);

static void      ibus_proxy_connection_closed_cb
                                            (GDBusConnection    *connection,
                                             gboolean            remote_peer_vanished,
                                             GError             *error,
                                             IBusProxy          *proxy);

G_DEFINE_TYPE (IBusProxy, ibus_proxy, G_TYPE_DBUS_PROXY)

static void
ibus_proxy_class_init (IBusProxyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = ibus_proxy_constructed;
    gobject_class->dispose = ibus_proxy_dispose;

    klass->destroy = ibus_proxy_real_destroy;

    /* install signals */
    /**
     * IBusProxy::destroy:
     * @object: An IBusProxy.
     *
     * Destroy and free an IBusProxy
     *
     * See also:  ibus_proxy_destroy().
     *
     * <note><para>Argument @user_data is ignored in this function.</para></note>
     */
    proxy_signals[DESTROY] =
        g_signal_new (I_("destroy"),
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (IBusProxyClass, destroy),
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
}

#if 0
static GObject *
ibus_proxy_constructor (GType           type,
                        guint           n_construct_params,
                        GObjectConstructParam *construct_params)
{
    GObject *obj;
    IBusProxy *proxy;
    IBusProxyPrivate *priv;

    obj = G_OBJECT_CLASS (ibus_proxy_parent_class)->constructor (type, n_construct_params, construct_params);

    proxy = IBUS_PROXY (obj);
    priv = IBUS_PROXY_GET_PRIVATE (proxy);

    if (priv->connection == NULL) {
        g_object_unref (proxy);
        return NULL;
    }

    if (priv->name != NULL) {
        IBusError *error;
        gchar *rule;

        if (ibus_proxy_get_unique_name (proxy) == NULL) {
            g_object_unref (proxy);
            return NULL;
        }

        rule = g_strdup_printf ("type='signal',"
                                "sender='"      DBUS_SERVICE_DBUS   "',"
                                "path='"        DBUS_PATH_DBUS      "',"
                                "interface='"   DBUS_INTERFACE_DBUS "',"
                                "member='NameOwnerChanged',"
                                "arg0='%s'",
                                priv->unique_name);

        if (!ibus_connection_call (priv->connection,
                                   DBUS_SERVICE_DBUS,
                                   DBUS_PATH_DBUS,
                                   DBUS_INTERFACE_DBUS,
                                   "AddMatch",
                                   &error,
                                   G_TYPE_STRING, &rule,
                                   G_TYPE_INVALID)) {
            g_warning ("%s: %s", error->name, error->message);
            ibus_error_free (error);
        }
        g_free (rule);

        rule =  g_strdup_printf ("type='signal',"
                                 "sender='%s',"
                                 "path='%s'",
                                 priv->unique_name,
                                 priv->path);

        if (!ibus_connection_call (priv->connection,
                                   DBUS_SERVICE_DBUS,
                                   DBUS_PATH_DBUS,
                                   DBUS_INTERFACE_DBUS,
                                   "AddMatch",
                                   &error,
                                   G_TYPE_STRING, &rule,
                                   G_TYPE_INVALID)) {
            g_warning ("%s: %s", error->name, error->message);
            ibus_error_free (error);
        }
        g_free (rule);
    }
    g_signal_connect (priv->connection,
                      "ibus-signal",
                      (GCallback) _connection_ibus_signal_cb,
                      proxy);

    g_signal_connect (priv->connection,
                      "destroy",
                      (GCallback) _connection_destroy_cb,
                      proxy);
    return obj;
}
#endif

static void
ibus_proxy_init (IBusProxy *proxy)
{
}

static void
ibus_proxy_constructed (GObject *object)
{
    GDBusConnection *connection;
    connection = g_dbus_proxy_get_connection ((GDBusProxy *)object);

    g_assert (connection);
    g_assert (!g_dbus_connection_is_closed (connection));

    g_signal_connect (connection, "closed",
            G_CALLBACK (ibus_proxy_connection_closed_cb), object);

    /* FIXME add match rules? */
}

static void
ibus_proxy_dispose (GObject *object)
{
    if (! (IBUS_PROXY_FLAGS (object) & IBUS_IN_DESTRUCTION)) {
        IBUS_PROXY_SET_FLAGS (object, IBUS_IN_DESTRUCTION);
        if (! (IBUS_PROXY_FLAGS (object) & IBUS_DESTROYED)) {
            g_signal_emit (object, proxy_signals[DESTROY], 0);
            IBUS_PROXY_SET_FLAGS (object, IBUS_DESTROYED);
        }
        IBUS_PROXY_UNSET_FLAGS (object, IBUS_IN_DESTRUCTION);
    }

    G_OBJECT_CLASS(ibus_proxy_parent_class)->dispose (object);
}

static void
ibus_proxy_real_destroy (IBusProxy *proxy)
{
    /* FIXME */
#if 0
    IBusProxyPrivate *priv;
    priv = IBUS_PROXY_GET_PRIVATE (proxy);

    if (priv->connection) {
        /* disconnect signal handlers */
        g_signal_handlers_disconnect_by_func (priv->connection,
                                              (GCallback) _connection_ibus_signal_cb,
                                              proxy);
        g_signal_handlers_disconnect_by_func (priv->connection,
                                              (GCallback) _connection_destroy_cb,
                                              proxy);

        /* remove match rules */
        if (priv->name != NULL && ibus_connection_is_connected (priv->connection)) {

            IBusError *error;
            gchar *rule;

            rule = g_strdup_printf ("type='signal',"
                                    "sender='"      DBUS_SERVICE_DBUS   "',"
                                    "path='"        DBUS_PATH_DBUS      "',"
                                    "interface='"   DBUS_INTERFACE_DBUS "',"
                                    "member='NameOwnerChanged',"
                                    "arg0='%s'",
                                    priv->unique_name);

            if (!ibus_connection_call (priv->connection,
                                       DBUS_SERVICE_DBUS,
                                       DBUS_PATH_DBUS,
                                       DBUS_INTERFACE_DBUS,
                                       "RemoveMatch",
                                       &error,
                                       G_TYPE_STRING, &rule,
                                       G_TYPE_INVALID)) {

                g_warning ("%s: %s", error->name, error->message);
                ibus_error_free (error);
            }
            g_free (rule);

            rule =  g_strdup_printf ("type='signal',"
                                     "sender='%s',"
                                     "path='%s'",
                                     priv->unique_name,
                                     priv->path);

            if (!ibus_connection_call (priv->connection,
                                       DBUS_SERVICE_DBUS,
                                       DBUS_PATH_DBUS,
                                       DBUS_INTERFACE_DBUS,
                                       "RemoveMatch",
                                       &error,
                                       G_TYPE_STRING, &rule,
                                       G_TYPE_INVALID)) {

                g_warning ("%s: %s", error->name, error->message);
                ibus_error_free (error);
            }
            g_free (rule);

        }
    }

    g_free (priv->name);
    g_free (priv->unique_name);
    g_free (priv->path);
    g_free (priv->interface);

    priv->name = NULL;
    priv->path = NULL;
    priv->interface = NULL;

    if (priv->connection) {
        g_object_unref (priv->connection);
        priv->connection = NULL;
    }

    IBUS_OBJECT_CLASS(ibus_proxy_parent_class)->destroy (IBUS_OBJECT (proxy));
#endif
}

static void
ibus_proxy_connection_closed_cb (GDBusConnection *connection,
                                 gboolean         remote_peer_vanished,
                                 GError          *error,
                                 IBusProxy       *proxy)
{
    ibus_proxy_destroy (proxy);
}

void
ibus_proxy_destroy (IBusProxy *proxy)
{
    if (! (IBUS_PROXY_FLAGS (proxy) & IBUS_IN_DESTRUCTION)) {
        g_object_run_dispose (G_OBJECT (proxy));
    }
}

