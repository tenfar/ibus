#include "ibus.h"

void test_serializable (IBusSerializable *object)
{
    GVariant *variant;
    gchar *s1, *s2;

    variant = ibus_serializable_serialize (object);
    s1 = g_variant_print (variant, TRUE);

    g_object_unref (object);

    object = (IBusSerializable *) ibus_serializable_deserialize (variant);
    g_variant_unref (variant);

    variant = ibus_serializable_serialize (object);
    s2 = g_variant_print (variant, TRUE);
    g_object_unref (object);
    g_variant_unref (variant);
    g_assert_cmpstr (s1, ==, s2);
    g_free (s1);
    g_free (s2);
}

static void
test_text (void)
{
    test_serializable ((IBusSerializable *)ibus_text_new_from_string ("Hello"));
}

static void
test_engine_desc (void)
{
    test_serializable ((IBusSerializable *)ibus_engine_desc_new ("Hello",
                                        "Hello Engine",
                                        "Hello Engine Desc",
                                        "zh",
                                        "GPLv2",
                                        "Peng Huang <shawn.p.huang@gmail.com>",
                                        "icon",
                                        "en"));
}

static void
test_property (void)
{
    test_serializable ((IBusSerializable *)ibus_property_new ("key",
                                                          PROP_TYPE_NORMAL,
                                                          ibus_text_new_from_string ("label"),
                                                          "icon",
                                                          ibus_text_new_from_static_string ("tooltip"),
                                                          TRUE,
                                                          TRUE,
                                                          PROP_STATE_UNCHECKED,
                                                          ibus_prop_list_new ()));
}

static void
test_lookup_table (void)
{
    IBusLookupTable *table;
    table = ibus_lookup_table_new (9, 0, TRUE, FALSE);
    ibus_lookup_table_append_candidate (table, ibus_text_new_from_static_string ("Hello"));
    ibus_lookup_table_append_candidate (table, ibus_text_new_from_static_string ("Cool"));
    test_serializable ((IBusSerializable *)table);
}

static void
test_attribute (void)
{
    IBusAttrList *list = ibus_attr_list_new ();
    ibus_attr_list_append (list, ibus_attribute_new (1, 1, 1, 2));
    ibus_attr_list_append (list, ibus_attribute_new (2, 1, 1, 2));
    ibus_attr_list_append (list, ibus_attribute_new (3, 1, 1, 2));
    ibus_attr_list_append (list, ibus_attribute_new (3, 1, 1, 2));
    test_serializable ((IBusSerializable *)list);
}

gint
main (gint    argc,
      gchar **argv)
{
	g_type_init ();
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/ibus/text", test_text);
    g_test_add_func ("/ibus/enginedesc", test_engine_desc);
    g_test_add_func ("/ibus/lookuptable", test_lookup_table);
    g_test_add_func ("/ibus/attribute", test_attribute);
    g_test_add_func ("/ibus/property", test_property);

    return g_test_run ();
}
