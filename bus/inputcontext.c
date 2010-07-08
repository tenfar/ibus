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
#include "inputcontext.h"
#include <ibusinternal.h>
#include "marshalers.h"
#include "ibusimpl.h"
#include "engineproxy.h"
#include "factoryproxy.h"

struct _BusInputContext {
    IBusService parent;

    /* instance members */
    BusConnection *connection;
    BusEngineProxy *engine;
    gchar *client;

    gboolean has_focus;
    gboolean enabled;

    /* capabilities */
    guint capabilities;

    /* cursor location */
    gint x;
    gint y;
    gint w;
    gint h;

    /* prev key event */
    guint prev_keyval;
    guint prev_modifiers;

    /* preedit text */
    IBusText *preedit_text;
    guint     preedit_cursor_pos;
    gboolean  preedit_visible;
    guint     preedit_mode;

    /* auxiliary text */
    IBusText *auxiliary_text;
    gboolean  auxiliary_visible;

    /* lookup table */
    IBusLookupTable *lookup_table;
    gboolean lookup_table_visible;

    /* filter release */
    gboolean filter_release;
};

struct _BusInputContextClass {
    IBusServiceClass parent;

    /* class members */
};

enum {
    PROCESS_KEY_EVENT,
    SET_CURSOR_LOCATION,
    FOCUS_IN,
    FOCUS_OUT,
    UPDATE_PREEDIT_TEXT,
    SHOW_PREEDIT_TEXT,
    HIDE_PREEDIT_TEXT,
    UPDATE_AUXILIARY_TEXT,
    SHOW_AUXILIARY_TEXT,
    HIDE_AUXILIARY_TEXT,
    UPDATE_LOOKUP_TABLE,
    SHOW_LOOKUP_TABLE,
    HIDE_LOOKUP_TABLE,
    PAGE_UP_LOOKUP_TABLE,
    PAGE_DOWN_LOOKUP_TABLE,
    CURSOR_UP_LOOKUP_TABLE,
    CURSOR_DOWN_LOOKUP_TABLE,
    REGISTER_PROPERTIES,
    UPDATE_PROPERTY,
    ENABLED,
    DISABLED,
    ENGINE_CHANGED,
    REQUEST_ENGINE,
    LAST_SIGNAL,
};

enum {
    PROP_0,
};

typedef struct _BusInputContextPrivate BusInputContextPrivate;

static guint    context_signals[LAST_SIGNAL] = { 0 };

/* functions prototype */
static void     bus_input_context_destroy       (BusInputContext        *context);
static void     bus_input_context_service_method_call
                                                (IBusService            *service,
                                                 GDBusConnection        *connection,
                                                 const gchar            *sender,
                                                 const gchar            *object_path,
                                                 const gchar            *interface_name,
                                                 const gchar            *method_name,
                                                 GVariant               *parameters,
                                                 GDBusMethodInvocation  *invocation);
/*
static GVariant *bus_input_context_service_get_property
                                                (IBusService            *service,
                                                 GDBusConnection        *connection,
                                                 const gchar            *sender,
                                                 const gchar            *object_path,
                                                 const gchar            *interface_name,
                                                 const gchar            *property_name,
                                                 GError                **error);
static gboolean bus_input_context_service_set_property
                                                (IBusService            *service,
                                                 GDBusConnection        *connection,
                                                 const gchar            *sender,
                                                 const gchar            *object_path,
                                                 const gchar            *interface_name,
                                                 const gchar            *property_name,
                                                 GVariant               *value,
                                                 GError                **error);
*/
static gboolean bus_input_context_filter_keyboard_shortcuts
                                                (BusInputContext        *context,
                                                 guint                   keyval,
                                                 guint                   keycode,
                                                 guint                   modifiers);

static void     bus_input_context_unset_engine  (BusInputContext        *context);
static void     bus_input_context_commit_text   (BusInputContext        *context,
                                                 IBusText               *text);
static void     bus_input_context_update_preedit_text
                                                (BusInputContext        *context,
                                                 IBusText               *text,
                                                 guint                   cursor_pos,
                                                 gboolean                visible,
                                                 guint                   mode);
static void     bus_input_context_show_preedit_text
                                                (BusInputContext        *context);
static void     bus_input_context_hide_preedit_text
                                                (BusInputContext        *context);
static void     bus_input_context_update_auxiliary_text
                                                (BusInputContext        *context,
                                                 IBusText               *text,
                                                 gboolean                visible);
static void     bus_input_context_show_auxiliary_text
                                                (BusInputContext        *context);
static void     bus_input_context_hide_auxiliary_text
                                                (BusInputContext        *context);
static void     bus_input_context_update_lookup_table
                                                (BusInputContext        *context,
                                                 IBusLookupTable        *table,
                                                 gboolean                visible);
static void     bus_input_context_show_lookup_table
                                                (BusInputContext        *context);
static void     bus_input_context_hide_lookup_table
                                                (BusInputContext        *context);
static void     bus_input_context_page_up_lookup_table
                                                (BusInputContext        *context);
static void     bus_input_context_page_down_lookup_table
                                                (BusInputContext        *context);
static void     bus_input_context_cursor_up_lookup_table
                                                (BusInputContext        *context);
static void     bus_input_context_cursor_down_lookup_table
                                                (BusInputContext        *context);
static void     bus_input_context_register_properties
                                                (BusInputContext        *context,
                                                 IBusPropList           *props);
static void     bus_input_context_update_property
                                                (BusInputContext        *context,
                                                 IBusProperty           *prop);
static void     _engine_destroy_cb              (BusEngineProxy         *factory,
                                                 BusInputContext        *context);

