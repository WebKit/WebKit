/*
    Copyright (C) 2011 ProFUSION embedded systems

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ewk_js.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "NP_jsobject.h"
#include "ewk_private.h"
#include "npruntime.h"
#include "npruntime_impl.h"
#include <string.h>

#define EINA_MAGIC_CHECK_OR_RETURN(o, ...) \
    if (!EINA_MAGIC_CHECK(obj, EWK_JS_OBJECT_MAGIC)) { \
        EINA_MAGIC_FAIL(obj, EWK_JS_OBJECT_MAGIC); \
        return __VA_ARGS__; \
    }

struct _Ewk_JS_Class {
    const Ewk_JS_Class_Meta* meta;
    Eina_Hash* methods; // Key=NPIdentifier(name), value=pointer to meta->methods.
    Eina_Hash* properties; // Key=NPIdentifier(name), value=pointer to meta->properties.
    Ewk_JS_Default default_prop;
};

static Eina_Bool ewk_js_npvariant_to_variant(Ewk_JS_Variant* data, const NPVariant* result);

static Eina_Bool ewk_js_variant_to_npvariant(const Ewk_JS_Variant* data, NPVariant* result)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(data, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(result, EINA_FALSE);
    char* string_value;

    switch (data->type) {
    case EWK_JS_VARIANT_VOID:
        VOID_TO_NPVARIANT(*result);
        break;
    case EWK_JS_VARIANT_NULL:
        NULL_TO_NPVARIANT(*result);
        break;
    case EWK_JS_VARIANT_INT32:
        INT32_TO_NPVARIANT(data->value.i, *result);
        break;
    case EWK_JS_VARIANT_DOUBLE:
        DOUBLE_TO_NPVARIANT(data->value.d, *result);
        break;
    case EWK_JS_VARIANT_STRING:
        string_value = strdup(data->value.s);
        if (string_value)
            STRINGZ_TO_NPVARIANT(string_value, *result);
        else
            return EINA_FALSE;
        break;
    case EWK_JS_VARIANT_BOOL:
        BOOLEAN_TO_NPVARIANT(data->value.b, *result);
        break;
    case EWK_JS_VARIANT_OBJECT:
        OBJECT_TO_NPVARIANT(reinterpret_cast<NPObject*>(data->value.o), *result);
        break;
    default:
        return EINA_FALSE;
    }

    return EINA_TRUE;
}

// These methods are used by NPAI, thats the reason to use bool instead of Eina_Bool.
static bool ewk_js_property_has(NPObject* np_obj, NPIdentifier name)
{
    Ewk_JS_Object* obj = reinterpret_cast<Ewk_JS_Object*>(np_obj);

    EINA_SAFETY_ON_NULL_RETURN_VAL(np_obj, false);
    EINA_MAGIC_CHECK_OR_RETURN(obj, false);

    if (!_NPN_IdentifierIsString(name)) {
        ERR("int NPIdentifier is not supported.");
        return false;
    }

    char* prop_name = _NPN_UTF8FromIdentifier(name);
    bool fail = eina_hash_find(obj->properties, prop_name); // FIXME: should search methods too?
    free(prop_name);

    return fail;
}

static bool ewk_js_property_get(NPObject* np_obj, NPIdentifier name, NPVariant* result)
{
    Ewk_JS_Object* obj = reinterpret_cast<Ewk_JS_Object*>(np_obj);
    Ewk_JS_Variant* value;
    Ewk_JS_Property* prop;
    bool fail = false;

    EINA_SAFETY_ON_NULL_RETURN_VAL(np_obj, false);
    EINA_MAGIC_CHECK_OR_RETURN(obj, false);

    if (!_NPN_IdentifierIsString(name)) {
        ERR("int NPIdentifier is not supported.");
        return false;
    }

    value = static_cast<Ewk_JS_Variant*>(malloc(sizeof(Ewk_JS_Variant)));
    if (!value) {
        ERR("Could not allocate memory for ewk_js_variant");
        return false;
    }

    prop = static_cast<Ewk_JS_Property*>(eina_hash_find(obj->cls->properties, name));
    if (prop && prop->get) { // Class has property and property has getter.
        fail = prop->get(obj, prop->name, value);
        if (!fail)
            fail = ewk_js_variant_to_npvariant(value, result);
    } else if (obj->cls->default_prop.get) { // Default getter exists.
        fail = obj->cls->default_prop.get(obj, prop->name, value);
        if (!fail)
            fail = ewk_js_variant_to_npvariant(value, result);
    } else { // Fallback to objects hash map.
        char* prop_name = _NPN_UTF8FromIdentifier(name);
        free(value);
        value = static_cast<Ewk_JS_Variant*>(eina_hash_find(obj->properties, prop_name));
        free(prop_name);
        if (value)
            return ewk_js_variant_to_npvariant(value, result);
    }

    free(value);
    return fail;
}

static bool ewk_js_property_set(NPObject* np_obj, NPIdentifier name, const NPVariant* np_value)
{
    Ewk_JS_Object* obj = reinterpret_cast<Ewk_JS_Object*>(np_obj);
    Ewk_JS_Variant* value;
    Ewk_JS_Property* prop;
    bool fail = false;

    EINA_SAFETY_ON_NULL_RETURN_VAL(np_obj, false);
    EINA_MAGIC_CHECK_OR_RETURN(obj, false);

    if (!_NPN_IdentifierIsString(name)) {
        ERR("int NPIdentifier is not supported.");
        return fail;
    }

    value = static_cast<Ewk_JS_Variant*>(malloc(sizeof(Ewk_JS_Variant)));
    if (!value) {
        ERR("Could not allocate memory for ewk_js_variant");
        return false;
    }

    ewk_js_npvariant_to_variant(value, np_value);
    char* prop_name = _NPN_UTF8FromIdentifier(name);
    prop = static_cast<Ewk_JS_Property*>(eina_hash_find(obj->cls->properties, prop_name));
    if (prop && prop->set)
        fail = prop->set(obj, prop->name, value); // Class has property and property has setter.
    else if (obj->cls->default_prop.set)
        fail = obj->cls->default_prop.set(obj, prop_name, value); // Default getter exists.
    else { // Fallback to objects hash map.
        void* old = eina_hash_set(obj->properties, prop_name, value);
        free(old);
        fail = true;
    }

    free(prop_name);
    return fail;
}

static bool ewk_js_property_remove(NPObject* np_obj, NPIdentifier name)
{
    Ewk_JS_Object* obj = reinterpret_cast<Ewk_JS_Object*>(np_obj);
    Ewk_JS_Property* prop;
    bool fail = false;

    EINA_SAFETY_ON_NULL_RETURN_VAL(np_obj, false);
    EINA_MAGIC_CHECK_OR_RETURN(obj, false);

    if (!_NPN_IdentifierIsString(name)) {
        ERR("int NPIdentifier is not supported.");
        return fail;
    }

    char* prop_name = _NPN_UTF8FromIdentifier(name);
    prop = static_cast<Ewk_JS_Property*>(eina_hash_find(obj->cls->properties, prop_name));
    if (prop && prop->del)
        fail = prop->del(obj, prop->name); // Class has property and property has getter.
    else if (obj->cls->default_prop.del)
        fail = obj->cls->default_prop.del(obj, prop_name);
    else
        fail = eina_hash_del(obj->properties, prop_name, 0);

    free(prop_name);
    return fail;
}

static bool ewk_js_properties_enumerate(NPObject* np_obj, NPIdentifier** value, uint32_t* count)
{
    Ewk_JS_Object* obj = reinterpret_cast<Ewk_JS_Object*>(np_obj);
    Eina_Iterator* it;
    char* key;
    int i = 0;

    EINA_SAFETY_ON_NULL_RETURN_VAL(np_obj, false);
    EINA_MAGIC_CHECK_OR_RETURN(obj, false);

    *count = eina_hash_population(obj->properties);
    *value = static_cast<NPIdentifier*>(malloc(sizeof(NPIdentifier) * *count));
    if (!*value) {
        ERR("Could not allocate memory for NPIdentifier");
        return false;
    }

    it = eina_hash_iterator_key_new(obj->properties);
    EINA_ITERATOR_FOREACH(it, key)
        (*value)[i++] = _NPN_GetStringIdentifier(key);

    eina_iterator_free(it);
    return true;
}

static bool ewk_js_method_has(NPObject* np_obj, NPIdentifier name)
{
    Ewk_JS_Object* obj = reinterpret_cast<Ewk_JS_Object*>(np_obj);

    EINA_SAFETY_ON_NULL_RETURN_VAL(np_obj, false);
    EINA_MAGIC_CHECK_OR_RETURN(obj, false);

    if (!_NPN_IdentifierIsString(name)) {
        ERR("int NPIdentifier is not supported.");
        return false;
    }
    return eina_hash_find(obj->cls->methods, name); // Returns pointer if found(true), 0(false) otherwise.
}

static bool ewk_js_method_invoke(NPObject* np_obj, NPIdentifier name, const NPVariant* np_args, uint32_t np_arg_count, NPVariant* result)
{
    Ewk_JS_Object* obj = reinterpret_cast<Ewk_JS_Object*>(np_obj);
    Ewk_JS_Method* method;
    Ewk_JS_Variant* args;
    Ewk_JS_Variant* ret_val;

    EINA_SAFETY_ON_NULL_RETURN_VAL(np_obj, false);
    EINA_MAGIC_CHECK_OR_RETURN(obj, false);

    if (!_NPN_IdentifierIsString(name)) {
        ERR("int NPIdentifier is not supported.");
        return false;
    }

    method = static_cast<Ewk_JS_Method*>(eina_hash_find(obj->cls->methods, name));
    if (!method)
        return false;

    args = static_cast<Ewk_JS_Variant*>(malloc(sizeof(Ewk_JS_Variant)  *np_arg_count));
    if (!args) {
        ERR("Could not allocate memory for ewk_js_variant");
        return false;
    }

    for (uint32_t i = 0; i < np_arg_count; i++)
        ewk_js_npvariant_to_variant(&args[i], &np_args[i]);
    ret_val = method->invoke(obj, args, np_arg_count);
    ewk_js_variant_to_npvariant(ret_val, result);

    ewk_js_variant_free(ret_val);

    return true;
}

static Eina_Bool ewk_js_npobject_property_get(Ewk_JS_Object* obj, const char* name, Ewk_JS_Variant* value)
{
    NPIdentifier id = _NPN_GetStringIdentifier(name);
    NPVariant var;
    bool fail = _NPN_GetProperty(0, reinterpret_cast<NPObject*>(obj), id, &var);
    if (!fail)
        fail = ewk_js_npvariant_to_variant(value, &var);
    return fail;
}

static Eina_Bool ewk_js_npobject_property_set(Ewk_JS_Object* obj, const char* name, const Ewk_JS_Variant* value)
{
    NPIdentifier id = _NPN_GetStringIdentifier(name);
    NPVariant var;
    bool fail = ewk_js_variant_to_npvariant(value, &var);
    if (fail)
        fail = _NPN_SetProperty(0, reinterpret_cast<NPObject*>(obj), id, &var);
    return fail;
}

static Eina_Bool ewk_js_npobject_property_del(Ewk_JS_Object* obj, const char* name)
{
    NPIdentifier id = _NPN_GetStringIdentifier(name);
    return _NPN_RemoveProperty(0, reinterpret_cast<NPObject*>(obj), id);
}

static void ewk_js_property_free(Ewk_JS_Property* prop)
{
    free(const_cast<char*>(prop->name));
    if (prop->value.type == EWK_JS_VARIANT_STRING)
        free(prop->value.value.s);
    else if (prop->value.type == EWK_JS_VARIANT_OBJECT)
        ewk_js_object_free(prop->value.value.o);
    free(prop);
}

/**
 * Create a Ewk_JS_Class to be used in @a ewk_js_object_new.
 *
 * @param meta @a Ewk_JS_Class_Meta that describes the class to be created.
 *
 * @return The Ewk_JS_Class created.
 */
