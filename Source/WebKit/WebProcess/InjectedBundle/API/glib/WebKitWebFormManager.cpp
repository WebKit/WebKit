/*
 * Copyright (C) 2022 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitWebFormManager.h"

#include "WebKitWebFormManagerPrivate.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/JSNode.h>
#include <jsc/JSCContextPrivate.h>
#include <jsc/JSCValuePrivate.h>

using namespace WebCore;

/**
 * WebKitWebFormManager:
 *
 * Form manager of a #WebKitWebPage in a #WebKitScriptWorld
 *
 * Since: 2.40
 */

enum {
    FORM_CONTROLS_ASSOCIATED,
    WILL_SEND_SUBMIT_EVENT,
    WILL_SUBMIT_FORM,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitWebFormManager, webkit_web_form_manager, G_TYPE_OBJECT)

static void webkit_web_form_manager_init(WebKitWebFormManager*)
{
}

static void webkit_web_form_manager_class_init(WebKitWebFormManagerClass* klass)
{
    /**
     * WebKitWebFormManager::form-controls-associated:
     * @form_manager: the #WebKitWebFormManager on which the signal is emitted
     * @frame: a #WebKitFrame
     * @elements: (element-type JSCValue) (transfer none): a #GPtrArray of
     *     #JSCValue with the list of forms in the page
     *
     * Emitted after form elements (or form associated elements) are associated to @frame.
     * This is useful to implement form auto filling for web pages where form fields are added
     * dynamically. This signal might be emitted multiple times for the same frame.
     *
     * Note that this signal could be also emitted when form controls are moved between forms. In
     * that case, the @elements array carries the list of those elements which have moved.
     *
     * Clients should take a reference to the members of the @elements array if it is desired to
     * keep them alive after the signal handler returns.
     *
     * Since: 2.40
     */
    signals[FORM_CONTROLS_ASSOCIATED] = g_signal_new(
        "form-controls-associated",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        WEBKIT_TYPE_FRAME,
        G_TYPE_PTR_ARRAY);

    /**
     * WebKitWebFormManager::will-send-submit-event:
     * @form_manager: the #WebKitWebFormManager on which the signal is emitted
     * @form: the #JSCValue to be submitted, which will always correspond to an HTMLFormElement
     * @source_frame: the #WebKitFrame containing the form to be submitted
     * @target_frame: the #WebKitFrame containing the form's target,
     *     which may be the same as @source_frame if no target was specified
     *
     * This signal is emitted when the DOM submit event is about to be fired for @form.
     * JavaScript code may rely on the submit event to detect that the user has clicked
     * on a submit button, and to possibly cancel the form submission before
     * #WebKitWebFormManager::will-submit-form signal is emitted.
     * However, beware that, for historical reasons, the submit event is not emitted at
     * all if the form submission is triggered by JavaScript. For these reasons,
     * this signal may not be used to reliably detect whether a form will be submitted.
     * Instead, use it to detect if a user has clicked on a form's submit button even if
     * JavaScript later cancels the form submission, or to read the values of the form's
     * fields even if JavaScript later clears certain fields before submitting. This may
     * be needed, for example, to implement a robust browser password manager, as some
     * misguided websites may use such techniques to attempt to thwart password managers.
     *
     * Since: 2.40
     */
    signals[WILL_SEND_SUBMIT_EVENT] = g_signal_new(
        "will-send-submit-event",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 3,
        JSC_TYPE_VALUE,
        WEBKIT_TYPE_FRAME,
        WEBKIT_TYPE_FRAME);

    /**
     * WebKitWebFormManager::will-submit-form:
     * @form_manager: the #WebKitWebFormManager on which the signal is emitted
     * @form: the #JSCValue to be submitted, which will always correspond to an HTMLFormElement
     * @source_frame: the #WebKitFrame containing the form to be submitted
     * @target_frame: the #WebKitFrame containing the form's target,
     *     which may be the same as @source_frame if no target was specified
     *
     * This signal is emitted when @form will imminently be submitted. It can no longer
     * be cancelled. This event always occurs immediately before a form is submitted to
     * its target, so use this event to reliably detect when a form is submitted. This
     * signal is emitted after #WebKitWebFormManager::will-send-submit-event if that
     * signal is emitted.
     *
     * Since: 2.40
     */
    signals[WILL_SUBMIT_FORM] = g_signal_new(
        "will-submit-form",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 3,
        JSC_TYPE_VALUE,
        WEBKIT_TYPE_FRAME,
        WEBKIT_TYPE_FRAME);
}

