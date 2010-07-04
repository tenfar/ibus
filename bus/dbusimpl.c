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
#include "dbusimpl.h"
#include <string.h>
#include <ibusinternal.h>
#include "marshalers.h"
#include "matchrule.h"

enum {
    NAME_OWNER_CHANGED,
    LAST_SIGNAL,
};

enum {
    PROP_0,
};

static guint dbus_signals[LAST_SIGNAL] = { 0 };

/* functions prototype */
static void     bus_dbus_impl_destroy           (BusDBusImpl        *dbus);
static void     bus_dbus_impl_service_method_call
                                                (IBusService        *service,
                                                 GDBusConnection    *dbus_connection,
                                                 const gchar        *sender,
                                                 const gchar        *object_path,
                                                 const gchar        *interface_name,
                                                 const gchar        *method_name,
                                                 GVariant           *parameters,
                                                 GDBusMethodInvocation
                                                                    *invocation);
static GVariant *bus_dbus_impl_service_get_property
                                                (IBusService        *service,
                                                 GDBusConnection    *connection,
                                                 const gchar        *sender,
                                                 const gchar        *object_path,
                                                 const gchar        *interface_name,
                                                 const gchar        *property_name,
                                                 GError            **error);
static gboolean  bus_dbus_impl_service_set_property
                                                (IBusService        *service,
                                                 GDBusConnection    *connection,
                                                 const gchar        *sender,
                                                 const gchar        *object_path,
                                                 const gchar        *interface_name,
                                                 const gchar        *property_name,
                                                 GVariant           *value,
                                                 GError            **error);
static void      bus_dbus_impl_name_owner_changed
                                                (BusDBusImpl        *dbus,
                                                 gchar              *name,
                                                 gchar              *old_name,
                                                 gchar              *new_name);
static void      bus_dbus_impl_connection_destroy_cb
                                                (BusConnection      *connection,
                                                 BusDBusImpl        *dbus);
static void      bus_dbus_impl_rule_destroy_cb  (BusMatchRule       *rule,
                                                 BusDBusImpl        *dbus);

G_DEFINE_TYPE(BusDBusImpl, bus_dbus_impl, IBUS_TYPE_SERVICE)

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.freedesktop.DBus'>"
    "    <method name='Hello'>"
    "      <arg direction='out' type='s' name='unique_name' />"
    "    </method>"
    "    <method name='RequestName'>"
    "      <arg direction='in'  type='s' name='name' />"
    "      <arg direction='in'  type='u' name='flags' />"
    "      <arg direction='out' type='u' />"
    "    </method>"
    "    <method name='ReleaseName'>"
    "      <arg direction='in'  type='s' name='name' />"
    "      <arg direction='out' type='u' />"
    "    </method>"
    "    <method name='StartServiceByName'>"
    "      <arg direction='in'  type='s' />"
    "      <arg direction='in'  type='u' />"
    "      <arg direction='out' type='u' />"
    "    </method>"
    "    <method name='UpdateActivationEnvironment'>"
    "      <arg direction='in' type='a{ss}'/>"
    "    </method>"
    "    <method name='NameHasOwner'>"
    "      <arg direction='in'  type='s' name='name' />"
    "      <arg direction='out' type='b' />"
    "    </method>"
    "    <method name='ListNames'>"
    "      <arg direction='out' type='as' />"
    "    </method>"
    "    <method name='ListActivatableNames'>"
    "      <arg direction='out' type='as' />"
    "    </method>"
    "    <method name='AddMatch'>"
    "      <arg direction='in'  type='s' name='match_rule' />"
    "    </method>"
    "    <method name='RemoveMatch'>"
    "      <arg direction='in'  type='s' name='match_rule' />"
    "    </method>"
    "    <method name='GetNameOwner'>"
    "      <arg direction='in'  type='s' name='name' />"
    "      <arg direction='out' type='s' name='unique_name' />"
    "    </method>"
    "    <method name='ListQueuedOwners'>"
    "      <arg direction='in'  type='s' />"
    "      <arg direction='out' type='as' />"
    "    </method>"
    "    <method name='GetConnectionUnixUser'>"
    "      <arg direction='in'  type='s' />"
    "      <arg direction='out' type='u' />"
    "    </method>"
    "    <method name='GetConnectionUnixProcessID'>"
    "      <arg direction='in'  type='s' />"
    "      <arg direction='out' type='u' />"
    "    </method>"
    "    <method name='GetAdtAuditSessionData'>"
    "      <arg direction='in'  type='s' />"
    "      <arg direction='out' type='ay' />"
    "    </method>"
    "    <method name='GetConnectionSELinuxSecurityContext'>"
    "      <arg direction='in'  type='s' />"
    "      <arg direction='out' type='ay' />"
    "    </method>"
    "    <method name='ReloadConfig' />"
    "    <method name='GetId'>"
    "      <arg direction='out' type='s' />"
    "    </method>"
    "    <signal name='NameOwnerChanged'>"
    "      <arg type='s' name='name' />"
    "      <arg type='s' name='old_owner' />"
    "      <arg type='s' name='new_owner' />"
    "    </signal>"
    "    <signal name='NameLost'>"
    "      <arg type='s' name='name' />"
    "    </signal>"
    "    <signal name='NameAcquired'>"
    "      <arg type='s' name='name' />"
    "    </signal>"
    "  </interface>"
    "</node>";