static guint id = 0;
static IBusText *text_empty = NULL;
static IBusLookupTable *lookup_table_empty = NULL;
static IBusPropList    *props_empty = NULL;
static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.freedesktop.IBus.InputContext'>"
    /* methods */
    "    <method name='ProcessKeyEvent'>"
    "      <arg direction='in'  type='u' name='keyval' />"
    "      <arg direction='in'  type='u' name='keycode' />"
    "      <arg direction='in'  type='u' name='state' />"
    "      <arg direction='out' type='b' name='handled' />"
    "    </method>"
    "    <method name='SetCursorLocation'>"
    "      <arg direction='in' type='i' name='x' />"
    "      <arg direction='in' type='i' name='y' />"
    "      <arg direction='in' type='i' name='w' />"
    "      <arg direction='in' type='i' name='h' />"
    "    </method>"
    "    <method name='FocusIn' />"
    "    <method name='FocusOut' />"
    "    <method name='Reset' />"
    "    <method name='Enable' />"
    "    <method name='Disable' />"
    "    <method name='IsEnabled'>"
    "      <arg direction='out' type='b' name='enable' />"
    "    </method>"
    "    <method name='SetCapabilities'>"
    "      <arg direction='in' type='u' name='caps' />"
    "    </method>"
    "    <method name='PropertyActivate'>"
    "      <arg direction='in' type='s' name='name' />"
    "      <arg direction='in' type='i' name='state' />"
    "    </method>"
    "    <method name='SetEngine'>"
    "      <arg direction='in' type='s' name='name' />"
    "    </method>"
    "    <method name='GetEngine'>"
    "      <arg direction='out' type='v' name='desc' />"
    "    </method>"
    /* signals */
    "    <signal name='CommitText'>"
    "      <arg type='v' name='text' />"
    "    </signal>"
    "    <signal name='Enabled'/>"
    "    <signal name='Disabled'/>"
    "    <signal name='ForwardKeyEvent'>"
    "      <arg type='u' name='keyval' />"
    "      <arg type='u' name='keycode' />"
    "      <arg type='u' name='state' />"
    "    </signal>"
    "    <signal name='UpdatePreeditText'>"
    "      <arg type='v' name='text' />"
    "      <arg type='u' name='cursor_pos' />"
    "      <arg type='b' name='visible' />"
    "    </signal>"
    "    <signal name='ShowPreeditText'/>"
    "    <signal name='HidePreeditText'/>"
    "    <signal name='UpdateAuxiliaryText'>"
    "      <arg type='v' name='text' />"
    "      <arg type='b' name='visible' />"
    "    </signal>"
    "    <signal name='ShowAuxiliaryText'/>"
    "    <signal name='HideAuxiliaryText'/>"
    "    <signal name='UpdateLookupTable'>"
    "      <arg type='v' name='table' />"
    "      <arg type='b' name='visible' />"
    "    </signal>"
    "    <signal name='ShowLookupTable'/>"
    "    <signal name='HideLookupTable'/>"
    "    <signal name='PageUpLookupTable'/>"
    "    <signal name='PageDownLookupTable'/>"
    "    <signal name='CursorUpLookupTable'/>"
    "    <signal name='CursorDownLookupTable'/>"
    "    <signal name='RegisterProperties'>"
    "      <arg type='v' name='props' />"
    "    </signal>"
    "    <signal name='UpdateProperty'>"
    "      <arg type='v' name='prop' />"
    "    </signal>"
    "  </interface>"
    "</node>";

G_DEFINE_TYPE (BusInputContext, bus_input_context, IBUS_TYPE_SERVICE)

/* when send preedit to client */
/* FIXME */
#if 1
#define PREEDIT_CONDITION (context->capabilities & IBUS_CAP_PREEDIT_TEXT)
#else
#define PREEDIT_CONDITION  \
    ((context->capabilities & IBUS_CAP_PREEDIT_TEXT) && \
    (BUS_DEFAULT_IBUS->embed_preedit_text || (context->capabilities & IBUS_CAP_FOCUS) == 0))
#endif

static void
_connection_destroy_cb (BusConnection   *connection,
                        BusInputContext *context)
{
    BUS_IS_CONNECTION (connection);
    BUS_IS_INPUT_CONTEXT (context);

    ibus_object_destroy (IBUS_OBJECT (context));
}




