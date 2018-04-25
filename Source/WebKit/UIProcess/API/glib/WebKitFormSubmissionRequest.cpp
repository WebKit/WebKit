/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitFormSubmissionRequest.h"

#include "APIDictionary.h"
#include "APIString.h"
#include "WebFormSubmissionListenerProxy.h"
#include "WebKitFormSubmissionRequestPrivate.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * SECTION: WebKitFormSubmissionRequest
 * @Short_description: Represents a form submission request
 * @Title: WebKitFormSubmissionRequest
 *
 * When a form is about to be submitted in a #WebKitWebView, the
 * #WebKitWebView::submit-form signal is emitted. Its request argument
 * contains information about the text fields of the form, that are
 * typically used to store login information, returned as lists by
 * webkit_form_submission_request_list_text_fields(). You can submit the
 * form with webkit_form_submission_request_submit().
 */

struct _WebKitFormSubmissionRequestPrivate {
    RefPtr<WebFormSubmissionListenerProxy> listener;
    GRefPtr<GPtrArray> textFieldNames;
    GRefPtr<GPtrArray> textFieldValues;
    GRefPtr<GHashTable> values;
    bool handledRequest;
};

WEBKIT_DEFINE_TYPE(WebKitFormSubmissionRequest, webkit_form_submission_request, G_TYPE_OBJECT)

static void webkitFormSubmissionRequestDispose(GObject* object)
{
    WebKitFormSubmissionRequest* request = WEBKIT_FORM_SUBMISSION_REQUEST(object);

    // Make sure the request is always handled before finalizing.
    if (!request->priv->handledRequest)
        webkit_form_submission_request_submit(request);

    G_OBJECT_CLASS(webkit_form_submission_request_parent_class)->dispose(object);
}

static void webkit_form_submission_request_class_init(WebKitFormSubmissionRequestClass* requestClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(requestClass);
    objectClass->dispose = webkitFormSubmissionRequestDispose;
}

WebKitFormSubmissionRequest* webkitFormSubmissionRequestCreate(const Vector<std::pair<String, String>>& values, Ref<WebFormSubmissionListenerProxy>&& listener)
{
    WebKitFormSubmissionRequest* request = WEBKIT_FORM_SUBMISSION_REQUEST(g_object_new(WEBKIT_TYPE_FORM_SUBMISSION_REQUEST, nullptr));
    if (values.size()) {
        request->priv->textFieldNames = adoptGRef(g_ptr_array_new_full(values.size(), g_free));
        request->priv->textFieldValues = adoptGRef(g_ptr_array_new_full(values.size(), g_free));
        for (size_t i = 0; i < values.size(); i++) {
            g_ptr_array_add(request->priv->textFieldNames.get(), g_strdup(values[i].first.utf8().data()));
            g_ptr_array_add(request->priv->textFieldValues.get(), g_strdup(values[i].second.utf8().data()));
        }
    }
    request->priv->listener = WTFMove(listener);
    return request;
}

#if PLATFORM(GTK)
/**
 * webkit_form_submission_request_get_text_fields:
 * @request: a #WebKitFormSubmissionRequest
 *
 * Get a #GHashTable with the values of the text fields contained in the form
 * associated to @request. Note that fields will be missing if the form
 * contains multiple text input elements with the same name, so this
 * function does not reliably return all text fields.
 *
 * Returns: (allow-none) (transfer none): a #GHashTable with the form
 *    text fields, or %NULL if the form doesn't contain text fields.
 *
 * Deprecated: 2.20. Use webkit_form_submission_request_list_text_fields() instead.
 */
GHashTable* webkit_form_submission_request_get_text_fields(WebKitFormSubmissionRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_FORM_SUBMISSION_REQUEST(request), nullptr);

    if (!request->priv->values && request->priv->textFieldNames->len) {
        request->priv->values = adoptGRef(g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free));
        for (unsigned i = 0; i < request->priv->textFieldNames->len; i++) {
            GUniquePtr<char> name(g_strdup(static_cast<char*>(request->priv->textFieldNames->pdata[i])));
            GUniquePtr<char> value(g_strdup(static_cast<char*>(request->priv->textFieldValues->pdata[i])));
            g_hash_table_insert(request->priv->values.get(), name.release(), value.release());
        }
    }

    return request->priv->values.get();
}
#endif

/**
 * webkit_form_submission_request_list_text_fields:
 * @request: a #WebKitFormSubmissionRequest
 * @field_names: (out) (optional) (element-type utf8) (transfer none):
 *    names of the text fields in the form
 * @field_values: (out) (optional) (element-type utf8) (transfer none):
 *    values of the text fields in the form
 *
 * Get lists with the names and values of the text fields contained in
 * the form associated to @request. Note that names and values may be
 * %NULL.
 *
 * If this function returns %FALSE, then both @field_names and
 * @field_values will be empty.
 *
 * Returns: %TRUE if the form contains text fields, or %FALSE otherwise
 *
 * Since: 2.20
 */
gboolean webkit_form_submission_request_list_text_fields(WebKitFormSubmissionRequest* request, GPtrArray** fieldNames, GPtrArray** fieldValues)
{
    g_return_val_if_fail(WEBKIT_IS_FORM_SUBMISSION_REQUEST(request), FALSE);

    if (fieldNames)
        *fieldNames = request->priv->textFieldNames.get();
    if (fieldValues)
        *fieldValues = request->priv->textFieldValues.get();

    return !!request->priv->textFieldNames->len;
}

/**
 * webkit_form_submission_request_submit:
 * @request: a #WebKitFormSubmissionRequest
 *
 * Continue the form submission.
 */
void webkit_form_submission_request_submit(WebKitFormSubmissionRequest* request)
{
    g_return_if_fail(WEBKIT_IS_FORM_SUBMISSION_REQUEST(request));

    request->priv->listener->continueSubmission();
    request->priv->handledRequest = true;
}