Ewk_JS_Class* ewk_js_class_new(const Ewk_JS_Class_Meta* meta)
{
    Ewk_JS_Class* cls;

    EINA_SAFETY_ON_NULL_RETURN_VAL(meta, 0);

    cls = static_cast<Ewk_JS_Class*>(malloc(sizeof(Ewk_JS_Class)));
    if (!cls) {
        ERR("Could not allocate memory for ewk_js_class");
        return 0;
    }

    cls->meta = meta;
    cls->default_prop = meta->default_prop;

    // Don't free methods since they point to meta class methods(will be freed when meta class is freed).
    cls->methods = eina_hash_pointer_new(0);
    for (int i = 0; meta->methods && meta->methods[i].name; i++) {
        NPIdentifier id = _NPN_GetStringIdentifier(meta->methods[i].name);
        eina_hash_add(cls->methods, id, &meta->methods[i]);
    }

    // Don't free properties since they point to meta class properties(will be freed when meta class is freed).
    cls->properties = eina_hash_pointer_new(0);
    for (int i = 0; meta->properties && meta->properties[i].name; i++) {
        NPIdentifier id = _NPN_GetStringIdentifier(meta->properties[i].name);
        eina_hash_add(cls->properties, id, &meta->properties[i]);
    }

    return cls;
}