static void
bus_input_context_class_init (BusInputContextClass *class)
{
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (class);

    ibus_object_class->destroy = (IBusObjectDestroyFunc) bus_input_context_destroy;

    IBUS_SERVICE_CLASS (class)->service_method_call = bus_input_context_service_method_call;
    ibus_service_class_add_interfaces (IBUS_SERVICE_CLASS (class), introspection_xml);

    /* install signals */
    context_signals[PROCESS_KEY_EVENT] =
        g_signal_new (I_("process-key-event"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_BOOL__UINT_UINT_UINT,
            G_TYPE_BOOLEAN,
            3,
            G_TYPE_UINT,
            G_TYPE_UINT,
            G_TYPE_UINT);

    context_signals[SET_CURSOR_LOCATION] =
        g_signal_new (I_("set-cursor-location"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__INT_INT_INT_INT,
            G_TYPE_NONE,
            4,
            G_TYPE_INT,
            G_TYPE_INT,
            G_TYPE_INT,
            G_TYPE_INT);

    context_signals[FOCUS_IN] =
        g_signal_new (I_("focus-in"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[FOCUS_OUT] =
        g_signal_new (I_("focus-out"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[UPDATE_PREEDIT_TEXT] =
        g_signal_new (I_("update-preedit-text"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT_UINT_BOOLEAN,
            G_TYPE_NONE,
            3,
            IBUS_TYPE_TEXT,
            G_TYPE_UINT,
            G_TYPE_BOOLEAN);

    context_signals[SHOW_PREEDIT_TEXT] =
        g_signal_new (I_("show-preedit-text"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    context_signals[HIDE_PREEDIT_TEXT] =
        g_signal_new (I_("hide-preedit-text"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    context_signals[UPDATE_AUXILIARY_TEXT] =
        g_signal_new (I_("update-auxiliary-text"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT_BOOLEAN,
            G_TYPE_NONE,
            2,
            IBUS_TYPE_TEXT,
            G_TYPE_BOOLEAN);

    context_signals[SHOW_AUXILIARY_TEXT] =
        g_signal_new (I_("show-auxiliary-text"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    context_signals[HIDE_AUXILIARY_TEXT] =
        g_signal_new (I_("hide-auxiliary-text"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    context_signals[UPDATE_LOOKUP_TABLE] =
        g_signal_new (I_("update-lookup-table"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT_BOOLEAN,
            G_TYPE_NONE,
            2,
            IBUS_TYPE_LOOKUP_TABLE,
            G_TYPE_BOOLEAN);

    context_signals[SHOW_LOOKUP_TABLE] =
        g_signal_new (I_("show-lookup-table"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[HIDE_LOOKUP_TABLE] =
        g_signal_new (I_("hide-lookup-table"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[PAGE_UP_LOOKUP_TABLE] =
        g_signal_new (I_("page-up-lookup-table"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[PAGE_DOWN_LOOKUP_TABLE] =
        g_signal_new (I_("page-down-lookup-table"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[CURSOR_UP_LOOKUP_TABLE] =
        g_signal_new (I_("cursor-up-lookup-table"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[CURSOR_DOWN_LOOKUP_TABLE] =
        g_signal_new (I_("cursor-down-lookup-table"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[REGISTER_PROPERTIES] =
        g_signal_new (I_("register-properties"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT,
            G_TYPE_NONE,
            1,
            IBUS_TYPE_PROP_LIST);

    context_signals[UPDATE_PROPERTY] =
        g_signal_new (I_("update-property"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT,
            G_TYPE_NONE,
            1,
            IBUS_TYPE_PROPERTY);

    context_signals[ENABLED] =
        g_signal_new (I_("enabled"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    context_signals[DISABLED] =
        g_signal_new (I_("disabled"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    context_signals[ENGINE_CHANGED] =
        g_signal_new (I_("engine-changed"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    context_signals[REQUEST_ENGINE] =
        g_signal_new (I_("request-engine"),
            G_TYPE_FROM_CLASS (class),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING);

    text_empty = ibus_text_new_from_string ("");
    g_object_ref_sink (text_empty);
    lookup_table_empty = ibus_lookup_table_new (9, 0, FALSE, FALSE);
    g_object_ref_sink (lookup_table_empty);
    props_empty = ibus_prop_list_new ();
    g_object_ref_sink (props_empty);
}

static void
bus_input_context_init (BusInputContext *context)
{
    context->connection = NULL;
    context->client = NULL;
    context->engine = NULL;
    context->has_focus = FALSE;
    context->enabled = FALSE;

    context->prev_keyval = IBUS_VoidSymbol;
    context->prev_modifiers = 0;

    context->capabilities = 0;

    context->x = 0;
    context->y = 0;
    context->w = 0;
    context->h = 0;

    g_object_ref_sink (text_empty);
    context->preedit_text = text_empty;
    context->preedit_cursor_pos = 0;
    context->preedit_visible = FALSE;
    context->preedit_mode = IBUS_ENGINE_PREEDIT_CLEAR;

    g_object_ref_sink (text_empty);
    context->auxiliary_text = text_empty;
    context->auxiliary_visible = FALSE;

    g_object_ref_sink (lookup_table_empty);
    context->lookup_table = lookup_table_empty;
    context->lookup_table_visible = FALSE;

}

static void
bus_input_context_destroy (BusInputContext *context)
{
    if (context->has_focus) {
        bus_input_context_focus_out (context);
        context->has_focus = FALSE;
    }

    if (context->engine) {
        bus_input_context_unset_engine (context);
    }

    if (context->preedit_text) {
        g_object_unref (context->preedit_text);
        context->preedit_text = NULL;
    }

    if (context->auxiliary_text) {
        g_object_unref (context->auxiliary_text);
        context->auxiliary_text = NULL;
    }

    if (context->lookup_table) {
        g_object_unref (context->lookup_table);
        context->lookup_table = NULL;
    }

    if (context->connection) {
        g_signal_handlers_disconnect_by_func (context->connection,
                                         (GCallback) _connection_destroy_cb,
                                         context);
        g_object_unref (context->connection);
        context->connection = NULL;
    }

    if (context->client) {
        g_free (context->client);
        context->client = NULL;
    }

    IBUS_OBJECT_CLASS(bus_input_context_parent_class)->destroy (IBUS_OBJECT (context));
}

static void
_ic_process_key_event_reply_cb (GObject      *source,
                                GAsyncResult *result,
                                gpointer      user_data)
{
    GError *error = NULL;
    GVariant *retval = g_dbus_proxy_call_finish ((GDBusProxy *)source,
                    result, &error);
    if (retval != NULL) {
        g_dbus_method_invocation_return_value ((GDBusMethodInvocation *)user_data, retval);
    }
    else {
        g_dbus_method_invocation_return_gerror ((GDBusMethodInvocation *)user_data, error);
        g_error_free (error);
    }
}

static void
_ic_process_key_event  (BusInputContext       *context,
                        GVariant              *parameters,
                        GDBusMethodInvocation *invocation)
{
    guint keyval = IBUS_VoidSymbol;
    guint keycode = 0;
    guint modifiers = 0;

    g_variant_get (parameters, "(uuu)", &keyval, &keycode, &modifiers);
    if (G_UNLIKELY (!context->has_focus)) {
        /* workaround: set focus if context does not have focus */
        bus_input_context_focus_in (context);
    }

    if (G_LIKELY (context->has_focus)) {
        gboolean retval = bus_input_context_filter_keyboard_shortcuts (context, keyval, keycode, modifiers);
        /* If it is keyboard shortcut, reply TRUE to client */
        if (G_UNLIKELY (retval)) {
            g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
            return;
        }
    }

    if (context->has_focus && context->enabled && context->engine) {
        bus_engine_proxy_process_key_event (context->engine,
                                            keyval,
                                            keycode,
                                            modifiers,
                                            _ic_process_key_event_reply_cb,
                                            invocation);
    }
    else {
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
    }
}

static void
_ic_set_cursor_location (BusInputContext       *context,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation)
{
    g_dbus_method_invocation_return_value (invocation, NULL);

    g_variant_get (parameters, "(iiii)",
                    &context->x, &context->y, &context->w, &context->h);

    if (context->has_focus && context->enabled && context->engine) {
        bus_engine_proxy_set_cursor_location (context->engine,
                        context->x, context->y, context->w, context->h);
    }

    if (context->capabilities & IBUS_CAP_FOCUS) {
        g_signal_emit (context,
                       context_signals[SET_CURSOR_LOCATION],
                       0,
                       context->x,
                       context->y,
                       context->w,
                       context->h);
    }
}

static void
_ic_focus_in (BusInputContext       *context,
              GVariant              *parameters,
              GDBusMethodInvocation *invocation)
{
    if (context->capabilities & IBUS_CAP_FOCUS) {
        bus_input_context_focus_in (context);
        g_dbus_method_invocation_return_value (invocation, NULL);
    }
    else {
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "The input context does not support focus.");
    }
}

static void
_ic_focus_out (BusInputContext       *context,
               GVariant              *parameters,
               GDBusMethodInvocation *invocation)
{
    if (context->capabilities & IBUS_CAP_FOCUS) {
        bus_input_context_focus_out (context);
        g_dbus_method_invocation_return_value (invocation, NULL);
    }
    else {
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "The input context does not support focus.");
    }
}

static void
_ic_reset (BusInputContext       *context,
           GVariant              *parameters,
           GDBusMethodInvocation *invocation)
{
    if (context->enabled && context->engine) {
        bus_engine_proxy_reset (context->engine);
    }
    g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
_ic_set_capabilities (BusInputContext       *context,
                      GVariant              *parameters,
                      GDBusMethodInvocation *invocation)
{
    guint caps = 0;
    g_variant_get (parameters, "(u)", &caps);

    if (context->capabilities != caps) {
        context->capabilities = caps;

        /* If the context does not support IBUS_CAP_FOCUS, then we always assume
         * it has focus. */
        if ((caps & IBUS_CAP_FOCUS) == 0) {
            bus_input_context_focus_in (context);
        }

        if (context->engine) {
            bus_engine_proxy_set_capabilities (context->engine, caps);
        }
    }
    g_dbus_method_invocation_return_value (invocation, NULL);
}


static void
_ic_property_activate (BusInputContext       *context,
                       GVariant              *parameters,
                       GDBusMethodInvocation *invocation)
{
    gchar *prop_name = NULL;
    gint prop_state = 0;
    g_variant_get (parameters, "(&su)", &prop_name, &prop_state);

    if (context->enabled && context->engine) {
        bus_engine_proxy_property_activate (context->engine, prop_name, prop_state);
    }
    g_dbus_method_invocation_return_value (invocation, NULL);
}


static void
_ic_enable (BusInputContext       *context,
            GVariant              *parameters,
            GDBusMethodInvocation *invocation)
{
    bus_input_context_enable (context);
    g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
_ic_disable (BusInputContext       *context,
             GVariant              *parameters,
             GDBusMethodInvocation *invocation)
{
    bus_input_context_disable (context);
    g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
_ic_is_enabled (BusInputContext       *context,
                GVariant              *parameters,
                GDBusMethodInvocation *invocation)
{
    g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", context->enabled));
}

static void
_ic_set_engine (BusInputContext       *context,
                GVariant              *parameters,
                GDBusMethodInvocation *invocation)
{
    gchar *engine_name = NULL;
    g_variant_get (parameters, "(&s)", &engine_name);

    g_signal_emit (context, context_signals[REQUEST_ENGINE], 0, engine_name);

    if (context->engine == NULL) {
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "Can not find engine '%s'.", engine_name);
    }
    else {
        bus_input_context_enable (context);
        g_dbus_method_invocation_return_value (invocation, NULL);
    }
}

static void
_ic_get_engine (BusInputContext       *context,
                GVariant              *parameters,
                GDBusMethodInvocation *invocation)
{
    if (context->engine) {
        IBusEngineDesc *desc = bus_engine_proxy_get_desc (context->engine);
        g_dbus_method_invocation_return_value (invocation,
                        g_variant_new ("(v)", ibus_serializable_serialize ((IBusSerializable *)desc)));
    }
    else {
        g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                        "Input context does have engine.");
    }
}

static void
bus_input_context_service_method_call (IBusService            *service,
                                       GDBusConnection        *connection,
                                       const gchar            *sender,
                                       const gchar            *object_path,
                                       const gchar            *interface_name,
                                       const gchar            *method_name,
                                       GVariant               *parameters,
                                       GDBusMethodInvocation  *invocation)
{
    if (g_strcmp0 (interface_name, "org.freedesktop.IBus.InputContext") != 0) {
        IBUS_SERVICE_CLASS (bus_input_context_parent_class)->service_method_call (
                        service,
                        connection,
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
        void (* method_callback) (BusInputContext *, GVariant *, GDBusMethodInvocation *);
    } methods [] =  {
        { "ProcessKeyEvent",   _ic_process_key_event },
        { "SetCursorLocation", _ic_set_cursor_location },
        { "FocusIn",           _ic_focus_in },
        { "FocusOut",          _ic_focus_out },
        { "Reset",             _ic_reset },
        { "SetCapabilities",   _ic_set_capabilities },
        { "PropertyActivate",  _ic_property_activate },
        { "Enable",            _ic_enable },
        { "Disable",           _ic_disable },
        { "IsEnabled",         _ic_is_enabled },
        { "SetEngine",         _ic_set_engine },
        { "GetEngine",         _ic_get_engine },
    };

    gint i;
    for (i = 0; i < G_N_ELEMENTS (methods); i++) {
        if (g_strcmp0 (method_name, methods[i].method_name) == 0) {
            methods[i].method_callback ((BusInputContext *)service, parameters, invocation);
            return;
        }
    }

    g_return_if_reached ();
}


gboolean
bus_input_context_has_focus (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    return context->has_focus;
}

void
bus_input_context_focus_in (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->has_focus)
        return;

    context->has_focus = TRUE;

    if (context->engine == NULL && context->enabled) {
        g_signal_emit (context, context_signals[REQUEST_ENGINE], 0, NULL);
    }

    if (context->engine && context->enabled) {
        bus_engine_proxy_focus_in (context->engine);
        bus_engine_proxy_enable (context->engine);
        bus_engine_proxy_set_capabilities (context->engine, context->capabilities);
        bus_engine_proxy_set_cursor_location (context->engine, context->x, context->y, context->w, context->h);
    }

    if (context->capabilities & IBUS_CAP_FOCUS) {
        g_signal_emit (context, context_signals[FOCUS_IN], 0);
        if (context->engine && context->enabled) {
          if (context->preedit_visible && !PREEDIT_CONDITION) {
                g_signal_emit (context,
                               context_signals[UPDATE_PREEDIT_TEXT],
                               0,
                               context->preedit_text,
                               context->preedit_cursor_pos,
                               context->preedit_visible);
            }
            if (context->auxiliary_visible && (context->capabilities & IBUS_CAP_AUXILIARY_TEXT) == 0) {
                g_signal_emit (context,
                               context_signals[UPDATE_AUXILIARY_TEXT],
                               0,
                               context->auxiliary_text,
                               context->auxiliary_visible);
            }
            if (context->lookup_table_visible && (context->capabilities & IBUS_CAP_LOOKUP_TABLE) == 0) {
                g_signal_emit (context,
                               context_signals[UPDATE_LOOKUP_TABLE],
                               0,
                               context->lookup_table,
                               context->lookup_table_visible);
            }
        }
    }
}

static void
bus_input_context_clear_preedit_text (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->preedit_visible &&
        context->preedit_mode == IBUS_ENGINE_PREEDIT_COMMIT) {
        bus_input_context_commit_text (context, context->preedit_text);
    }

    /* always clear preedit text */
    bus_input_context_update_preedit_text (context,
        text_empty, 0, FALSE, IBUS_ENGINE_PREEDIT_CLEAR);
}

void
bus_input_context_focus_out (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!context->has_focus)
        return;

    bus_input_context_clear_preedit_text (context);
    bus_input_context_update_auxiliary_text (context, text_empty, FALSE);
    bus_input_context_update_lookup_table (context, lookup_table_empty, FALSE);
    bus_input_context_register_properties (context, props_empty);

    if (context->engine && context->enabled) {
        bus_engine_proxy_focus_out (context->engine);
    }

    context->has_focus = FALSE;

    if (context->capabilities & IBUS_CAP_FOCUS) {
        g_signal_emit (context, context_signals[FOCUS_OUT], 0);
    }
}

#define DEFINE_FUNC(name)                                                   \
    void                                                                    \
    bus_input_context_##name (BusInputContext *context)                     \
    {                                                                       \
        g_assert (BUS_IS_INPUT_CONTEXT (context));                          \
                                                                            \
        if (context->has_focus && context->enabled && context->engine) {    \
            bus_engine_proxy_##name (context->engine);                      \
        }                                                                   \
    }

DEFINE_FUNC(page_up)
DEFINE_FUNC(page_down)
DEFINE_FUNC(cursor_up)
DEFINE_FUNC(cursor_down)

#undef DEFINE_FUNC

void
bus_input_context_candidate_clicked (BusInputContext *context,
                                     guint            index,
                                     guint            button,
                                     guint            state)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->enabled && context->engine) {
        bus_engine_proxy_candidate_clicked (context->engine,
                                            index,
                                            button,
                                            state);
    }
}

void
bus_input_context_property_activate (BusInputContext *context,
                                     const gchar     *prop_name,
                                     gint             prop_state)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->enabled && context->engine) {
        bus_engine_proxy_property_activate (context->engine, prop_name, prop_state);
    }
}

static void
bus_input_context_commit_text (BusInputContext *context,
                               IBusText        *text)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!context->enabled)
        return;

    if (text == text_empty || text == NULL)
        return;

    GVariant *variant = ibus_serializable_serialize ((IBusSerializable *)text);
    ibus_service_emit_signal ((IBusService *)context,
                              NULL,
                              "org.freedesktop.IBus.InputContext",
                              "CommitText",
                              g_variant_new ("(v)", variant),
                              NULL);
}

static void
bus_input_context_update_preedit_text (BusInputContext *context,
                                       IBusText        *text,
                                       guint            cursor_pos,
                                       gboolean         visible,
                                       guint            mode)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->preedit_text) {
        g_object_unref (context->preedit_text);
    }

    context->preedit_text = (IBusText *) g_object_ref_sink (text ? text : text_empty);
    context->preedit_cursor_pos = cursor_pos;
    context->preedit_visible = visible;
    context->preedit_mode = mode;

    if (PREEDIT_CONDITION) {
        GVariant *variant = ibus_serializable_serialize ((IBusSerializable *)context->preedit_text);
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "UpdatePreeditText",
                                  g_variant_new ("(vub)", variant, context->preedit_cursor_pos, context->preedit_visible),
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[UPDATE_PREEDIT_TEXT],
                       0,
                       context->preedit_text,
                       context->preedit_cursor_pos,
                       context->preedit_visible);
    }
}

static void
bus_input_context_show_preedit_text (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->preedit_visible) {
        return;
    }

    context->preedit_visible = TRUE;

    if (PREEDIT_CONDITION) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "ShowPreeditText",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[SHOW_PREEDIT_TEXT],
                       0);
    }
}

static void
bus_input_context_hide_preedit_text (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!context->preedit_visible) {
        return;
    }

    context->preedit_visible = FALSE;

    if (PREEDIT_CONDITION) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "HidePreeditText",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[HIDE_PREEDIT_TEXT],
                       0);
    }
}

static void
bus_input_context_update_auxiliary_text (BusInputContext *context,
                                         IBusText        *text,
                                         gboolean         visible)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->auxiliary_text) {
        g_object_unref (context->auxiliary_text);
    }

    context->auxiliary_text = (IBusText *) g_object_ref_sink (text ? text : text_empty);
    context->auxiliary_visible = visible;

    if (context->capabilities & IBUS_CAP_AUXILIARY_TEXT) {
        GVariant *variant = ibus_serializable_serialize ((IBusSerializable *)text);
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "UpdateAuxiliaryText",
                                  g_variant_new ("(vb)", variant, visible),
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[UPDATE_AUXILIARY_TEXT],
                       0,
                       context->auxiliary_text,
                       context->auxiliary_visible);
    }
}

static void
bus_input_context_show_auxiliary_text (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->auxiliary_visible) {
        return;
    }

    context->auxiliary_visible = TRUE;

    if ((context->capabilities & IBUS_CAP_AUXILIARY_TEXT) == IBUS_CAP_AUXILIARY_TEXT) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "ShowAuxiliaryText",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[SHOW_AUXILIARY_TEXT],
                       0);
    }
}

static void
bus_input_context_hide_auxiliary_text (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!context->auxiliary_visible) {
        return;
    }

    context->auxiliary_visible = FALSE;

    if ((context->capabilities & IBUS_CAP_AUXILIARY_TEXT) == IBUS_CAP_AUXILIARY_TEXT) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "HideAuxiliaryText",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[HIDE_AUXILIARY_TEXT],
                       0);
    }
}

static void
bus_input_context_update_lookup_table (BusInputContext *context,
                                       IBusLookupTable *table,
                                       gboolean         visible)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->lookup_table) {
        g_object_unref (context->lookup_table);
    }

    context->lookup_table = (IBusLookupTable *) g_object_ref_sink (table ? table : lookup_table_empty);
    context->lookup_table_visible = visible;

    if (context->capabilities & IBUS_CAP_LOOKUP_TABLE) {
        GVariant *variant = ibus_serializable_serialize ((IBusSerializable *)table);
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "UpdateLookupTable",
                                  g_variant_new ("(vb)", variant, visible),
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[UPDATE_LOOKUP_TABLE],
                       0,
                       context->lookup_table,
                       context->lookup_table_visible);
    }
}

static void
bus_input_context_show_lookup_table (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->lookup_table_visible) {
        return;
    }

    context->lookup_table_visible = TRUE;

    if ((context->capabilities & IBUS_CAP_LOOKUP_TABLE) == IBUS_CAP_LOOKUP_TABLE) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "ShowLookupTable",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[SHOW_LOOKUP_TABLE],
                       0);
    }
}

static void
bus_input_context_hide_lookup_table (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!context->lookup_table_visible) {
        return;
    }

    context->lookup_table_visible = FALSE;

    if ((context->capabilities & IBUS_CAP_LOOKUP_TABLE) == IBUS_CAP_LOOKUP_TABLE) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "HideLookupTable",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[HIDE_LOOKUP_TABLE],
                       0);
    }
}

static void
bus_input_context_page_up_lookup_table (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!ibus_lookup_table_page_up (context->lookup_table)) {
        return;
    }

    if ((context->capabilities & IBUS_CAP_LOOKUP_TABLE) == IBUS_CAP_LOOKUP_TABLE) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "PageUpLookupTable",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[PAGE_UP_LOOKUP_TABLE],
                       0);
    }
}