static void
bus_dbus_impl_class_init (BusDBusImplClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);

    IBUS_OBJECT_CLASS (gobject_class)->destroy = (IBusObjectDestroyFunc) bus_dbus_impl_destroy;

    IBUS_SERVICE_CLASS (class)->service_method_call =  bus_dbus_impl_service_method_call;
    IBUS_SERVICE_CLASS (class)->service_get_property = bus_dbus_impl_service_get_property;
    IBUS_SERVICE_CLASS (class)->service_set_property = bus_dbus_impl_service_set_property;

    ibus_service_class_add_interfaces (IBUS_SERVICE_CLASS (class), introspection_xml);

    class->name_owner_changed = bus_dbus_impl_name_owner_changed;

    /* install signals */
    dbus_signals[NAME_OWNER_CHANGED] =
        g_signal_new (I_("name-owner-changed"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET (BusDBusImplClass, name_owner_changed),
            NULL, NULL,
            bus_marshal_VOID__STRING_STRING_STRING,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);
}

static void
bus_dbus_impl_init (BusDBusImpl *dbus)
{
    dbus->unique_names = g_hash_table_new (g_str_hash, g_str_equal);
    dbus->names = g_hash_table_new (g_str_hash, g_str_equal);
    dbus->objects = g_hash_table_new (g_str_hash, g_str_equal);
    dbus->connections = NULL;
    dbus->rules = NULL;
    dbus->id = 1;

    g_object_ref (dbus);
    g_hash_table_insert (dbus->objects, "/org/freedesktop/DBus", dbus);
}

static void
bus_dbus_impl_destroy (BusDBusImpl *dbus)
{
    GSList *p;

    for (p = dbus->rules; p != NULL; p = p->next) {
        BusMatchRule *rule = BUS_MATCH_RULE (p->data);
        g_signal_handlers_disconnect_by_func (rule,
                        G_CALLBACK (bus_dbus_impl_rule_destroy_cb), dbus);
        ibus_object_destroy ((IBusObject *) rule);
        g_object_unref (rule);
    }
    g_slist_free (dbus->rules);
    dbus->rules = NULL;

    for (p = dbus->connections; p != NULL; p = p->next) {
        GDBusConnection *connection = G_DBUS_CONNECTION (p->data);
        g_signal_handlers_disconnect_by_func (connection, bus_dbus_impl_connection_destroy_cb, dbus);
        g_dbus_connection_close (connection);
        g_object_unref (connection);
    }
    g_slist_free (dbus->connections);
    dbus->connections = NULL;

    g_hash_table_remove_all (dbus->unique_names);
    g_hash_table_remove_all (dbus->names);
    g_hash_table_remove_all (dbus->objects);

    dbus->unique_names = NULL;
    dbus->names = NULL;
    dbus->objects = NULL;

    IBUS_OBJECT_CLASS(bus_dbus_impl_parent_class)->destroy ((IBusObject *)dbus);
}

