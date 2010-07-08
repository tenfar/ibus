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
#include "engineproxy.h"
#include <ibusinternal.h>
#include "marshalers.h"
#include "ibusimpl.h"
#include "option.h"

struct _BusEngineProxy {
    IBusProxy parent;
    /* instance members */
    gboolean has_focus;
    gboolean enabled;
    guint capabilities;
    /* cursor location */
    gint x;
    gint y;
    gint w;
    gint h;

    IBusEngineDesc *desc;
    IBusKeymap     *keymap;
    IBusPropList *prop_list;

    /* private member */
};

struct _BusEngineProxyClass {
    IBusProxyClass parent;
    /* class members */
};

enum {
    COMMIT_TEXT,
    FORWARD_KEY_EVENT,
    DELETE_SURROUNDING_TEXT,
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
    LAST_SIGNAL,
};

static guint    engine_signals[LAST_SIGNAL] = { 0 };
// static guint            engine_signals[LAST_SIGNAL] = { 0 };

/* functions prototype */
static void     bus_engine_proxy_real_destroy   (IBusProxy          *proxy);

static void     bus_engine_proxy_g_signal       (GDBusProxy         *proxy,
                                                 const gchar        *sender_name,
                                                 const gchar        *signal_name,
                                                 GVariant           *parameters);

G_DEFINE_TYPE (BusEngineProxy, bus_engine_proxy, IBUS_TYPE_PROXY)