static void
bus_input_context_page_down_lookup_table (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!ibus_lookup_table_page_down (context->lookup_table)) {
        return;
    }

    if ((context->capabilities & IBUS_CAP_LOOKUP_TABLE) == IBUS_CAP_LOOKUP_TABLE) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "PageDownLookupTable",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[PAGE_DOWN_LOOKUP_TABLE],
                       0);
    }
}

static void
bus_input_context_cursor_up_lookup_table (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!ibus_lookup_table_cursor_up (context->lookup_table)) {
        return;
    }

    if ((context->capabilities & IBUS_CAP_LOOKUP_TABLE) == IBUS_CAP_LOOKUP_TABLE) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "CursorUpLookupTable",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[CURSOR_UP_LOOKUP_TABLE],
                       0);
    }
}

static void
bus_input_context_cursor_down_lookup_table (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!ibus_lookup_table_cursor_down (context->lookup_table)) {
        return;
    }

    if ((context->capabilities & IBUS_CAP_LOOKUP_TABLE) == IBUS_CAP_LOOKUP_TABLE) {
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "CursorDownLookupTable",
                                  NULL,
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[CURSOR_DOWN_LOOKUP_TABLE],
                       0);
    }
}

static void
bus_input_context_register_properties (BusInputContext *context,
                                       IBusPropList    *props)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (IBUS_IS_PROP_LIST (props));

    if (context->capabilities & IBUS_CAP_PROPERTY) {
        GVariant *variant = ibus_serializable_serialize ((IBusSerializable *)props);
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "RegisterProperties",
                                  g_variant_new ("(v)", variant),
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[REGISTER_PROPERTIES],
                       0,
                       props);
    }
}