static void
bus_dbus_impl_hello (BusDBusImpl           *dbus,
                     BusConnection         *connection,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation)
{
    if (bus_connection_get_unique_name (connection) != NULL) {
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "Already handled an Hello message");
    }
    else {
        gchar *name = g_strdup_printf (":1.%d", dbus->id ++);
        bus_connection_set_unique_name (connection, name);
        g_free (name);

        name = (gchar *) bus_connection_get_unique_name (connection);
        g_hash_table_insert (dbus->unique_names, name, connection);
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", name));

        g_signal_emit (dbus,
                       dbus_signals[NAME_OWNER_CHANGED],
                       0,
                       name,
                       "",
                       name);
    }
}

static void
bus_dbus_impl_list_names (BusDBusImpl           *dbus,
                          BusConnection         *connection,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation)
{
    GVariantBuilder builder;
    g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

    /* FIXME should add them? */
    g_variant_builder_add (&builder, "s", "org.freedesktop.DBus");
    g_variant_builder_add (&builder, "s", "org.freedesktop.IBus");

    // append well-known names
    GList *names, *name;
    names = g_hash_table_get_keys (dbus->names);
    names = g_list_sort (names, (GCompareFunc) g_strcmp0);
    for (name = names; name != NULL; name = name->next) {
        g_variant_builder_add (&builder, "s", name->data);
    }
    g_list_free (names);

    // append unique names
    names = g_hash_table_get_keys (dbus->unique_names);
    names = g_list_sort (names, (GCompareFunc) g_strcmp0);
    for (name = names; name != NULL; name = name->next) {
        g_variant_builder_add (&builder, "s", name->data);
    }
    g_list_free (names);

    g_dbus_method_invocation_return_value (invocation,
                    g_variant_new ("(as)", &builder));
}

static void
bus_dbus_impl_name_has_owner (BusDBusImpl           *dbus,
                              BusConnection         *connection,
                              GVariant              *parameters,
                              GDBusMethodInvocation *invocation)
{
    const gchar *name = NULL;
    g_variant_get (parameters, "(&s)", &name);

    gboolean has_owner;
    if (name[0] == ':') {
        has_owner = g_hash_table_lookup (dbus->unique_names, name) != NULL;
    }
    else {
        if (g_strcmp0 (name, "org.freedesktop.DBus") == 0 ||
            g_strcmp0 (name, "org.freedesktop.IBus") == 0)
            has_owner = TRUE;
        else
            has_owner = g_hash_table_lookup (dbus->names, name) != NULL;
    }
    g_dbus_method_invocation_return_value (invocation,
                    g_variant_new ("(b)", has_owner));
}

static void
bus_dbus_impl_get_name_owner (BusDBusImpl           *dbus,
                              BusConnection         *connection,
                              GVariant              *parameters,
                              GDBusMethodInvocation *invocation)
{
    const gchar *name_owner = NULL;
    const gchar *name = NULL;
    g_variant_get (parameters, "(&s)", &name);

    if (g_strcmp0 (name, "org.freedesktop.IBus") == 0 ||
        g_strcmp0 (name, "org.freedesktop.DBus") == 0) {
        name_owner = name;
    }
    else {
        BusConnection *owner = bus_dbus_impl_get_connection_by_name (dbus, name);
        if (owner != NULL) {
            name_owner = bus_connection_get_unique_name (owner);
        }
    }

    if (name_owner == NULL) {
        g_dbus_method_invocation_return_error (invocation,
                        G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER,
                        "Can not get name owner of '%s': no suce name", name);
    }
    else {
        g_dbus_method_invocation_return_value (invocation,
                        g_variant_new ("(s)", name_owner));
    }
}


static void
bus_dbus_impl_get_id (BusDBusImpl           *dbus,
                      BusConnection         *connection,
                      GVariant              *parameters,
                      GDBusMethodInvocation *invocation)
{
    /* FXIME */
    const gchar *uuid = "FXIME";
    g_dbus_method_invocation_return_value (invocation,
                    g_variant_new ("(s)", uuid));
}

