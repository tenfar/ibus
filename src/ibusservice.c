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
#include "ibusservice.h"
#include "ibusinternal.h"

#define IBUS_SERVICE_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), IBUS_TYPE_SERVICE, IBusServicePrivate))

/* XXX */
enum {
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_OBJECT_PATH,
    PROP_INTERFACE_NAME,
    PROP_INTERFACE_INFO,
    PROP_CONNECTION,
};

/* IBusServicePrivate */
struct _IBusServicePrivate {
    gchar *object_path;
    gchar *interface_name;
    GDBusInterfaceInfo *interface_info;
    GDBusConnection *connection;
};

/*
static guint    service_signals[LAST_SIGNAL] = { 0 };
*/

/* functions prototype */
static void      ibus_service_destroy        (IBusService        *service);
static void      ibus_service_set_property   (IBusService        *service,
                                              guint              prop_id,
                                              const GValue       *value,
                                              GParamSpec         *pspec);
static void      ibus_service_get_property   (IBusService        *service,
                                              guint              prop_id,
                                              GValue             *value,
                                              GParamSpec         *pspec);
static void      ibus_service_service_method_call
                                             (IBusService        *service,
                                              GDBusConnection    *connection,
                                              const gchar        *sender,
                                              const gchar        *object_path,
                                              const gchar        *interface_name,
                                              const gchar        *method_name,
                                              GVariant           *parameters,
                                              GDBusMethodInvocation
                                                                 *invocation);
static GVariant *ibus_service_service_get_property
                                             (IBusService        *service,
                                              GDBusConnection    *connection,
                                              const gchar        *sender,
                                              const gchar        *object_path,
                                              const gchar        *interface_name,
                                              const gchar        *property_name,
                                              GError            **error);
static gboolean  ibus_service_service_set_property
                                             (IBusService        *service,
                                              GDBusConnection    *connection,
                                              const gchar        *sender,
                                              const gchar        *object_path,
                                              const gchar        *interface_name,
                                              const gchar        *property_name,
                                              GVariant           *value,
                                              GError            **error);

G_DEFINE_TYPE (IBusService, ibus_service, IBUS_TYPE_OBJECT)

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.freedesktop.IBus.Service'>"
    "    <method name='Destroy' />"
    "  </interface>"
    "</node>";

