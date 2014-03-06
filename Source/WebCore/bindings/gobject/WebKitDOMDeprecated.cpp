/*
 *  Copyright (C) 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitDOMDeprecated.h"

#include "WebKitDOMBlob.h"
#include "WebKitDOMDOMStringList.h"
#include "WebKitDOMHTMLCollection.h"
#include "WebKitDOMHTMLFormElement.h"
#include "WebKitDOMHTMLHeadElement.h"
#include "WebKitDOMNodeList.h"
#include "WebKitDOMObject.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMProcessingInstruction.h"
#include "WebKitDOMWebKitNamedFlow.h"

using namespace WebKit;

WebKitDOMBlob* webkit_dom_blob_webkit_slice(WebKitDOMBlob* self, gint64 start, gint64 end, const gchar* content_type)
{
    return webkit_dom_blob_slice(self, start, end, content_type);
}

gchar* webkit_dom_html_element_get_id(WebKitDOMHTMLElement* element)
{
    g_warning("The get_id method on WebKitDOMHTMLElement is deprecated. Use the one in WebKitDOMElement instead.");
    return webkit_dom_element_get_id(WEBKIT_DOM_ELEMENT(element));
}

void webkit_dom_html_element_set_id(WebKitDOMHTMLElement* element, const gchar* value)
{
    g_warning("The set_id method on WebKitDOMHTMLElement is deprecated. Use the one in WebKitDOMElement instead.");
    webkit_dom_element_set_id(WEBKIT_DOM_ELEMENT(element), value);
}

gchar* webkit_dom_html_element_get_class_name(WebKitDOMHTMLElement* element)
{
    return webkit_dom_element_get_class_name(WEBKIT_DOM_ELEMENT(element));
}

void webkit_dom_html_element_set_class_name(WebKitDOMHTMLElement* element, const gchar* value)
{
    webkit_dom_element_set_class_name(WEBKIT_DOM_ELEMENT(element), value);
}

WebKitDOMDOMTokenList* webkit_dom_html_element_get_class_list(WebKitDOMHTMLElement* element)
{
    return webkit_dom_element_get_class_list(WEBKIT_DOM_ELEMENT(element));
}

void webkit_dom_html_form_element_dispatch_form_change(WebKitDOMHTMLFormElement* self)
{
    g_warning("The onformchange functionality has been removed from the DOM spec, this function does nothing.");
}

void webkit_dom_html_form_element_dispatch_form_input(WebKitDOMHTMLFormElement* self)
{
    g_warning("The onforminput functionality has been removed from the DOM spec, this function does nothing.");
}

gboolean webkit_dom_webkit_named_flow_get_overflow(WebKitDOMWebKitNamedFlow* flow)
{
    g_warning("The WebKitDOMWebKitNamedFlow::overflow property has been renamed to WebKitDOMWebKitNamedFlow::overset. Please update your code to use the new name.");
    return webkit_dom_webkit_named_flow_get_overset(flow);
}

gchar* webkit_dom_element_get_webkit_region_overflow(WebKitDOMElement* element)
{
    return webkit_dom_element_get_webkit_region_overset(element);
}

WebKitDOMNodeList* webkit_dom_webkit_named_flow_get_content_nodes(WebKitDOMWebKitNamedFlow* namedFlow)
{
    return webkit_dom_webkit_named_flow_get_content(namedFlow);

}

WebKitDOMNodeList* webkit_dom_webkit_named_flow_get_regions_by_content_node(WebKitDOMWebKitNamedFlow* namedFlow, WebKitDOMNode* contentNode)
{
    return webkit_dom_webkit_named_flow_get_regions_by_content(namedFlow, contentNode);
}

// WebKitDOMBarInfo

typedef struct _WebKitDOMBarInfo {
    WebKitDOMObject parent_instance;
} WebKitDOMBarInfo;

typedef struct _WebKitDOMBarInfoClass {
    WebKitDOMObjectClass parent_class;
} WebKitDOMBarInfoClass;

G_DEFINE_TYPE(WebKitDOMBarInfo, webkit_dom_bar_info, WEBKIT_TYPE_DOM_OBJECT)

typedef enum {
    PROP_0,
    PROP_VISIBLE,
} WebKitDOMBarInfoProperties;

static void webkit_dom_bar_info_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    switch (propertyId) {
    case PROP_VISIBLE: {
        WEBKIT_WARN_FEATURE_NOT_PRESENT("BarInfo")
        g_value_set_boolean(value, FALSE);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_bar_info_class_init(WebKitDOMBarInfoClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_bar_info_get_property;

    g_object_class_install_property(gobjectClass,
        PROP_VISIBLE,
        g_param_spec_boolean("visible",
            "bar_info_visible - removed from WebKit, does nothing",
            "read-only  gboolean BarInfo.visible - removed from WebKit, does nothing",
            FALSE,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_bar_info_init(WebKitDOMBarInfo*)
{
}

gboolean webkit_dom_bar_info_get_visible(void*)
{
    g_warning("The BarInfo type has been removed from the DOM spec, this function does nothing.");
    return FALSE;
}

// WebKitDOMCSSStyleDeclaration

WebKitDOMCSSValue* webkit_dom_css_style_declaration_get_property_css_value(WebKitDOMCSSStyleDeclaration*, const gchar*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

// WebKitDOMDocument

gboolean webkit_dom_document_get_webkit_hidden(WebKitDOMDocument*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return FALSE;
}

gchar* webkit_dom_document_get_webkit_visibility_state(WebKitDOMDocument*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return g_strdup("");
}

// WebKitDOMHTMLDocument

void webkit_dom_html_document_open(WebKitDOMHTMLDocument*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
}

// WebKitDOMHTMLElement

void webkit_dom_html_element_set_item_id(WebKitDOMHTMLElement*, const gchar*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
}

gchar* webkit_dom_html_element_get_item_id(WebKitDOMHTMLElement*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return g_strdup("");
}

WebKitDOMDOMSettableTokenList* webkit_dom_html_element_get_item_ref(WebKitDOMHTMLElement*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

WebKitDOMDOMSettableTokenList* webkit_dom_html_element_get_item_prop(WebKitDOMHTMLElement*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

void webkit_dom_html_element_set_item_scope(WebKitDOMHTMLElement*, gboolean)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
}

gboolean webkit_dom_html_element_get_item_scope(WebKitDOMHTMLElement*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return FALSE;
}

void* webkit_dom_html_element_get_item_type(WebKitDOMHTMLElement*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

// WebKitDOMHTMLStyleElement

void webkit_dom_html_style_element_set_scoped(WebKitDOMHTMLStyleElement*, gboolean)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
}

gboolean webkit_dom_html_style_element_get_scoped(WebKitDOMHTMLStyleElement*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return FALSE;
}

// WebKitDOMHTMLPropertiesCollection

typedef struct _WebKitDOMHTMLPropertiesCollection {
    WebKitDOMHTMLCollection parent_instance;
} WebKitDOMHTMLPropertiesCollection;

typedef struct _WebKitDOMHTMLPropertiesCollectionClass {
    WebKitDOMHTMLCollectionClass parent_class;
} WebKitDOMHTMLPropertiesCollectionClass;

G_DEFINE_TYPE(WebKitDOMHTMLPropertiesCollection, webkit_dom_html_properties_collection, WEBKIT_TYPE_DOM_HTML_COLLECTION)

enum {
    HTML_PROPERTIES_COLLECTION_PROP_0,
    HTML_PROPERTIES_COLLECTION_PROP_LENGTH,
    HTML_PROPERTIES_COLLECTION_PROP_NAMES,
};

static void webkit_dom_html_properties_collection_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    switch (propertyId) {
    case HTML_PROPERTIES_COLLECTION_PROP_LENGTH: {
        WEBKIT_WARN_FEATURE_NOT_PRESENT("Microdata")
        break;
    }
    case HTML_PROPERTIES_COLLECTION_PROP_NAMES: {
        WEBKIT_WARN_FEATURE_NOT_PRESENT("Microdata")
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_properties_collection_class_init(WebKitDOMHTMLPropertiesCollectionClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_html_properties_collection_get_property;

    g_object_class_install_property(gobjectClass,
        HTML_PROPERTIES_COLLECTION_PROP_LENGTH,
        g_param_spec_ulong("length",
            "html_properties_collection_length - removed from WebKit, does nothing",
            "read-only  gulong HTMLPropertiesCollection.length - removed from WebKit, does nothing",
            0,
            G_MAXULONG,
            0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(gobjectClass,
        HTML_PROPERTIES_COLLECTION_PROP_NAMES,
        g_param_spec_object("names",
            "html_properties_collection_names - removed from WebKit, does nothing",
            "read-only  WebKitDOMDOMStringList* HTMLPropertiesCollection.names - removed from WebKit, does nothing",
            WEBKIT_TYPE_DOM_DOM_STRING_LIST,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_html_properties_collection_init(WebKitDOMHTMLPropertiesCollection* request)
{
}

WebKitDOMNode* webkit_dom_html_properties_collection_item(void*, gulong)
{
    g_warning("%s: the PropertiesCollection object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

void* webkit_dom_html_properties_collection_named_item(void*, const gchar*)
{
    g_warning("%s: the PropertiesCollection object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

gulong webkit_dom_html_properties_collection_get_length(void*)
{
    g_warning("%s: the PropertiesCollection object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

WebKitDOMDOMStringList* webkit_dom_html_properties_collection_get_names(void*)
{
    g_warning("%s: the PropertiesCollection object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

// WebKitDOMNode

WebKitDOMNamedNodeMap* webkit_dom_node_get_attributes(WebKitDOMNode*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

gboolean webkit_dom_node_has_attributes(WebKitDOMNode*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return FALSE;
}

// WebKitDOMMemoryInfo

typedef struct _WebKitDOMMemoryInfo {
    WebKitDOMObject parent_instance;
} WebKitDOMMemoryInfo;

typedef struct _WebKitDOMMemoryInfoClass {
    WebKitDOMObjectClass parent_class;
} WebKitDOMMemoryInfoClass;


G_DEFINE_TYPE(WebKitDOMMemoryInfo, webkit_dom_memory_info, WEBKIT_TYPE_DOM_OBJECT)

enum {
    DOM_MEMORY_PROP_0,
    DOM_MEMORY_PROP_TOTAL_JS_HEAP_SIZE,
    DOM_MEMORY_PROP_USED_JS_HEAP_SIZE,
    DOM_MEMORY_PROP_JS_HEAP_SIZE_LIMIT,
};

static void webkit_dom_memory_info_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    switch (propertyId) {
    case DOM_MEMORY_PROP_TOTAL_JS_HEAP_SIZE: {
        g_value_set_ulong(value, 0);
        WEBKIT_WARN_FEATURE_NOT_PRESENT("MemoryInfo")
        break;
    }
    case DOM_MEMORY_PROP_USED_JS_HEAP_SIZE: {
        g_value_set_ulong(value, 0);
        WEBKIT_WARN_FEATURE_NOT_PRESENT("MemoryInfo")
        break;
    }
    case DOM_MEMORY_PROP_JS_HEAP_SIZE_LIMIT: {
        g_value_set_ulong(value, 0);
        WEBKIT_WARN_FEATURE_NOT_PRESENT("MemoryInfo")
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_memory_info_class_init(WebKitDOMMemoryInfoClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_memory_info_get_property;

    g_object_class_install_property(gobjectClass,
        DOM_MEMORY_PROP_TOTAL_JS_HEAP_SIZE,
        g_param_spec_ulong("total-js-heap-size",
            "memory_info_total-js-heap-size - removed from WebKit, does nothing",
            "read-only  gulong MemoryInfo.total-js-heap-size - removed from WebKit, does nothing",
            0,
            G_MAXULONG,
            0,
            WEBKIT_PARAM_READABLE));
    g_object_class_install_property(gobjectClass,
        DOM_MEMORY_PROP_USED_JS_HEAP_SIZE,
        g_param_spec_ulong("used-js-heap-size",
            "memory_info_used-js-heap-size - removed from WebKit, does nothing",
            "read-only  gulong MemoryInfo.used-js-heap-size - removed from WebKit, does nothing",
            0,
            G_MAXULONG,
            0,
            WEBKIT_PARAM_READABLE));
    g_object_class_install_property(gobjectClass,
        DOM_MEMORY_PROP_JS_HEAP_SIZE_LIMIT,
        g_param_spec_ulong("js-heap-size-limit",
            "memory_info_js-heap-size-limit - removed from WebKit, does nothing",
            "read-only  gulong MemoryInfo.js-heap-size-limit - removed from WebKit, does nothing",
            0,
            G_MAXULONG,
            0,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_memory_info_init(WebKitDOMMemoryInfo*)
{
}

gulong webkit_dom_memory_info_get_total_js_heap_size(void*)
{
    g_warning("%s: the MemoryInfo object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

gulong webkit_dom_memory_info_get_used_js_heap_size(void*)
{
    g_warning("%s: the MemoryInfo object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

gulong webkit_dom_memory_info_get_js_heap_size_limit(void*)
{
    g_warning("%s: the MemoryInfo object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

// WebKitDOMMicroDataItemValue

typedef struct _WebKitDOMMicroDataItemValue {
    WebKitDOMObject parent_instance;
} WebKitDOMMicroDataItemValue;

typedef struct _WebKitDOMMicroDataItemValueClass {
    WebKitDOMObjectClass parent_class;
} WebKitDOMMicroDataItemValueClass;

G_DEFINE_TYPE(WebKitDOMMicroDataItemValue, webkit_dom_micro_data_item_value, WEBKIT_TYPE_DOM_OBJECT)

static void webkit_dom_micro_data_item_value_class_init(WebKitDOMMicroDataItemValueClass*)
{
}

static void webkit_dom_micro_data_item_value_init(WebKitDOMMicroDataItemValue*)
{
}

// WebKitDOMPerformance

void* webkit_dom_performance_get_memory(WebKitDOMPerformance*)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

// WebKitDOMPropertyNodeList

typedef struct _WebKitDOMPropertyNodeList {
    WebKitDOMNodeList parent_instance;
} WebKitDOMPropertyNodeList;

typedef struct _WebKitDOMPropertyNodeListClass {
    WebKitDOMNodeListClass parent_class;
} WebKitDOMPropertyNodeListClass;

G_DEFINE_TYPE(WebKitDOMPropertyNodeList, webkit_dom_property_node_list, WEBKIT_TYPE_DOM_NODE_LIST)

enum {
    PROPERTY_NODE_LIST_PROP_0,
    PROPERTY_NODE_LIST_PROP_LENGTH,
};

static void webkit_dom_property_node_list_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    switch (propertyId) {
    case PROPERTY_NODE_LIST_PROP_LENGTH: {
        g_value_set_ulong(value, 0);
        WEBKIT_WARN_FEATURE_NOT_PRESENT("Microdata")
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_property_node_list_class_init(WebKitDOMPropertyNodeListClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_property_node_list_get_property;

    g_object_class_install_property(gobjectClass,
        PROPERTY_NODE_LIST_PROP_LENGTH,
        g_param_spec_ulong("length",
            "property_node_list_length - removed from WebKit, does nothing",
            "read-only  gulong PropertyNodeList.length - removed from WebKit, does nothing",
            0,
            G_MAXULONG,
            0,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_property_node_list_init(WebKitDOMPropertyNodeList*)
{
}

WebKitDOMNode* webkit_dom_property_node_list_item(void*, gulong)
{
    g_warning("%s: the PropertyNodeList object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

gulong webkit_dom_property_node_list_get_length(void*)
{
    g_warning("%s: the PropertyNodeList object has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

gdouble webkit_dom_html_media_element_get_start_time(WebKitDOMHTMLMediaElement*)
{
    g_warning("%s: the HTMLMediaElement:start-time property has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

gdouble webkit_dom_html_media_element_get_initial_time(WebKitDOMHTMLMediaElement*)
{
    g_warning("%s: the HTMLMediaElement:initial-time property has been removed from WebKit, this function does nothing.", __func__);
    return 0;
}

// WebKitDOMProcessingInstruction

gchar* webkit_dom_processing_instruction_get_data(WebKitDOMProcessingInstruction* self)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
    return g_strdup("");
}

void webkit_dom_processing_instruction_set_data(WebKitDOMProcessingInstruction* self, const gchar* value, GError** error)
{
    g_warning("%s: this functionality has been removed from WebKit, this function does nothing.", __func__);
}