static void
bus_input_context_update_property (BusInputContext *context,
                                   IBusProperty    *prop)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (IBUS_IS_PROPERTY (prop));

    if (context->capabilities & IBUS_CAP_PROPERTY) {
        GVariant *variant = ibus_serializable_serialize ((IBusSerializable *)prop);
        ibus_service_emit_signal ((IBusService *)context,
                                  NULL,
                                  "org.freedesktop.IBus.InputContext",
                                  "UpdateProperty",
                                  g_variant_new ("(v)", variant),
                                  NULL);
    }
    else {
        g_signal_emit (context,
                       context_signals[UPDATE_PROPERTY],
                       0,
                       prop);
    }
}

static void
_engine_destroy_cb (BusEngineProxy  *engine,
                    BusInputContext *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    bus_input_context_set_engine (context, NULL);
}

static void
_engine_commit_text_cb (BusEngineProxy  *engine,
                        IBusText        *text,
                        BusInputContext *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (text != NULL);
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    bus_input_context_commit_text (context, text);
}

static void
_engine_forward_key_event_cb (BusEngineProxy    *engine,
                              guint              keyval,
                              guint              keycode,
                              guint              state,
                              BusInputContext   *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    if (!context->enabled)
        return;

    ibus_service_emit_signal ((IBusService *)context,
                              NULL,
                              "org.freedesktop.IBus.InputContext",
                              "ForwardKeyEvent",
                              g_variant_new ("(uuu)", keyval, keycode, state),
                              NULL);
}