/**
 * Release resources allocated by @a cls.
 *
 * @param cls @a Ewk_JS_Class to be released.
 */
void ewk_js_class_free(Ewk_JS_Class* cls)
{
    EINA_SAFETY_ON_NULL_RETURN(cls);
    eina_hash_free(cls->methods);
    eina_hash_free(cls->properties);
    free(cls);
}

static NPClass EWK_NPCLASS = {
    NP_CLASS_STRUCT_VERSION,
    0, // NPAllocateFunctionPtr
    0, // NPDeallocateFunctionPtr
    0, // NPInvalidateFunctionPtr
    ewk_js_method_has, // NPHasMethodFunctionPtr
    ewk_js_method_invoke, // NPInvokeFunctionPtr
    0, // NPInvokeDefaultFunctionPtr
    ewk_js_property_has, // NPHasPropertyFunctionPtr
    ewk_js_property_get, // NPGetPropertyFunctionPtr
    ewk_js_property_set, // NPSetPropertyFunctionPtr
    ewk_js_property_remove, // NPRemovePropertyFunctionPtr
    ewk_js_properties_enumerate, // NPEnumerationFunctionPtr
    0 // NPConstructFunction
};

static Ewk_JS_Object* ewk_js_npobject_to_object(NPObject* np_obj)
{
    NPIdentifier* values;
    uint32_t np_props_count;
    Ewk_JS_Class* cls;
    Ewk_JS_Object* obj;
    Eina_Iterator* it;
    Ewk_JS_Property* prop;
    JavaScriptObject* jso;

    if (EINA_MAGIC_CHECK(reinterpret_cast<Ewk_JS_Object*>(np_obj), EWK_JS_OBJECT_MAGIC))
        return reinterpret_cast<Ewk_JS_Object*>(np_obj);

    if (!_NPN_Enumerate(0, np_obj, &values, &np_props_count))
        return 0;

    cls = static_cast<Ewk_JS_Class*>(malloc(sizeof(Ewk_JS_Class)));
    if (!cls) {
        ERR("Could not allocate memory for ewk_js_class");
        return 0;
    }

    cls->meta = 0;
    Ewk_JS_Default def = {
        ewk_js_npobject_property_set,
        ewk_js_npobject_property_get,
        ewk_js_npobject_property_del
    };
    cls->default_prop = def;
    cls->methods = eina_hash_pointer_new(0);
    cls->properties = eina_hash_pointer_new(reinterpret_cast<Eina_Free_Cb>(ewk_js_property_free));
    for (uint32_t i = 0; i < np_props_count; i++) {
        if (_NPN_HasProperty(0, np_obj, values[i])) {
            NPVariant var;
            Ewk_JS_Property* prop = static_cast<Ewk_JS_Property*>(calloc(sizeof(Ewk_JS_Property), 1));
            if (!prop) {
                ERR("Could not allocate memory for ewk_js_property");
                goto error;
            }

            _NPN_GetProperty(0, np_obj, values[i], &var);
            ewk_js_npvariant_to_variant(&(prop->value), &var);
            prop->name = _NPN_UTF8FromIdentifier(values[i]);
            eina_hash_add(cls->properties, values[i], prop);
        }
    }

    // Can't use ewk_js_object_new(cls) because it expects cls->meta to exist.
    obj = static_cast<Ewk_JS_Object*>(malloc(sizeof(Ewk_JS_Object)));
    if (!obj) {
        ERR("Could not allocate memory for ewk_js_object");
        goto error;
    }

    free(values);
    EINA_MAGIC_SET(obj, EWK_JS_OBJECT_MAGIC);
    obj->name = 0;
    obj->cls = cls;
    obj->view = 0;

    jso = reinterpret_cast<JavaScriptObject*>(np_obj);
    if (!strcmp("Array", jso->imp->className().ascii().data()))
        obj->type = EWK_JS_OBJECT_ARRAY;
    else if (!strcmp("Function", jso->imp->className().ascii().data()))
        obj->type = EWK_JS_OBJECT_FUNCTION;
    else
        obj->type = EWK_JS_OBJECT_OBJECT;

    if (eina_hash_population(cls->properties) < 25)
        obj->properties = eina_hash_string_small_new(0);
    else
        obj->properties = eina_hash_string_superfast_new(0);

    it = eina_hash_iterator_data_new(cls->properties);
    EINA_ITERATOR_FOREACH(it, prop) {
        const char* key = prop->name;
        Ewk_JS_Variant* value = &prop->value;
        eina_hash_add(obj->properties, key, value);
    }

    eina_iterator_free(it);
    obj->base = *reinterpret_cast<JavaScriptObject*>(np_obj);

    return obj;

error:
    ewk_js_class_free(cls);
    free(values);
    return 0;
}