static void
ibus_service_class_init (IBusServiceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);

    gobject_class->set_property = (GObjectSetPropertyFunc) ibus_service_set_property;
    gobject_class->get_property = (GObjectGetPropertyFunc) ibus_service_get_property;
    ibus_object_class->destroy = (IBusObjectDestroyFunc) ibus_service_destroy;

    /* virtual functions */
    klass->service_method_call = ibus_service_service_method_call;
    klass->service_get_property = ibus_service_service_get_property;
    klass->service_set_property = ibus_service_service_set_property;

    /* class members */
    klass->interfaces = g_array_new (TRUE, TRUE, sizeof (GDBusInterfaceInfo));
    ibus_service_class_add_interfaces (klass, introspection_xml);

    /* install properties */
    /**
     * IBusService:object-path:
     *
     * The path of service object.
     */
    g_object_class_install_property (
                    gobject_class,
                    PROP_OBJECT_PATH,
                    g_param_spec_string (
                        "object-path",
                        "object path",
                        "The path of service object",
                        NULL,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_NAME |
                        G_PARAM_STATIC_NICK |
                        G_PARAM_STATIC_BLURB)
                    );
    /**
     * IBusService:interface-name:
     *
     * The interface name.
     */
    g_object_class_install_property (
                    gobject_class,
                    PROP_INTERFACE_NAME,
                    g_param_spec_string (
                        "interface-name",
                        "interface name",
                        "The interface name of service object",
                        NULL,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_NAME |
                        G_PARAM_STATIC_NICK |
                        G_PARAM_STATIC_BLURB)
                    );
    /**
     * IBusService:interface-info:
     *
     * The interface info.
     */
    g_object_class_install_property (
                    gobject_class,
                    PROP_INTERFACE_INFO,
                    g_param_spec_boxed (
                        "interface-info",
                        "interface info",
                        "The interface info of service object",
                        G_TYPE_DBUS_INTERFACE_INFO,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_NAME |
                        G_PARAM_STATIC_NICK |
                        G_PARAM_STATIC_BLURB)
                    );
    /**
     * IBusService:connection:
     *
     * The connection of service object.
     */
    g_object_class_install_property (
                    gobject_class,
                    PROP_CONNECTION,
                    g_param_spec_object (
                        "connection",
                        "connection",
                        "The connection of service object",
                        G_TYPE_DBUS_CONNECTION,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_NAME |
                        G_PARAM_STATIC_NICK |
                        G_PARAM_STATIC_BLURB)
                    );


    /* XXX */
    #if 0
    /* Install signals */
    /**
     * IBusService::ibus-message:
     * @service: An IBusService.
     * @connection: Corresponding IBusConnection.
     * @message: An IBusMessage to be sent.
     *
     * Send a message as IBusMessage though the @connection.
     *
     * Returns: TRUE if succeed; FALSE otherwise.
     * <note><para>Argument @user_data is ignored in this function.</para></note>
     */
    service_signals[IBUS_MESSAGE] =
        g_signal_new (I_("ibus-message"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (IBusServiceClass, ibus_message),
            NULL, NULL,
            ibus_marshal_BOOLEAN__POINTER_POINTER,
            G_TYPE_BOOLEAN,
            2,
            G_TYPE_POINTER,
            G_TYPE_POINTER);

    /**
     * IBusService::ibus-signal:
     * @service: An IBusService.
     * @connection: Corresponding IBusConnection.
     * @message: An IBusMessage to be sent.
     *
     * Send a signal as IBusMessage though the @connection.
     *
     * Returns: TRUE if succeed; FALSE otherwise.
     * <note><para>Argument @user_data is ignored in this function.</para></note>
     */
    service_signals[IBUS_SIGNAL] =
        g_signal_new (I_("ibus-signal"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (IBusServiceClass, ibus_signal),
            NULL, NULL,
            ibus_marshal_BOOLEAN__POINTER_POINTER,
            G_TYPE_BOOLEAN,
            2,
            G_TYPE_POINTER,
            G_TYPE_POINTER);
    #endif
    g_type_class_add_private (klass, sizeof (IBusServicePrivate));
}

static void
ibus_service_init (IBusService *service)
{
    service->priv = IBUS_SERVICE_GET_PRIVATE (service);
}

static void
ibus_service_destroy (IBusService *service)
{
    g_free (service->priv->object_path);
    service->priv->object_path = NULL;

    g_free (service->priv->interface_name);
    service->priv->interface_name = NULL;

    if (service->priv->interface_info) {
        g_dbus_interface_info_unref (service->priv->interface_info);
        service->priv->interface_info = NULL;
    }

    if (service->priv->connection) {
        g_object_unref (service->priv->connection);
        service->priv->connection = NULL;
    }

    IBUS_OBJECT_CLASS(ibus_service_parent_class)->destroy (IBUS_OBJECT (service));
}

static void
ibus_service_set_property (IBusService  *service,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    switch (prop_id) {
    case PROP_OBJECT_PATH:
        service->priv->object_path = g_value_dup_string (value);
        break;
    case PROP_INTERFACE_NAME:
        service->priv->interface_name = g_value_dup_string (value);
        break;
    case PROP_INTERFACE_INFO:
        service->priv->interface_info = g_value_get_boxed (value);
        if (service->priv->interface_info)
            g_dbus_interface_info_ref (service->priv->interface_info);
        break;
    case PROP_CONNECTION:
        service->priv->connection = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (service, prop_id, pspec);
    }
}

static void
ibus_service_get_property (IBusService *service,
                           guint        prop_id,
                           GValue      *value,
                           GParamSpec  *pspec)
{
    switch (prop_id) {
    case PROP_OBJECT_PATH:
        g_value_set_string (value, service->priv->object_path);
        break;
    case PROP_INTERFACE_NAME:
        g_value_set_string (value, service->priv->interface_name);
        break;
    case PROP_INTERFACE_INFO:
        g_value_set_boxed (value, service->priv->interface_info);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, service->priv->connection);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (service, prop_id, pspec);
    }
}