static void
_engine_delete_surrounding_text_cb (BusEngineProxy    *engine,
                                    gint               offset_from_cursor,
                                    guint              nchars,
                                    BusInputContext   *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    if (!context->enabled)
        return;

    ibus_service_emit_signal ((IBusService *)context,
                              NULL,
                              "org.freedesktop.IBus.InputContext",
                              "DeleteSurroundingText",
                              g_variant_new ("(iu)", offset_from_cursor, nchars),
                              NULL);
}

static void
_engine_update_preedit_text_cb (BusEngineProxy  *engine,
                                IBusText        *text,
                                guint            cursor_pos,
                                gboolean         visible,
                                guint            mode,
                                BusInputContext *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (IBUS_IS_TEXT (text));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    if (!context->enabled)
        return;

    bus_input_context_update_preedit_text (context, text, cursor_pos, visible, mode);
}

static void
_engine_update_auxiliary_text_cb (BusEngineProxy   *engine,
                                  IBusText         *text,
                                  gboolean          visible,
                                  BusInputContext  *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (IBUS_IS_TEXT (text));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    if (!context->enabled)
        return;

    bus_input_context_update_auxiliary_text (context, text, visible);
}

static void
_engine_update_lookup_table_cb (BusEngineProxy   *engine,
                                IBusLookupTable  *table,
                                gboolean          visible,
                                BusInputContext  *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (IBUS_IS_LOOKUP_TABLE (table));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    if (!context->enabled)
        return;

    bus_input_context_update_lookup_table (context, table, visible);
}