static void
bus_dbus_impl_rule_destroy_cb (BusMatchRule *rule,
                           BusDBusImpl  *dbus)
{
    dbus->rules = g_slist_remove (dbus->rules, rule);
    g_object_unref (rule);
}

static void
bus_dbus_impl_add_match (BusDBusImpl           *dbus,
                         BusConnection         *connection,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation)
{
    const gchar *rule_text = NULL;
    g_variant_get (parameters, "(&s)", &rule_text);

    BusMatchRule *rule = bus_match_rule_new (rule_text);
    if (rule == NULL) {
        g_dbus_method_invocation_return_error (invocation,
                        G_DBUS_ERROR, G_DBUS_ERROR_MATCH_RULE_INVALID,
                        "Parse match rule [%s] failed", rule_text);
        return;
    }

    g_dbus_method_invocation_return_value (invocation, NULL);
    GSList *p;
    for (p = dbus->rules; p != NULL; p = p->next) {
        if (bus_match_rule_is_equal (rule, (BusMatchRule *)p->data)) {
            bus_match_rule_add_recipient ((BusMatchRule *)p->data, connection);
            g_object_unref (rule);
            return;
        }
    }

    if (rule) {
        bus_match_rule_add_recipient (rule, connection);
        dbus->rules = g_slist_append (dbus->rules, rule);
        g_signal_connect (rule, "destroy", G_CALLBACK (bus_dbus_impl_rule_destroy_cb), dbus);
    }
}

static void
bus_dbus_impl_remove_match (BusDBusImpl           *dbus,
                            BusConnection         *connection,
                            GVariant              *parameters,
                            GDBusMethodInvocation *invocation)
{
    const gchar *rule_text = NULL;
    g_variant_get (parameters, "(&s)", &rule_text);

    BusMatchRule *rule = bus_match_rule_new (rule_text);
    if (rule == NULL) {
        g_dbus_method_invocation_return_error (invocation,
                        G_DBUS_ERROR, G_DBUS_ERROR_MATCH_RULE_INVALID,
                        "Parse match rule [%s] failed", rule_text);
        return;
    }

    g_dbus_method_invocation_return_value (invocation, NULL);
    GSList *p;
    for (p = dbus->rules; p != NULL; p = p->next) {
        if (bus_match_rule_is_equal (rule, (BusMatchRule *)p->data)) {
            bus_match_rule_remove_recipient ((BusMatchRule *)p->data, connection);
            break;
        }
    }
    g_object_unref (rule);
}

static void
bus_dbus_impl_request_name (BusDBusImpl           *dbus,
                            BusConnection         *connection,
                            GVariant              *parameters,
                            GDBusMethodInvocation *invocation)
{
    /* FIXME need to handle flags */
    const gchar *name = NULL;
    guint flags = 0;
    g_variant_get (parameters, "(&su)", &name, &flags);

    if (name[0] == ':' || !g_dbus_is_name (name)) {
        g_dbus_method_invocation_return_error (invocation,
                        G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                        "'%s' is not a legal service name.", name);
        return;
    }

    if (g_strcmp0 (name, "org.freedesktop.DBus") == 0 ||
        g_strcmp0 (name, "org.freedesktop.IBus") == 0) {
        g_dbus_method_invocation_return_error (invocation,
                        G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                        "Can not acquire the service name '%s', it is reserved by IBus", name);
        return;
    }

    if (g_hash_table_lookup (dbus->names, name) != NULL) {
        g_dbus_method_invocation_return_error (invocation,
                        G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "Service name '%s' already has an owner.", name);
        return;
    }

    g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", 1));
    g_hash_table_insert (dbus->names,
                         (gpointer )bus_connection_add_name (connection, name),
                         connection);

    g_signal_emit (dbus,
                   dbus_signals[NAME_OWNER_CHANGED],
                   0,
                   name,
                   "",
                   bus_connection_get_unique_name (connection));
}