static Eina_Bool ewk_js_npvariant_to_variant(Ewk_JS_Variant* data, const NPVariant* result)
{
    int sz;
    EINA_SAFETY_ON_NULL_RETURN_VAL(data, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(result, EINA_FALSE);
    switch (result->type) {
    case NPVariantType_Void:
        data->type = EWK_JS_VARIANT_VOID;
        data->value.o = 0;
        break;
    case NPVariantType_Null:
        data->type = EWK_JS_VARIANT_NULL;
        data->value.o = 0;
        break;
    case NPVariantType_Int32:
        data->type = EWK_JS_VARIANT_INT32;
        data->value.i = NPVARIANT_TO_INT32(*result);
        break;
    case NPVariantType_Double:
        data->type = EWK_JS_VARIANT_DOUBLE;
        data->value.d = NPVARIANT_TO_DOUBLE(*result);
        break;
    case NPVariantType_String:
        sz = NPVARIANT_TO_STRING(*result).UTF8Length;
        data->value.s = static_cast<char*>(malloc(sizeof(char) * (sz + 1)));
        if (!data->value.s)
            return EINA_FALSE;
        memcpy(data->value.s, NPVARIANT_TO_STRING(*result).UTF8Characters, sz);
        data->value.s[sz] = '\0';
        data->type = EWK_JS_VARIANT_STRING;
        break;
    case NPVariantType_Bool:
        data->type = EWK_JS_VARIANT_BOOL;
        data->value.b = NPVARIANT_TO_BOOLEAN(*result);
        break;
    case NPVariantType_Object:
        data->type = EWK_JS_VARIANT_OBJECT;
        data->value.o = ewk_js_npobject_to_object(NPVARIANT_TO_OBJECT(*result));
        break;
    default:
        return EINA_FALSE;
    }

    return EINA_TRUE;
}

Ewk_JS_Object* ewk_js_object_new(const Ewk_JS_Class_Meta* meta_cls)
{
    Ewk_JS_Object* obj;

    EINA_SAFETY_ON_NULL_RETURN_VAL(meta_cls, 0);

    obj = static_cast<Ewk_JS_Object*>(malloc(sizeof(Ewk_JS_Object)));
    if (!obj) {
        ERR("Could not allocate memory for ewk_js_object");
        return 0;
    }

    EINA_MAGIC_SET(obj, EWK_JS_OBJECT_MAGIC);
    obj->cls = ewk_js_class_new(meta_cls);
    obj->view = 0;
    obj->name = 0;
    obj->type = EWK_JS_OBJECT_OBJECT;

    if (eina_hash_population(obj->cls->properties) < 25)
        obj->properties = eina_hash_string_small_new(reinterpret_cast<Eina_Free_Cb>(ewk_js_variant_free));
    else
        obj->properties = eina_hash_string_superfast_new(reinterpret_cast<Eina_Free_Cb>(ewk_js_variant_free));

    for (int i = 0; obj->cls->meta->properties && obj->cls->meta->properties[i].name; i++) {
        Ewk_JS_Property prop = obj->cls->meta->properties[i];
        const char* key = obj->cls->meta->properties[i].name;
        Ewk_JS_Variant* value = static_cast<Ewk_JS_Variant*>(malloc(sizeof(Ewk_JS_Variant)));
        if (!value) {
            ERR("Could not allocate memory for ewk_js_variant");
            goto error;
        }
        if (prop.get)
            prop.get(obj, key, value);
        else {
            value->type = prop.value.type;
            switch (value->type) {
            case EWK_JS_VARIANT_VOID:
            case EWK_JS_VARIANT_NULL:
                value->value.o = 0;
                break;
            case EWK_JS_VARIANT_STRING:
                value->value.s = strdup(prop.value.value.s);
                break;
            case EWK_JS_VARIANT_BOOL:
                value->value.b = prop.value.value.b;
                break;
            case EWK_JS_VARIANT_INT32:
                value->value.i = prop.value.value.i;
                break;
            case EWK_JS_VARIANT_DOUBLE:
                value->value.d = prop.value.value.d;
                break;
            case EWK_JS_VARIANT_OBJECT:
                value->value.o = prop.value.value.o;
                break;
            }
        }
        eina_hash_add(obj->properties, key, value);
    }

    obj->base.object.referenceCount = 1;
    obj->base.object._class = &EWK_NPCLASS;
    return obj;

error:
    ewk_js_object_free(obj);
    return 0;
}

void ewk_js_object_free(Ewk_JS_Object* obj)
{
    EINA_SAFETY_ON_NULL_RETURN(obj);
    EINA_MAGIC_CHECK_OR_RETURN(obj);
    Eina_Bool script_obj = !obj->cls->meta;

    eina_hash_free(obj->properties);
    eina_stringshare_del(obj->name);

    ewk_js_class_free(const_cast<Ewk_JS_Class*>(obj->cls));

    EINA_MAGIC_SET(obj, EINA_MAGIC_NONE);

    if (script_obj)
        free(obj);
}

Eina_Hash* ewk_js_object_properties_get(const Ewk_JS_Object* obj)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(obj, 0);
    EINA_MAGIC_CHECK_OR_RETURN(obj, 0);
    return obj->properties;
}