static void
ibus_service_service_method_call (IBusService           *service,
                                  GDBusConnection       *connection,
                                  const gchar           *sender,
                                  const gchar           *object_path,
                                  const gchar           *interface_name,
                                  const gchar           *method_name,
                                  GVariant              *parameters,
                                  GDBusMethodInvocation *invocation)
{
    if (g_strcmp0 (method_name, "Destroy") == 0) {
        g_dbus_method_invocation_return_value (invocation, NULL);
        ibus_object_destroy ((IBusObject *)service);
        return;
    }

    g_dbus_method_invocation_return_error (invocation,
                                           G_DBUS_ERROR,
                                           G_DBUS_ERROR_UNKNOWN_METHOD,
                                           "%s::%s", interface_name, method_name);
    return;
}

static GVariant *
ibus_service_service_get_property (IBusService     *service,
                                  GDBusConnection  *connection,
                                  const gchar      *sender,
                                  const gchar      *object_path,
                                  const gchar      *interface_name,
                                  const gchar      *property_name,
                                  GError          **error)
{
    return NULL;
}

static gboolean
ibus_service_service_set_property (IBusService     *service,
                                  GDBusConnection  *connection,
                                  const gchar      *sender,
                                  const gchar      *object_path,
                                  const gchar      *interface_name,
                                  const gchar      *property_name,
                                  GVariant         *value,
                                  GError          **error)
{
    return FALSE;
}

IBusService *
ibus_service_new (GDBusConnection *connection,
                  const gchar     *object_path)
{
    g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
    g_return_val_if_fail (object_path != NULL, NULL);

    GObject *obj = g_object_new (IBUS_TYPE_SERVICE,
                                 "object-path", object_path,
                                 "connection", connection,
                                 NULL);
    return IBUS_SERVICE (obj);
}

const gchar *
ibus_service_get_object_path (IBusService *service)
{
    g_return_val_if_fail (IBUS_IS_SERVICE (service), NULL);
    return service->priv->object_path;
}

GDBusConnection *
ibus_service_get_connection (IBusService *service)
{
    g_return_val_if_fail (IBUS_IS_SERVICE (service), NULL);
    return service->priv->connection;
}

gboolean
ibus_service_emit_signal (IBusService *service,
                          const gchar *dest_bus_name,
                          const gchar *signal_name,
                          GVariant    *parameters,
                          GError    **error)
{
    g_return_val_if_fail (IBUS_IS_SERVICE (service), FALSE);
    return g_dbus_connection_emit_signal (service->priv->connection,
                                          dest_bus_name,
                                          service->priv->object_path,
                                          service->priv->interface_name,
                                          signal_name,
                                          parameters,
                                          error);
}

gboolean
ibus_service_class_add_interfaces (IBusServiceClass   *klass,
                                   const gchar        *xml_data)
{
    g_return_val_if_fail (IBUS_IS_SERVICE_CLASS (klass), FALSE);
    g_return_val_if_fail (xml_data != NULL, FALSE);

    GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml (xml_data, NULL);
    if (introspection_data != NULL) {
        GDBusInterfaceInfo **p = introspection_data->interfaces;
        while (*p != NULL) {
            g_dbus_interface_info_ref (*p);
            g_array_append_val (klass->interfaces, *p);
            p++;
        }
        g_dbus_node_info_unref (introspection_data);
        return TRUE;
    }
    return FALSE;
}