static void
_engine_register_properties_cb (BusEngineProxy  *engine,
                                IBusPropList    *props,
                                BusInputContext *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (IBUS_IS_PROP_LIST (props));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    if (!context->enabled)
        return;

    bus_input_context_register_properties (context, props);
}

static void
_engine_update_property_cb (BusEngineProxy  *engine,
                            IBusProperty    *prop,
                            BusInputContext *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (IBUS_IS_PROPERTY (prop));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    g_assert (context->engine == engine);

    if (!context->enabled)
        return;

    bus_input_context_update_property (context, prop);
}

#define DEFINE_FUNCTION(name)                                   \
    static void                                                 \
    _engine_##name##_cb (BusEngineProxy   *engine,              \
                         BusInputContext *context)              \
    {                                                           \
        g_assert (BUS_IS_ENGINE_PROXY (engine));                \
        g_assert (BUS_IS_INPUT_CONTEXT (context));              \
                                                                \
        g_assert (context->engine == engine);                   \
                                                                \
        if (!context->enabled)                                  \
            return;                                             \
                                                                \
        bus_input_context_##name (context);                     \
    }

DEFINE_FUNCTION (show_preedit_text)
DEFINE_FUNCTION (hide_preedit_text)
DEFINE_FUNCTION (show_auxiliary_text)
DEFINE_FUNCTION (hide_auxiliary_text)
DEFINE_FUNCTION (show_lookup_table)
DEFINE_FUNCTION (hide_lookup_table)
DEFINE_FUNCTION (page_up_lookup_table)
DEFINE_FUNCTION (page_down_lookup_table)
DEFINE_FUNCTION (cursor_up_lookup_table)
DEFINE_FUNCTION (cursor_down_lookup_table)
#undef DEFINE_FUNCTION

BusInputContext *
bus_input_context_new (BusConnection    *connection,
                       const gchar      *client)
{
    g_assert (BUS_IS_CONNECTION (connection));
    g_assert (client != NULL);

    BusInputContext *context;
    gchar *path;

    path = g_strdup_printf (IBUS_PATH_INPUT_CONTEXT, ++id);

    context = (BusInputContext *) g_object_new (BUS_TYPE_INPUT_CONTEXT,
                                                "path", path,
                                                NULL);
    g_free (path);

#if 0
    ibus_service_add_to_connection (IBUS_SERVICE (context),
                                 IBUS_CONNECTION (connection));
#endif

    g_object_ref_sink (connection);
    context->connection = connection;
    context->client = g_strdup (client);

    g_signal_connect (context->connection,
                      "destroy",
                      (GCallback) _connection_destroy_cb,
                      context);

    return context;
}

void
bus_input_context_enable (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (!context->has_focus) {
        context->enabled = TRUE;
        /* TODO: Do we need to emit "enabled" signal? */
        return;
    }

    if (context->engine == NULL) {
        g_signal_emit (context, context_signals[REQUEST_ENGINE], 0, NULL);
    }

    if (context->engine == NULL)
        return;

    context->enabled = TRUE;

    bus_engine_proxy_enable (context->engine);
    bus_engine_proxy_focus_in (context->engine);
    bus_engine_proxy_set_capabilities (context->engine, context->capabilities);
    bus_engine_proxy_set_cursor_location (context->engine, context->x, context->y, context->w, context->h);

    ibus_service_emit_signal ((IBusService *)context,
                              NULL,
                              "org.freedesktop.IBus.InputContext",
                              "Enabled",
                              NULL,
                              NULL);
    g_signal_emit (context,
                   context_signals[ENABLED],
                   0);
}