static void
bus_engine_proxy_class_init (BusEngineProxyClass *klass)
{
    IBUS_PROXY_CLASS (klass)->destroy = bus_engine_proxy_real_destroy;
    G_DBUS_PROXY_CLASS (klass)->g_signal = bus_engine_proxy_g_signal;

    /* install signals */
    engine_signals[COMMIT_TEXT] =
        g_signal_new (I_("commit-text"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT,
            G_TYPE_NONE,
            1,
            IBUS_TYPE_TEXT);

    engine_signals[FORWARD_KEY_EVENT] =
        g_signal_new (I_("forward-key-event"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__UINT_UINT_UINT,
            G_TYPE_NONE,
            3,
            G_TYPE_UINT,
            G_TYPE_UINT,
            G_TYPE_UINT);

    engine_signals[DELETE_SURROUNDING_TEXT] =
        g_signal_new (I_("delete-surrounding-text"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__INT_UINT,
            G_TYPE_NONE,
            2,
            G_TYPE_INT,
            G_TYPE_UINT);

    engine_signals[UPDATE_PREEDIT_TEXT] =
        g_signal_new (I_("update-preedit-text"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT_UINT_BOOLEAN_UINT,
            G_TYPE_NONE,
            4,
            IBUS_TYPE_TEXT,
            G_TYPE_UINT,
            G_TYPE_BOOLEAN,
            G_TYPE_UINT);

    engine_signals[SHOW_PREEDIT_TEXT] =
        g_signal_new (I_("show-preedit-text"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[HIDE_PREEDIT_TEXT] =
        g_signal_new (I_("hide-preedit-text"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[UPDATE_AUXILIARY_TEXT] =
        g_signal_new (I_("update-auxiliary-text"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT_BOOLEAN,
            G_TYPE_NONE,
            2,
            IBUS_TYPE_TEXT,
            G_TYPE_BOOLEAN);

    engine_signals[SHOW_AUXILIARY_TEXT] =
        g_signal_new (I_("show-auxiliary-text"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[HIDE_AUXILIARY_TEXT] =
        g_signal_new (I_("hide-auxiliary-text"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[UPDATE_LOOKUP_TABLE] =
        g_signal_new (I_("update-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT_BOOLEAN,
            G_TYPE_NONE,
            2,
            IBUS_TYPE_LOOKUP_TABLE,
            G_TYPE_BOOLEAN);

    engine_signals[SHOW_LOOKUP_TABLE] =
        g_signal_new (I_("show-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[HIDE_LOOKUP_TABLE] =
        g_signal_new (I_("hide-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[PAGE_UP_LOOKUP_TABLE] =
        g_signal_new (I_("page-up-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[PAGE_DOWN_LOOKUP_TABLE] =
        g_signal_new (I_("page-down-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[CURSOR_UP_LOOKUP_TABLE] =
        g_signal_new (I_("cursor-up-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[CURSOR_DOWN_LOOKUP_TABLE] =
        g_signal_new (I_("cursor-down-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    engine_signals[REGISTER_PROPERTIES] =
        g_signal_new (I_("register-properties"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT,
            G_TYPE_NONE,
            1,
            IBUS_TYPE_PROP_LIST);

    engine_signals[UPDATE_PROPERTY] =
        g_signal_new (I_("update-property"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            bus_marshal_VOID__OBJECT,
            G_TYPE_NONE,
            1,
            IBUS_TYPE_PROPERTY);

}

static void
bus_engine_proxy_init (BusEngineProxy *engine)
{
}

static void
bus_engine_proxy_real_destroy (IBusProxy *proxy)
{
    BusEngineProxy *engine = (BusEngineProxy *)proxy;

    g_dbus_proxy_call ((GDBusProxy *)proxy,
                       "org.freedesktop.IBus.Service.Destroy",
                       NULL,
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       NULL,
                       NULL);

    if (engine->desc) {
        g_object_unref (engine->desc);
        engine->desc = NULL;
    }

    if (engine->keymap) {
        g_object_unref (engine->keymap);
        engine->keymap = NULL;
    }

    IBUS_PROXY_CLASS(bus_engine_proxy_parent_class)->destroy ((IBusProxy *)engine);
}

static void
_g_object_unref_if_floating (gpointer instance)
{
    if (g_object_is_floating (instance))
        g_object_unref (instance);
}

static void
bus_engine_proxy_g_signal (GDBusProxy  *proxy,
                           const gchar *sender_name,
                           const gchar *signal_name,
                           GVariant    *parameters)
{
    BusEngineProxy *engine = (BusEngineProxy *)proxy;

    static const struct {
        const gchar *signal_name;
        const guint  signal_id;
    } signals [] = {
        { "ShowPreeditText",        SHOW_PREEDIT_TEXT },
        { "HidePreeditText",        HIDE_PREEDIT_TEXT },
        { "ShowAuxiliaryText",      SHOW_AUXILIARY_TEXT },
        { "HideAuxiliaryText",      HIDE_AUXILIARY_TEXT },
        { "ShowLookupTable",        SHOW_LOOKUP_TABLE },
        { "HideLookupTable",        HIDE_LOOKUP_TABLE },
        { "PageUpLookupTable",      PAGE_UP_LOOKUP_TABLE },
        { "PageDownLookupTable",    PAGE_DOWN_LOOKUP_TABLE },
        { "CursorUpLookupTable",    CURSOR_UP_LOOKUP_TABLE },
        { "CursorDownLookupTable",  CURSOR_DOWN_LOOKUP_TABLE },
    };

    gint i;
    for (i = 0; i < G_N_ELEMENTS (signals); i++) {
        if (g_strcmp0 (signal_name, signals[i].signal_name) == 0) {
            g_signal_emit (engine, engine_signals[signals[i].signal_id], 0);
            return;
        }
    }

    if (g_strcmp0 (signal_name, "CommitText") == 0) {
        GVariant *arg0 = g_variant_get_child_value (parameters, 0);
        g_return_if_fail (arg0 != NULL);

        IBusText *text = (IBusText *)ibus_serializable_deserialize (arg0);
        g_variant_unref (arg0);
        g_return_if_fail (text != NULL);
        g_signal_emit (engine, engine_signals[COMMIT_TEXT], 0, text);
        _g_object_unref_if_floating (text);
    }
    else if (g_strcmp0 (signal_name, "ForwardKeyEvent") == 0) {
        guint32 keyval = 0;
        guint32 keycode = 0;
        guint32 states = 0;
        g_variant_get (parameters, "(uuu)", &keyval, &keycode, &states);

        g_signal_emit (engine,
                       engine_signals[FORWARD_KEY_EVENT],
                       0,
                       keyval,
                       keycode,
                       states);
    }
    else if (g_strcmp0 (signal_name, "DeleteSurroundingText") == 0) {
        gint  offset_from_cursor = 0;
        guint nchars = 0;
        g_variant_get (parameters, "(uuu)", &offset_from_cursor, &nchars);

        g_signal_emit (engine,
                       engine_signals[DELETE_SURROUNDING_TEXT],
                       0, offset_from_cursor, nchars);
    }
    else if (g_strcmp0 (signal_name, "UpdatePreeditText") == 0) {
        GVariant *arg0 = NULL;
        gint cursor_pos = 0;
        gboolean visible = FALSE;
        guint mode = 0;

        g_variant_get (parameters, "(vibu)", &arg0, &cursor_pos, &visible, &mode);
        g_return_if_fail (arg0 != NULL);

        IBusText *text = (IBusText *)ibus_serializable_deserialize (arg0);
        g_variant_unref (arg0);
        g_return_if_fail (text != NULL);

        g_signal_emit (engine,
                       engine_signals[UPDATE_PREEDIT_TEXT],
                       0, text, cursor_pos, visible, mode);

        _g_object_unref_if_floating (text);
    }
    else if (g_strcmp0 (signal_name, "UpdateAuxiliaryText") == 0) {
        GVariant *arg0 = NULL;
        gboolean visible = FALSE;

        g_variant_get (parameters, "(vb)", &arg0, &visible);
        g_return_if_fail (arg0 != NULL);

        IBusText *text = (IBusText *)ibus_serializable_deserialize (arg0);
        g_variant_unref (arg0);
        g_return_if_fail (text != NULL);

        g_signal_emit (engine, engine_signals[UPDATE_AUXILIARY_TEXT], 0, text, visible);
        _g_object_unref_if_floating (text);
    }
    else if (g_strcmp0 (signal_name, "UpdateLookupTable") == 0) {
        GVariant *arg0 = NULL;
        gboolean visible = FALSE;

        g_variant_get (parameters, "(vb)", &arg0, &visible);
        g_return_if_fail (arg0 != NULL);

        IBusLookupTable *table = (IBusLookupTable *)ibus_serializable_deserialize (arg0);
        g_variant_unref (arg0);
        g_return_if_fail (table != NULL);

        g_signal_emit (engine, engine_signals[UPDATE_LOOKUP_TABLE], 0, table, visible);
        _g_object_unref_if_floating (table);
    }
    else if (g_strcmp0 (signal_name, "RegisterProperties") == 0) {
        GVariant *arg0 = g_variant_get_child_value (parameters, 0);
        g_return_if_fail (arg0 != NULL);

        IBusPropList *prop_list = (IBusPropList *)ibus_serializable_deserialize (arg0);
        g_variant_unref (arg0);
        g_return_if_fail (prop_list != NULL);

        g_signal_emit (engine, engine_signals[REGISTER_PROPERTIES], 0, prop_list);
        _g_object_unref_if_floating (prop_list);
    }
    else if (g_strcmp0 (signal_name, "UpdateProperty") == 0) {
        GVariant *arg0 = g_variant_get_child_value (parameters, 0);
        g_return_if_fail (arg0 != NULL);

        IBusProperty *prop = (IBusProperty *)ibus_serializable_deserialize (arg0);
        g_variant_unref (arg0);
        g_return_if_fail (prop != NULL);

        g_signal_emit (engine, engine_signals[UPDATE_PROPERTY], 0, prop);
        _g_object_unref_if_floating (prop);
    }
    else
        return G_DBUS_PROXY_CLASS (bus_engine_proxy_parent_class)->g_signal (
                        proxy, sender_name, signal_name, parameters);
}

BusEngineProxy *
bus_engine_proxy_new (const gchar    *path,
                      IBusEngineDesc *desc,
                      BusConnection  *connection)
{
    g_assert (path);
    g_assert (IBUS_IS_ENGINE_DESC (desc));
    g_assert (BUS_IS_CONNECTION (connection));

    BusEngineProxy *engine =
        (BusEngineProxy *) g_initable_new (BUS_TYPE_ENGINE_PROXY,
                                           NULL,
                                           NULL,
                                           "g-connection",     bus_connection_get_dbus_connection (connection),
                                           "g-interface-name", IBUS_INTERFACE_ENGINE,
                                           "g-object-path",    path,
                                           NULL);
    if (engine == NULL)
        return NULL;

    engine->desc = desc;
    g_object_ref_sink (desc);
    if (desc->layout != NULL && desc->layout[0] != '\0') {
        engine->keymap = ibus_keymap_get (desc->layout);
    }

    if (engine->keymap == NULL) {
        engine->keymap = ibus_keymap_get ("us");
    }
    return engine;
}

void
bus_engine_proxy_process_key_event (BusEngineProxy      *engine,
                                    guint                keyval,
                                    guint                keycode,
                                    guint                state,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));

    /* FIXME */
#if 0
    if (keycode != 0 && !BUS_DEFAULT_IBUS->use_sys_layout && engine->keymap != NULL) {
        guint t = ibus_keymap_lookup_keysym (engine->keymap, keycode, state);
        if (t != IBUS_VoidSymbol) {
            keyval = t;
        }
    }
#endif

    g_dbus_proxy_call ((GDBusProxy *)engine,
                       "ProcessKeyEvent",
                       g_variant_new ("(uuu)", keyval, keycode, state),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       callback,
                       user_data);
}

void
bus_engine_proxy_set_cursor_location (BusEngineProxy *engine,
                                      gint            x,
                                      gint            y,
                                      gint            w,
                                      gint            h)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));

    if (engine->x != x || engine->y != y || engine->w != w || engine->h != h) {
        engine->x = x;
        engine->y = y;
        engine->w = w;
        engine->h = h;
        g_dbus_proxy_call ((GDBusProxy *)engine,
                           "SetCursorLocation",
                           g_variant_new ("(iiii)", x, y, w, h),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
    }
}

void
bus_engine_proxy_set_capabilities (BusEngineProxy *engine,
                                   guint           caps)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));

    if (engine->capabilities != caps) {
        engine->capabilities = caps;
        g_dbus_proxy_call ((GDBusProxy *)engine,
                           "SetCapabilities",
                           g_variant_new ("(u)", caps),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
    }
}

void
bus_engine_proxy_property_activate (BusEngineProxy *engine,
                                    const gchar    *prop_name,
                                    guint           prop_state)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (prop_name != NULL);

    g_dbus_proxy_call ((GDBusProxy *)engine,
                       "PropertyActivate",
                       g_variant_new ("(su)", prop_name, prop_state),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       NULL,
                       NULL);
}

void
bus_engine_proxy_property_show (BusEngineProxy *engine,
                                const gchar    *prop_name)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (prop_name != NULL);

    g_dbus_proxy_call ((GDBusProxy *)engine,
                       "PropertyShow",
                       g_variant_new ("(s)", prop_name),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       NULL,
                       NULL);
}

void bus_engine_proxy_property_hide (BusEngineProxy *engine,
                                     const gchar    *prop_name)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (prop_name != NULL);

    g_dbus_proxy_call ((GDBusProxy *)engine,
                       "PropertyHide",
                       g_variant_new ("(s)", prop_name),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       NULL,
                       NULL);
}

#define DEFINE_FUNCTION(Name, name)                         \
    void                                                    \
    bus_engine_proxy_##name (BusEngineProxy *engine)        \
    {                                                       \
        g_assert (BUS_IS_ENGINE_PROXY (engine));            \
        g_dbus_proxy_call ((GDBusProxy *)engine,            \
                           #Name,                           \
                           NULL,                            \
                           G_DBUS_CALL_FLAGS_NONE,          \
                           -1, NULL, NULL, NULL);           \
    }

DEFINE_FUNCTION (Reset, reset)
DEFINE_FUNCTION (PageUp, page_up)
DEFINE_FUNCTION (PageDown, page_down)
DEFINE_FUNCTION (CursorUp, cursor_up)
DEFINE_FUNCTION (CursorDown, cursor_down)

#undef DEFINE_FUNCTION

void
bus_engine_proxy_focus_in (BusEngineProxy *engine)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    if (!engine->has_focus) {
        engine->has_focus = TRUE;
        g_dbus_proxy_call ((GDBusProxy *)engine,
                           "FocusIn",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
    }
}

void
bus_engine_proxy_focus_out (BusEngineProxy *engine)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    if (engine->has_focus) {
        engine->has_focus = FALSE;
        g_dbus_proxy_call ((GDBusProxy *)engine,
                           "FocusOut",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
    }
}

void
bus_engine_proxy_enable (BusEngineProxy *engine)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    if (!engine->enabled) {
        engine->enabled = TRUE;
        g_dbus_proxy_call ((GDBusProxy *)engine,
                           "Enable",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
    }
}

void
bus_engine_proxy_disable (BusEngineProxy *engine)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    if (engine->enabled) {
        engine->enabled = FALSE;
        g_dbus_proxy_call ((GDBusProxy *)engine,
                           "Disable",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
    }
}

void
bus_engine_proxy_candidate_clicked (BusEngineProxy *engine,
                                    guint           index,
                                    guint           button,
                                    guint           state)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));

    g_dbus_proxy_call ((GDBusProxy *)engine,
                       "CandidateClicked",
                       g_variant_new ("(uuu)", index, button, state),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       NULL,
                       NULL);
}

IBusEngineDesc *
bus_engine_proxy_get_desc (BusEngineProxy *engine)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));

    return engine->desc;
}

gboolean
bus_engine_proxy_is_enabled (BusEngineProxy *engine)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));

    return engine->enabled;
}