static void
bus_dbus_impl_release_name (BusDBusImpl           *dbus,
                            BusConnection         *connection,
                            GVariant              *parameters,
                            GDBusMethodInvocation *invocation)
{
    const gchar *name= NULL;
    g_variant_get (parameters, "(&s)", &name);

    if (name[0] == ':' || !g_dbus_is_name (name)) {
        g_dbus_method_invocation_return_error (invocation,
                        G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                        "'%s' is not a legal service name.", name);
        return;
    }

    if (g_strcmp0 (name, "org.freedesktop.DBus") == 0 ||
        g_strcmp0 (name, "org.freedesktop.IBus") == 0) {
        g_dbus_method_invocation_return_error (invocation,
                        G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                        "Service name '%s' is owned by bus.", name);
        return;
    }

    guint retval;
    if (g_hash_table_lookup (dbus->names, name) == NULL) {
        retval = 2;
    }
    else {
        if (bus_connection_remove_name (connection, name)) {
            retval = 1;
        }
        else {
            retval = 3;
        }
    }
    g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", retval));
}

static void
bus_dbus_impl_name_owner_changed (BusDBusImpl   *dbus,
                                  gchar         *name,
                                  gchar         *old_name,
                                  gchar         *new_name)
{
    g_assert (BUS_IS_DBUS_IMPL (dbus));
    g_assert (name != NULL);
    g_assert (old_name != NULL);
    g_assert (new_name != NULL);
    /* FIXME */
#if 0
    IBusMessage *message;

    message = ibus_message_new_signal (DBUS_PATH_DBUS,
                                       DBUS_INTERFACE_DBUS,
                                       "NameOwnerChanged");
    ibus_message_append_args (message,
                              G_TYPE_STRING, &name,
                              G_TYPE_STRING, &old_name,
                              G_TYPE_STRING, &new_name,
                              G_TYPE_INVALID);
    ibus_message_set_sender (message, DBUS_SERVICE_DBUS);

    bus_dbus_impl_dispatch_message_by_rule (dbus, message, NULL);

    ibus_message_unref (message);
#endif
}

static void
bus_dbus_impl_service_method_call (IBusService           *service,
                                   GDBusConnection       *dbus_connection,
                                   const gchar           *sender,
                                   const gchar           *object_path,
                                   const gchar           *interface_name,
                                   const gchar           *method_name,
                                   GVariant              *parameters,
                                   GDBusMethodInvocation *invocation)
{
    BusDBusImpl *dbus = BUS_DBUS_IMPL (service);

    BusConnection *connection = bus_connection_lookup (dbus_connection);
    g_assert (BUS_IS_CONNECTION (connection));

    if (g_strcmp0 (interface_name, "org.freedesktop.DBus") != 0) {
        IBUS_SERVICE_CLASS (bus_dbus_impl_parent_class)->service_method_call (
                                        (IBusService *) dbus,
                                        dbus_connection,
                                        sender,
                                        object_path,
                                        interface_name,
                                        method_name,
                                        parameters,
                                        invocation);
        return;
    }

    static const struct {
        const gchar *method_name;
        void (* method) (BusDBusImpl *, BusConnection *, GVariant *, GDBusMethodInvocation *);
    } methods[] =  {
        /* DBus interface */
        { "Hello",          bus_dbus_impl_hello },
        { "ListNames",      bus_dbus_impl_list_names },
        { "NameHasOwner",   bus_dbus_impl_name_has_owner },
        { "GetNameOwner",   bus_dbus_impl_get_name_owner },
        { "GetId",          bus_dbus_impl_get_id },
        { "AddMatch",       bus_dbus_impl_add_match },
        { "RemoveMatch",    bus_dbus_impl_remove_match },
        { "RequestName",    bus_dbus_impl_request_name },
        { "ReleaseName",    bus_dbus_impl_release_name },
    };

    gint i;
    for (i = 0; G_N_ELEMENTS (methods); i++) {
        if (g_strcmp0 (method_name, methods[i].method_name) == 0) {
            methods[i].method (dbus, connection, parameters, invocation);
            return;
        }
    }

    /* unsupport methods */
    g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
                    "org.freedesktop.DBus does not support %s", method_name);
}