void
bus_input_context_disable (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    bus_input_context_clear_preedit_text (context);
    bus_input_context_update_auxiliary_text (context, text_empty, FALSE);
    bus_input_context_update_lookup_table (context, lookup_table_empty, FALSE);
    bus_input_context_register_properties (context, props_empty);

    if (context->engine) {
        bus_engine_proxy_focus_out (context->engine);
        bus_engine_proxy_disable (context->engine);
    }

    ibus_service_emit_signal ((IBusService *)context,
                              NULL,
                              "org.freedesktop.IBus.InputContext",
                              "Disabled",
                              NULL,
                              NULL);
    g_signal_emit (context,
                   context_signals[DISABLED],
                   0);

    context->enabled = FALSE;
}

gboolean
bus_input_context_is_enabled (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    return context->enabled;
}

const static struct {
    const gchar *name;
    GCallback    callback;
} signals [] = {
    { "commit-text",            G_CALLBACK (_engine_commit_text_cb) },
    { "forward-key-event",      G_CALLBACK (_engine_forward_key_event_cb) },
    { "delete-surrounding-text", G_CALLBACK (_engine_delete_surrounding_text_cb) },
    { "update-preedit-text",    G_CALLBACK (_engine_update_preedit_text_cb) },
    { "show-preedit-text",      G_CALLBACK (_engine_show_preedit_text_cb) },
    { "hide-preedit-text",      G_CALLBACK (_engine_hide_preedit_text_cb) },
    { "update-auxiliary-text",  G_CALLBACK (_engine_update_auxiliary_text_cb) },
    { "show-auxiliary-text",    G_CALLBACK (_engine_show_auxiliary_text_cb) },
    { "hide-auxiliary-text",    G_CALLBACK (_engine_hide_auxiliary_text_cb) },
    { "update-lookup-table",    G_CALLBACK (_engine_update_lookup_table_cb) },
    { "show-lookup-table",      G_CALLBACK (_engine_show_lookup_table_cb) },
    { "hide-lookup-table",      G_CALLBACK (_engine_hide_lookup_table_cb) },
    { "page-up-lookup-table",   G_CALLBACK (_engine_page_up_lookup_table_cb) },
    { "page-down-lookup-table", G_CALLBACK (_engine_page_down_lookup_table_cb) },
    { "cursor-up-lookup-table", G_CALLBACK (_engine_cursor_up_lookup_table_cb) },
    { "cursor-down-lookup-table", G_CALLBACK (_engine_cursor_down_lookup_table_cb) },
    { "register-properties",    G_CALLBACK (_engine_register_properties_cb) },
    { "update-property",        G_CALLBACK (_engine_update_property_cb) },
    { "destroy",                G_CALLBACK (_engine_destroy_cb) },
    { NULL, 0 }
};

static void
bus_input_context_unset_engine (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    bus_input_context_clear_preedit_text (context);
    bus_input_context_update_auxiliary_text (context, text_empty, FALSE);
    bus_input_context_update_lookup_table (context, lookup_table_empty, FALSE);
    bus_input_context_register_properties (context, props_empty);

    if (context->engine) {
        gint i;
        for (i = 0; signals[i].name != NULL; i++) {
            g_signal_handlers_disconnect_by_func (context->engine, signals[i].callback, context);
        }
        /* Do not destroy the engine anymore, because of global engine feature */
        g_object_unref (context->engine);
        context->engine = NULL;
    }
}

void
bus_input_context_set_engine (BusInputContext *context,
                              BusEngineProxy  *engine)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    if (context->engine == engine)
        return;

    if (context->engine != NULL) {
        bus_input_context_unset_engine (context);
    }

    if (engine == NULL) {
        bus_input_context_disable (context);
    }
    else {
        gint i;
        context->engine = engine;
        g_object_ref_sink (context->engine);

        for (i = 0; signals[i].name != NULL; i++) {
            g_signal_connect (context->engine,
                              signals[i].name,
                              signals[i].callback,
                              context);
        }
        if (context->has_focus && context->enabled) {
            bus_engine_proxy_focus_in (context->engine);
            bus_engine_proxy_enable (context->engine);
            bus_engine_proxy_set_capabilities (context->engine, context->capabilities);
            bus_engine_proxy_set_cursor_location (context->engine, context->x, context->y, context->w, context->h);
        }
    }
    g_signal_emit (context,
                   context_signals[ENGINE_CHANGED],
                   0);
}

BusEngineProxy *
bus_input_context_get_engine (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    return context->engine;
}

static gboolean
bus_input_context_filter_keyboard_shortcuts (BusInputContext    *context,
                                             guint               keyval,
                                             guint               keycode,
                                             guint               modifiers)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    gboolean retval = FALSE;

    if (context->filter_release){
        if(modifiers & IBUS_RELEASE_MASK) {
            /* filter release key event */
            return TRUE;
        }
        else {
            /* stop filter release key event */
            context->filter_release = FALSE;
        }
    }

    if (keycode != 0 && bus_ibus_impl_is_use_sys_layout (BUS_DEFAULT_IBUS)) {
        IBusKeymap *keymap = BUS_DEFAULT_KEYMAP;
        if (keymap != NULL) {
            guint tmp = ibus_keymap_lookup_keysym (keymap,
                                                 keycode,
                                                 modifiers);
            if (tmp != IBUS_VoidSymbol)
                keyval = tmp;
        }
    }

    retval = bus_ibus_impl_filter_keyboard_shortcuts (BUS_DEFAULT_IBUS,
                                                      context,
                                                      keyval,
                                                      modifiers,
                                                      context->prev_keyval,
                                                      context->prev_modifiers);
    context->prev_keyval = keyval;
    context->prev_modifiers = modifiers;

    if (retval == TRUE) {
        /* begin filter release key event */
        context->filter_release = TRUE;
    }

    return retval;
}

guint
bus_input_context_get_capabilities (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    return context->capabilities;
}