WebKitWebFormManager* webkitWebFormManagerCreate()
{
    return WEBKIT_WEB_FORM_MANAGER(g_object_new(WEBKIT_TYPE_WEB_FORM_MANAGER, nullptr));
}

void webkitWebFormManagerDidAssociateFormControls(WebKitWebFormManager* formManager, WebKitFrame* frame, Vector<GRefPtr<JSCValue>>&& elements)
{
    GRefPtr<GPtrArray> formElements = adoptGRef(g_ptr_array_sized_new(elements.size()));
    for (const auto& element : elements)
        g_ptr_array_add(formElements.get(), element.get());
    g_signal_emit(formManager, signals[FORM_CONTROLS_ASSOCIATED], 0, frame, formElements.get());
}

void webkitWebFormManagerWillSendSubmitEvent(WebKitWebFormManager* formManager, GRefPtr<JSCValue>&& form, WebKitFrame* sourceFrame, WebKitFrame* targetFrame)
{
    g_signal_emit(formManager, signals[WILL_SEND_SUBMIT_EVENT], 0, form.get(), sourceFrame, targetFrame);
}

void webkitWebFormManagerWillSubmitForm(WebKitWebFormManager* formManager, GRefPtr<JSCValue>&& form, WebKitFrame* sourceFrame, WebKitFrame* targetFrame)
{
    g_signal_emit(formManager, signals[WILL_SUBMIT_FORM], 0, form.get(), sourceFrame, targetFrame);
}

static Node* nodeForJSCValue(JSCValue* value)
{
    auto* jsObject = JSValueToObject(jscContextGetJSContext(jsc_value_get_context(value)), jscValueGetJSValue(value), nullptr);
    return jsObject ? JSNode::toWrapped(toJS(jsObject)->vm(), toJS(jsObject)) : nullptr;
}

/**
 * webkit_web_form_manager_input_element_is_user_edited:
 * @element: a #JSCValue
 *
 * Get whether @element is an HTML text input element that has been edited by a user action.
 *
 * Returns: %TRUE if @element is an HTML text input element that has been edited by a user action,
 *    or %FALSE otherwise
 *
 * Since: 2.40
 */
gboolean webkit_web_form_manager_input_element_is_user_edited(JSCValue* element)
{
    g_return_val_if_fail(JSC_IS_VALUE(element), FALSE);
    g_return_val_if_fail(jsc_value_is_object(element), FALSE);

    auto* node = nodeForJSCValue(element);
    if (is<HTMLInputElement>(node))
        return downcast<HTMLInputElement>(*node).lastChangeWasUserEdit();

    if (is<HTMLTextAreaElement>(node))
        return downcast<HTMLTextAreaElement>(*node).lastChangeWasUserEdit();

    return FALSE;
}

/**
 * webkit_web_form_manager_input_element_auto_fill:
 * @element: a #JSCValue
 * @value: the text to set
 *
 * Set the value of an HTML input element as if it had been edited by
 * the user, triggering a change event, and set it as filled automatically.
 * If @element is not an HTML input element this function does nothing.
 *
 * Since: 2.40
 */
void webkit_web_form_manager_input_element_auto_fill(JSCValue* element, const char* value)
{
    g_return_if_fail(JSC_IS_VALUE(element));
    g_return_if_fail(jsc_value_is_object(element));

    auto* node = nodeForJSCValue(element);
    if (!is<WebCore::HTMLInputElement>(node))
        return;

    auto& inputElement = downcast<WebCore::HTMLInputElement>(*node);
    inputElement.setAutoFilled(true);
    inputElement.setValueForUser(String::fromUTF8(value));
}

/**
 * webkit_web_form_manager_input_element_is_auto_filled:
 * @element: a #JSCValue
 *
 * Get whether @element is an HTML input element that has been filled automatically.
 *
 * Returns: %TRUE if @element is an HTML input element that has been filled automatically,
 *    or %FALSE otherwise
 *
 * Since: 2.40
 */
gboolean webkit_web_form_manager_input_element_is_auto_filled(JSCValue* element)
{
    g_return_val_if_fail(JSC_IS_VALUE(element), FALSE);
    g_return_val_if_fail(jsc_value_is_object(element), FALSE);

    auto* node = nodeForJSCValue(element);
    return is<WebCore::HTMLInputElement>(node) ? downcast<WebCore::HTMLInputElement>(*node).isAutoFilled() : FALSE;
}