#if 0
static void
bus_dbus_impl_service_method_call (IBusService           *service,
                                   GDBusConnection       *connection,
                                   const gchar           *sender,
                                   const gchar           *object_path,
                                   const gchar           *interface_name,
                                   const gchar           *method_name,
                                   GVariant              *parameters,
                                   GDBusMethodInvocation *invocation)
{

    /* FIXME */
#if 0
    const gchar *dest;

    if (G_UNLIKELY (IBUS_OBJECT_DESTROYED (dbus))) {
        return FALSE;
    }

    ibus_message_set_sender (message, bus_connection_get_unique_name (connection));

    switch (ibus_message_get_type (message)) {
#if 1
    case DBUS_MESSAGE_TYPE_ERROR:
        g_debug ("From :%s to %s, Error: %s : %s",
                 ibus_message_get_sender (message),
                 ibus_message_get_destination (message),
                 ibus_message_get_error_name (message),
                 ibus_message_get_error_message (message));
        break;
#endif
#if 1
    case DBUS_MESSAGE_TYPE_SIGNAL:
        g_debug ("From :%s to %s, Signal: %s @ %s",
                 ibus_message_get_sender (message),
                 ibus_message_get_destination (message),
                 ibus_message_get_member (message),
                 ibus_message_get_path (message)
                 );
        break;
#endif
#if 1
    case DBUS_MESSAGE_TYPE_METHOD_CALL:
        g_debug("From %s to %s, Method %s on %s",
                ibus_message_get_sender (message),
                ibus_message_get_destination (message),
                ibus_message_get_path (message),
                ibus_message_get_member (message));
        break;
#endif
    }

    dest = ibus_message_get_destination (message);

    if (dest == NULL ||
        strcmp ((gchar *)dest, IBUS_SERVICE_IBUS) == 0 ||
        strcmp ((gchar *)dest, DBUS_SERVICE_DBUS) == 0) {
        /* this message is sent to ibus-daemon */

        switch (ibus_message_get_type (message)) {
        case DBUS_MESSAGE_TYPE_SIGNAL:
        case DBUS_MESSAGE_TYPE_METHOD_RETURN:
        case DBUS_MESSAGE_TYPE_ERROR:
            bus_dbus_impl_dispatch_message_by_rule (dbus, message, NULL);
            return FALSE;
        case DBUS_MESSAGE_TYPE_METHOD_CALL:
            {
                const gchar *path;
                IBusService *object;

                path = ibus_message_get_path (message);

                object = g_hash_table_lookup (dbus->objects, path);

                if (object == NULL ||
                    ibus_service_handle_message (object,
                                                 (IBusConnection *) connection,
                                                 message) == FALSE) {
                    IBusMessage *error;
                    error = ibus_message_new_error_printf (message,
                                                           DBUS_ERROR_UNKNOWN_METHOD,
                                                           "Unknown method %s on %s",
                                                           ibus_message_get_member (message),
                                                           path);
                    ibus_connection_send ((IBusConnection *) connection, error);
                    ibus_message_unref (error);
                }

                /* dispatch message */
                bus_dbus_impl_dispatch_message_by_rule (dbus, message, NULL);
            }
            break;
        default:
            g_assert (FALSE);
        }
    }
    else {
        /* If the destination is not IBus or DBus, the message will be forwanded. */
        bus_dbus_impl_dispatch_message (dbus, message);
    }

    g_signal_stop_emission_by_name (connection, "ibus-message");
    return TRUE;
#endif
}
#endif

static GVariant *
bus_dbus_impl_service_get_property (IBusService        *service,
                                    GDBusConnection    *connection,
                                    const gchar        *sender,
                                    const gchar        *object_path,
                                    const gchar        *interface_name,
                                    const gchar        *property_name,
                                    GError            **error)
{
    return IBUS_SERVICE_CLASS (bus_dbus_impl_parent_class)->
                service_get_property (service,
                                      connection,
                                      sender,
                                      object_path,
                                      interface_name,
                                      property_name,
                                      error);
}