const char* ewk_js_object_name_get(const Ewk_JS_Object* obj)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(obj, 0);
    EINA_MAGIC_CHECK_OR_RETURN(obj, 0);
    return obj->name;
}

Eina_Bool ewk_js_object_invoke(Ewk_JS_Object* obj, Ewk_JS_Variant* args, int arg_count, Ewk_JS_Variant* result)
{
    NPVariant* np_args;
    NPVariant np_result;
    bool fail = EINA_FALSE;

    EINA_MAGIC_CHECK_OR_RETURN(obj, EINA_FALSE);
    if (ewk_js_object_type_get(obj) != EWK_JS_OBJECT_FUNCTION)
        return EINA_FALSE;
    if (arg_count)
        EINA_SAFETY_ON_NULL_RETURN_VAL(args, EINA_FALSE);

    np_args = static_cast<NPVariant*>(malloc(sizeof(NPVariant)  *arg_count));
    if (!np_args) {
        ERR("Could not allocate memory to method arguments");
        return EINA_FALSE;
    }

    for (int i = 0; i < arg_count; i++)
        if (!ewk_js_variant_to_npvariant(&args[i], &np_args[i]))
            goto end;

    if (!(fail = _NPN_InvokeDefault(0, reinterpret_cast<NPObject*>(obj), np_args, arg_count, &np_result)))
        goto end;
    if (result)
        fail = ewk_js_npvariant_to_variant(result, &np_result);

end:
    free(np_args);
    return fail;
}