static gboolean
bus_dbus_impl_service_set_property (IBusService        *service,
                                    GDBusConnection    *connection,
                                    const gchar        *sender,
                                    const gchar        *object_path,
                                    const gchar        *interface_name,
                                    const gchar        *property_name,
                                    GVariant           *value,
                                    GError            **error)
{
    return IBUS_SERVICE_CLASS (bus_dbus_impl_parent_class)->
                service_set_property (service,
                                      connection,
                                      sender,
                                      object_path,
                                      interface_name,
                                      property_name,
                                      value,
                                      error);

}

/* FIXME */
#if 0
static void
_connection_ibus_message_sent_cb (BusConnection  *connection,
                                  IBusMessage    *message,
                                  BusDBusImpl    *dbus)
{
    bus_dbus_impl_dispatch_message_by_rule (dbus, message, connection);
}
#endif

BusDBusImpl *
bus_dbus_impl_get_default (void)
{
    static BusDBusImpl *dbus = NULL;

    if (dbus == NULL) {
        dbus = (BusDBusImpl *) g_object_new (BUS_TYPE_DBUS_IMPL,
                                             "object-path", "/org/freedesktop/DBus",
                                             NULL);
    }

    return dbus;
}

static void
bus_dbus_impl_connection_destroy_cb (BusConnection *connection,
                                     BusDBusImpl   *dbus)
{
    /* FIXME */
#if 0
    /*
    ibus_service_remove_from_connection (
                    IBUS_SERVICE (dbus),
                    IBUS_CONNECTION (connection));
    */

    const gchar *unique_name = bus_connection_get_unique_name (connection);
    if (unique_name != NULL) {
        g_hash_table_remove (dbus->unique_names, unique_name);
        g_signal_emit (dbus,
                       dbus_signals[NAME_OWNER_CHANGED],
                       0,
                       unique_name,
                       unique_name,
                       "");
    }

    const GList *name = bus_connection_get_names (connection);

    while (name != NULL) {
        g_hash_table_remove (dbus->names, name->data);
        g_signal_emit (dbus,
                       dbus_signals[NAME_OWNER_CHANGED],
                       0,
                       name->data,
                       unique_name,
                       "");
        name = name->next;
    }
#endif
    dbus->connections = g_slist_remove (dbus->connections, connection);
    g_object_unref (connection);
}


gboolean
bus_dbus_impl_new_connection (BusDBusImpl   *dbus,
                              BusConnection *connection)
{
    g_assert (BUS_IS_DBUS_IMPL (dbus));
    g_assert (BUS_IS_CONNECTION (connection));
    g_assert (g_slist_find (dbus->connections, connection) == NULL);

    g_object_ref_sink (connection);
    dbus->connections = g_slist_append (dbus->connections, connection);

    /* FIXME */
#if 0
    g_signal_connect (connection,
                      "ibus-message",
                      G_CALLBACK (_connection_ibus_message_cb),
                      dbus);

    g_signal_connect (connection,
                      "ibus-message-sent",
                      G_CALLBACK (_connection_ibus_message_sent_cb),
                      dbus);
#endif
    g_signal_connect (connection,
                      "destroy",
                      G_CALLBACK (bus_dbus_impl_connection_destroy_cb),
                      dbus);
    return TRUE;
}


BusConnection *
bus_dbus_impl_get_connection_by_name (BusDBusImpl    *dbus,
                                      const gchar    *name)
{
    g_assert (BUS_IS_DBUS_IMPL (dbus));
    g_assert (name != NULL);

    if (name[0] == ':') {
        return (BusConnection *)g_hash_table_lookup (dbus->unique_names, name);
    }
    else {
        return (BusConnection *)g_hash_table_lookup (dbus->names, name);
    }
}


void
bus_dbus_impl_dispatch_message (BusDBusImpl  *dbus,
                                GDBusMessage *message)
{
    /* FIXME */
#if 0
    g_assert (BUS_IS_DBUS_IMPL (dbus));
    g_assert (message != NULL);

    const gchar *destination;
    BusConnection *dest_connection = NULL;

    if (G_UNLIKELY (IBUS_OBJECT_DESTROYED (dbus))) {
        return;
    }

    destination = ibus_message_get_destination (message);

    if (destination != NULL) {
        dest_connection = bus_dbus_impl_get_connection_by_name (dbus, destination);

        if (dest_connection != NULL) {
            ibus_connection_send (IBUS_CONNECTION (dest_connection), message);
        }
        else {
            IBusMessage *reply_message;
            reply_message = ibus_message_new_error_printf (message,
                                                     DBUS_ERROR_SERVICE_UNKNOWN,
                                                     "Can not find service %s",
                                                     destination);
            bus_dbus_impl_dispatch_message (dbus, reply_message);
            ibus_message_unref (reply_message);
        }
    }

    bus_dbus_impl_dispatch_message_by_rule (dbus, message, dest_connection);
#endif
}

void
bus_dbus_impl_dispatch_message_by_rule (BusDBusImpl     *dbus,
                                        GDBusMessage    *message,
                                        GDBusConnection *skip_connection)
{
    /* FIXME */
#if 0
    g_assert (BUS_IS_DBUS_IMPL (dbus));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (skip_connection) || skip_connection == NULL);

    GList *recipients = NULL;
    GList *link = NULL;

    static gint32 data_slot = -1;

    if (G_UNLIKELY (IBUS_OBJECT_DESTROYED (dbus))) {
        return;
    }

    if (data_slot == -1) {
        dbus_message_allocate_data_slot (&data_slot);
    }

    /* If this message has been dispatched by rule, it will be ignored. */
    if (dbus_message_get_data (message, data_slot) != NULL)
        return;

    dbus_message_set_data (message, data_slot, (gpointer) TRUE, NULL);

    for (link = dbus->rules; link != NULL; link = link->next) {
        GList *list = bus_match_rule_get_recipients (BUS_MATCH_RULE (link->data),
                                                     message);
        recipients = g_list_concat (recipients, list);
    }

    for (link = recipients; link != NULL; link = link->next) {
        BusConnection *connection = BUS_CONNECTION (link->data);
        if (connection != skip_connection) {
            ibus_connection_send (IBUS_CONNECTION (connection), message);
        }
    }
    g_list_free (recipients);
#endif
}


static void
_object_destroy_cb (IBusService *object,
                    BusDBusImpl *dbus)
{
    bus_dbus_impl_unregister_object (dbus, object);
}


gboolean
bus_dbus_impl_register_object (BusDBusImpl *dbus,
                               IBusService *object)
{
    /* FIXME */
#if 0
    g_assert (BUS_IS_DBUS_IMPL (dbus));
    g_assert (IBUS_IS_SERVICE (object));

    const gchar *path;

    if (G_UNLIKELY (IBUS_OBJECT_DESTROYED (dbus))) {
        return FALSE;
    }

    path = ibus_service_get_path (object);

    g_return_val_if_fail (path, FALSE);

    g_return_val_if_fail  (g_hash_table_lookup (dbus->objects, path) == NULL, FALSE);

    g_object_ref_sink (object);
    g_hash_table_insert (dbus->objects, (gpointer)path, object);

    g_signal_connect (object, "destroy", G_CALLBACK (_object_destroy_cb), dbus);
#endif
    return TRUE;
}

gboolean
bus_dbus_impl_unregister_object (BusDBusImpl *dbus,
                                 IBusService *object)
{
    /* FIXME */
#if 0
    g_assert (BUS_IS_DBUS_IMPL (dbus));
    g_assert (IBUS_IS_SERVICE (object));

    const gchar *path;

    if (G_UNLIKELY (IBUS_OBJECT_DESTROYED (dbus))) {
        return FALSE;
    }

    path = ibus_service_get_path (object);
    g_return_val_if_fail (path, FALSE);

    g_return_val_if_fail  (g_hash_table_lookup (dbus->objects, path) == object, FALSE);

    g_signal_handlers_disconnect_by_func (object, G_CALLBACK (_object_destroy_cb), dbus);

    g_hash_table_remove (dbus->objects, path);
    g_object_unref (object);
#endif
    return TRUE;
}