Ewk_JS_Object_Type ewk_js_object_type_get(Ewk_JS_Object* obj)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(obj, EWK_JS_OBJECT_OBJECT);
    EINA_MAGIC_CHECK_OR_RETURN(obj, EWK_JS_OBJECT_OBJECT);

    return obj->type;
}

void ewk_js_object_type_set(Ewk_JS_Object* obj, Ewk_JS_Object_Type type)
{
    EINA_SAFETY_ON_NULL_RETURN(obj);
    EINA_MAGIC_CHECK_OR_RETURN(obj);

    obj->type = type;
}

void ewk_js_variant_free(Ewk_JS_Variant* var)
{
    EINA_SAFETY_ON_NULL_RETURN(var);
    if (var->type == EWK_JS_VARIANT_STRING)
        free(var->value.s);
    else if (var->type == EWK_JS_VARIANT_OBJECT)
        ewk_js_object_free(var->value.o);
    free(var);
}

void ewk_js_variant_array_free(Ewk_JS_Variant* var, int count)
{
    EINA_SAFETY_ON_NULL_RETURN(var);
    for (int i = 0; i < count; i++) {
        if (var[i].type == EWK_JS_VARIANT_STRING)
            free(var[i].value.s);
        else if (var[i].type == EWK_JS_VARIANT_OBJECT)
            ewk_js_object_free(var[i].value.o);
    }
    free(var);
}

#else

Eina_Hash* ewk_js_object_properties_get(const Ewk_JS_Object* obj)
{
    return 0;
}

const char* ewk_js_object_name_get(const Ewk_JS_Object* obj)
{
    return 0;
}

void ewk_js_variant_free(Ewk_JS_Variant* var)
{
}

void ewk_js_variant_array_free(Ewk_JS_Variant* var, int count)
{
}

Ewk_JS_Object* ewk_js_object_new(const Ewk_JS_Class_Meta* meta_cls)
{
    return 0;
}

void ewk_js_object_free(Ewk_JS_Object* obj)
{
}

Eina_Bool ewk_js_object_invoke(Ewk_JS_Object* obj, Ewk_JS_Variant* args, int arg_count, Ewk_JS_Variant* result)
{
    return EINA_FALSE;
}

Ewk_JS_Object_Type ewk_js_object_type_get(Ewk_JS_Object* obj)
{
    return EWK_JS_OBJECT_INVALID;
}

void ewk_js_object_type_set(Ewk_JS_Object* obj, Ewk_JS_Object_Type type)
{
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)
