/*
 * Copyright (C) 2014, 2020 Igalia S.L.
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
#include "WebKitUserContentManager.h"

#include "APISerializedScriptValue.h"
#include "InjectUserScriptImmediately.h"
#include "WebKitInitialize.h"
#include "WebKitJavascriptResultPrivate.h"
#include "WebKitUserContentManagerPrivate.h"
#include "WebKitUserContentPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebScriptMessageHandler.h"
#include <wtf/CompletionHandler.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

#if PLATFORM(WPE)
#include "WPEView.h"
#endif

using namespace WebCore;
using namespace WebKit;

struct _WebKitUserContentManagerPrivate {
    _WebKitUserContentManagerPrivate()
        : userContentController(adoptRef(new WebUserContentControllerProxy))
    {
    }

    RefPtr<WebUserContentControllerProxy> userContentController;
};

/**
 * WebKitUserContentManager:
 *
 * Manages user-defined content which affects web pages.
 *
 * Using a #WebKitUserContentManager user CSS style sheets can be set to
 * be injected in the web pages loaded by a #WebKitWebView, by
 * webkit_user_content_manager_add_style_sheet().
 *
 * To use a #WebKitUserContentManager, it must be created using
 * webkit_user_content_manager_new(), and then passed to
 * webkit_web_view_new_with_user_content_manager(). User style
 * sheets can be created with webkit_user_style_sheet_new().
 *
 * User style sheets can be added and removed at any time, but
 * they will affect the web pages loaded afterwards.
 *
 * Since: 2.6
 */

WEBKIT_DEFINE_TYPE(WebKitUserContentManager, webkit_user_content_manager, G_TYPE_OBJECT)

enum {
    SCRIPT_MESSAGE_RECEIVED,
    SCRIPT_MESSAGE_WITH_REPLY_RECEIVED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void webkit_user_content_manager_class_init(WebKitUserContentManagerClass* klass)
{
    webkitInitialize();

    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass);

    /**
     * WebKitUserContentManager::script-message-received:
     * @manager: the #WebKitUserContentManager
     * @js_result: the #WebKitJavascriptResult holding the value received from the JavaScript world.
     *
     * This signal is emitted when JavaScript in a web view calls
     * <code>window.webkit.messageHandlers.<name>.postMessage()</code>, after registering
     * <code><name></code> using
     * webkit_user_content_manager_register_script_message_handler()
     *
     * Since: 2.8
     */
    signals[SCRIPT_MESSAGE_RECEIVED] =
        g_signal_new(
            "script-message-received",
            G_TYPE_FROM_CLASS(gObjectClass),
            static_cast<GSignalFlags>(G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
            0, nullptr, nullptr,
            g_cclosure_marshal_VOID__BOXED,
            G_TYPE_NONE, 1,
            WEBKIT_TYPE_JAVASCRIPT_RESULT);

    /**
     * WebKitUserContentManager::script-message-with-reply-received:
     * @manager: the #WebKitUserContentManager
     * @js_result: the #WebKitJavascriptResult holding the value received from the JavaScript world.
     * @reply: the #WebKitScriptMessageReply to send the reply to the script message.
     *
     * This signal is emitted when JavaScript in a web view calls
     * <code>window.webkit.messageHandlers.<name>.postMessage()</code>, after registering
     * <code><name></code> using
     * webkit_user_content_manager_register_script_message_handler_with_reply()
     *
     * The given @reply can be used to send a return value with
     * webkit_script_message_reply_return_value() or an error message with
     * webkit_script_message_reply_return_error_message(). If none of them are
     * called, an automatic reply with an undefined value will be sent.
     *
     * It is possible to handle the reply asynchronously, by simply calling
     * g_object_ref() on the @reply and returning %TRUE.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     *
     * Since: 2.40
     */
    signals[SCRIPT_MESSAGE_WITH_REPLY_RECEIVED] =
        g_signal_new(
            "script-message-with-reply-received",
            G_TYPE_FROM_CLASS(gObjectClass),
            static_cast<GSignalFlags>(G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
            0, g_signal_accumulator_true_handled, nullptr,
            g_cclosure_marshal_generic,
            G_TYPE_BOOLEAN, 2,
            WEBKIT_TYPE_JAVASCRIPT_RESULT,
            WEBKIT_TYPE_SCRIPT_MESSAGE_REPLY);
}

/**
 * webkit_user_content_manager_new:
 *
 * Creates a new user content manager.
 *
 * Returns: A #WebKitUserContentManager
 *
 * Since: 2.6
 */
WebKitUserContentManager* webkit_user_content_manager_new()
{
    return WEBKIT_USER_CONTENT_MANAGER(g_object_new(WEBKIT_TYPE_USER_CONTENT_MANAGER, nullptr));
}

/**
 * webkit_user_content_manager_add_style_sheet:
 * @manager: A #WebKitUserContentManager
 * @stylesheet: A #WebKitUserStyleSheet
 *
 * Adds a #WebKitUserStyleSheet to the given #WebKitUserContentManager.
 *
 * The same #WebKitUserStyleSheet can be reused with multiple
 * #WebKitUserContentManager instances.
 *
 * Since: 2.6
 */
void webkit_user_content_manager_add_style_sheet(WebKitUserContentManager* manager, WebKitUserStyleSheet* styleSheet)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(styleSheet);
    manager->priv->userContentController->addUserStyleSheet(webkitUserStyleSheetGetUserStyleSheet(styleSheet));
}

/**
 * webkit_user_content_manager_remove_style_sheet:
 * @manager: A #WebKitUserContentManager
 * @stylesheet: A #WebKitUserStyleSheet
 *
 * Removes a #WebKitUserStyleSheet from the given #WebKitUserContentManager.
 *
 * See also webkit_user_content_manager_remove_all_style_sheets().
 *
 * Since: 2.32
 */
void webkit_user_content_manager_remove_style_sheet(WebKitUserContentManager* manager, WebKitUserStyleSheet* styleSheet)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(styleSheet);
    manager->priv->userContentController->removeUserStyleSheet(webkitUserStyleSheetGetUserStyleSheet(styleSheet));
}

/**
 * webkit_user_content_manager_remove_all_style_sheets:
 * @manager: A #WebKitUserContentManager
 *
 * Removes all user style sheets from the given #WebKitUserContentManager.
 *
 * Since: 2.6
 */
void webkit_user_content_manager_remove_all_style_sheets(WebKitUserContentManager* manager)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    manager->priv->userContentController->removeAllUserStyleSheets();
}

/**
 * webkit_user_content_manager_add_script:
 * @manager: A #WebKitUserContentManager
 * @script: A #WebKitUserScript
 *
 * Adds a #WebKitUserScript to the given #WebKitUserContentManager.
 *
 * The same #WebKitUserScript can be reused with multiple
 * #WebKitUserContentManager instances.
 *
 * Since: 2.6
 */
void webkit_user_content_manager_add_script(WebKitUserContentManager* manager, WebKitUserScript* script)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(script);
    manager->priv->userContentController->addUserScript(webkitUserScriptGetUserScript(script), InjectUserScriptImmediately::No);
}

/**
 * webkit_user_content_manager_remove_script:
 * @manager: A #WebKitUserContentManager
 * @script: A #WebKitUserScript
 *
 * Removes a #WebKitUserScript from the given #WebKitUserContentManager.
 *
 * See also webkit_user_content_manager_remove_all_scripts().
 *
 * Since: 2.32
 */
void webkit_user_content_manager_remove_script(WebKitUserContentManager* manager, WebKitUserScript* script)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(script);
    manager->priv->userContentController->removeUserScript(webkitUserScriptGetUserScript(script));
}

/**
 * webkit_user_content_manager_remove_all_scripts:
 * @manager: A #WebKitUserContentManager
 *
 * Removes all user scripts from the given #WebKitUserContentManager
 *
 * See also webkit_user_content_manager_remove_script().
 *
 * Since: 2.6
 */
void webkit_user_content_manager_remove_all_scripts(WebKitUserContentManager* manager)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    manager->priv->userContentController->removeAllUserScripts();
}

/**
 * WebKitScriptMessageReply: (ref-func webkit_script_message_reply_ref) (unref-func webkit_script_message_reply_unref)
 *
 * A reply for a script message received.
 * If no reply has been sent by the user, an automatically generated reply with
 * undefined value with be sent.
 *
 * Since: 2.40
 */
struct _WebKitScriptMessageReply {
    _WebKitScriptMessageReply(WTF::Function<void(API::SerializedScriptValue*, const String&)>&& completionHandler)
        : completionHandler(WTFMove(completionHandler))
        , referenceCount(1)
    {
    }

    void sendValue(JSCValue* value)
    {
        auto serializedValue = API::SerializedScriptValue::createFromJSCValue(value);
        completionHandler(serializedValue.get(), { });
    }

    void sendErrorMessage(const char* errorMessage)
    {
        completionHandler(nullptr, String::fromUTF8(errorMessage));
    }

    ~_WebKitScriptMessageReply()
    {
        if (completionHandler) {
            auto value = adoptGRef(jsc_value_new_undefined(API::SerializedScriptValue::sharedJSCContext()));
            sendValue(value.get());
        }
    }

    WTF::CompletionHandler<void(API::SerializedScriptValue*, const String&)> completionHandler;
    int referenceCount;
};

G_DEFINE_BOXED_TYPE(WebKitScriptMessageReply, webkit_script_message_reply, webkit_script_message_reply_ref, webkit_script_message_reply_unref)

/**
 * webkit_script_message_reply_ref:
 * @script_message_reply: A #WebKitScriptMessageReply
 *
 * Atomically increments the reference count of @script_message_reply by one.
 *
 * Returns: the @script_message_reply passed in.
 *
 * Since: 2.40
 */
WebKitScriptMessageReply*
webkit_script_message_reply_ref(WebKitScriptMessageReply* scriptMessageReply)
{
    g_return_val_if_fail(scriptMessageReply, nullptr);
    g_atomic_int_inc(&scriptMessageReply->referenceCount);
    return scriptMessageReply;
}

/**
 * webkit_script_message_reply_unref:
 * @script_message_reply: A #WebKitScriptMessageReply
 *
 * Atomically decrements the reference count of @script_message_reply by one.
 *
 * If the reference count drops to 0, all the memory allocated by the
 * #WebKitScriptMessageReply is released. This function is MT-safe and may
 * be called from any thread.
 *
 * Since: 2.40
 */
void webkit_script_message_reply_unref(WebKitScriptMessageReply* scriptMessageReply)
{
    g_return_if_fail(scriptMessageReply);
    if (g_atomic_int_dec_and_test(&scriptMessageReply->referenceCount)) {
        scriptMessageReply->~WebKitScriptMessageReply();
        fastFree(scriptMessageReply);
    }
}

WebKitScriptMessageReply* webKitScriptMessageReplyCreate(WTF::Function<void(API::SerializedScriptValue*, const String&)>&& completionHandler)
{
    WebKitScriptMessageReply* scriptMessageReply = static_cast<WebKitScriptMessageReply*>(fastMalloc(sizeof(WebKitScriptMessageReply)));
    new (scriptMessageReply) WebKitScriptMessageReply(WTFMove(completionHandler));
    return scriptMessageReply;
}

/**
 * webkit_script_message_reply_return_value:
 * @script_message_reply: A #WebKitScriptMessageReply
 * @reply_value: Reply value of the provided script message
 *
 * Reply to a script message with a value.
 *
 * This function can be called twice for passing the reply value in.
 *
 * Since: 2.40
 */
void webkit_script_message_reply_return_value(WebKitScriptMessageReply* message, JSCValue* replyValue)
{
    g_return_if_fail(message != nullptr);
    g_return_if_fail(message->completionHandler);

    message->sendValue(replyValue);
}

/**
 * webkit_script_message_reply_return_error_message:
 * @script_message_reply: A #WebKitScriptMessageReply
 * @error_message: An error message to return as specified by the user's script message
 *
 * Reply to a script message with an error message.
 *
 * Since: 2.40
 */
void
webkit_script_message_reply_return_error_message(WebKitScriptMessageReply* message, const char* errorMessage)
{
    g_return_if_fail(message != nullptr);
    g_return_if_fail(errorMessage != nullptr);
    g_return_if_fail(message->completionHandler);

    message->sendErrorMessage(errorMessage);
}

class ScriptMessageClientGtk final : public WebScriptMessageHandler::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScriptMessageClientGtk(WebKitUserContentManager* manager, const char* handlerName, bool supportsAsyncReply)
        : m_handlerName(g_quark_from_string(handlerName))
        , m_manager(manager)
        , m_supportsAsyncReply(supportsAsyncReply)
    {
    }

    void didPostMessage(WebPageProxy&, FrameInfoData&&, API::ContentWorld&, WebCore::SerializedScriptValue& serializedScriptValue) override
    {
        WebKitJavascriptResult* jsResult = webkitJavascriptResultCreate(serializedScriptValue);
        g_signal_emit(m_manager, signals[SCRIPT_MESSAGE_RECEIVED], m_handlerName, jsResult);
        webkit_javascript_result_unref(jsResult);
    }

    bool supportsAsyncReply() override
    {
        return m_supportsAsyncReply;
    }

    void didPostMessageWithAsyncReply(WebPageProxy&, FrameInfoData&&, API::ContentWorld&, WebCore::SerializedScriptValue& serializedScriptValue, WTF::Function<void(API::SerializedScriptValue*, const String&)>&& completionHandler) override
    {
        WebKitJavascriptResult* jsResult = webkitJavascriptResultCreate(serializedScriptValue);
        WebKitScriptMessageReply* message = webKitScriptMessageReplyCreate(WTFMove(completionHandler));
        gboolean returnValue;
        g_signal_emit(m_manager, signals[SCRIPT_MESSAGE_WITH_REPLY_RECEIVED], m_handlerName, jsResult, message, &returnValue);
        webkit_javascript_result_unref(jsResult);
        webkit_script_message_reply_unref(message);
    }

    virtual ~ScriptMessageClientGtk() { }

private:
    GQuark m_handlerName;
    WebKitUserContentManager* m_manager;
    bool m_supportsAsyncReply;
};

/**
 * webkit_user_content_manager_register_script_message_handler:
 * @manager: A #WebKitUserContentManager
 * @name: Name of the script message channel
 *
 * Registers a new user script message handler.
 *
 * After it is registered,
 * scripts can use `window.webkit.messageHandlers.<name>.postMessage(value)`
 * to send messages. Those messages are received by connecting handlers
 * to the #WebKitUserContentManager::script-message-received signal. The
 * handler name is used as the detail of the signal. To avoid race
 * conditions between registering the handler name, and starting to
 * receive the signals, it is recommended to connect to the signal
 * *before* registering the handler name:
 *
 * ```c
 * WebKitWebView *view = webkit_web_view_new ();
 * WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager ();
 * g_signal_connect (manager, "script-message-received::foobar",
 *                   G_CALLBACK (handle_script_message), NULL);
 * webkit_user_content_manager_register_script_message_handler (manager, "foobar");
 * ```
 *
 * Registering a script message handler will fail if the requested
 * name has been already registered before.
 *
 * Returns: %TRUE if message handler was registered successfully, or %FALSE otherwise.
 *
 * Since: 2.8
 */
gboolean webkit_user_content_manager_register_script_message_handler(WebKitUserContentManager* manager, const char* name)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager), FALSE);
    g_return_val_if_fail(name, FALSE);

    Ref<WebScriptMessageHandler> handler =
        WebScriptMessageHandler::create(makeUnique<ScriptMessageClientGtk>(manager, name, false), AtomString::fromUTF8(name), API::ContentWorld::pageContentWorld());
    return manager->priv->userContentController->addUserScriptMessageHandler(handler.get());
}

/**
 * webkit_user_content_manager_unregister_script_message_handler:
 * @manager: A #WebKitUserContentManager
 * @name: Name of the script message channel
 *
 * Unregisters a previously registered message handler.
 *
 * Note that this does *not* disconnect handlers for the
 * #WebKitUserContentManager::script-message-received signal;
 * they will be kept connected, but the signal will not be emitted
 * unless the handler name is registered again.
 *
 * See also webkit_user_content_manager_register_script_message_handler().
 *
 * Since: 2.8
 */
void webkit_user_content_manager_unregister_script_message_handler(WebKitUserContentManager* manager, const char* name)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(name);
    manager->priv->userContentController->removeUserMessageHandlerForName(String::fromUTF8(name), API::ContentWorld::pageContentWorld());
}

/**
 * webkit_user_content_manager_register_script_message_handler_with_reply:
 * @manager: A #WebKitUserContentManager
 * @name: Name of the script message channel
 * @world_name (nullable): the name of a #WebKitScriptWorld
 *
 * Registers a new user script message handler in script world with name @world_name.
 *
 * Different from webkit_user_content_manager_register_script_message_handler(),
 * when using this function to register the handler, the connected signal is
 * script-message-with-reply-received, and a reply provided by the user is expected.
 * Otherwise, the user will receive a default undefined value.
 *
 * If %NULL is passed as the @world_name, the default world will be used.
 * See webkit_user_content_manager_register_script_message_handler() for full description.
 *
 * Registering a script message handler will fail if the requested
 * name has been already registered before.
 *
 * The registered handler can be unregistered by using
 * #webkit_user_content_manager_unregister_script_message_handler() and
 * #webkit_user_content_manager_unregister_script_message_handler_in_world().
 *
 * Returns: %TRUE if message handler was registered successfully, or %FALSE otherwise.
 *
 * Since: 2.40
 */
gboolean webkit_user_content_manager_register_script_message_handler_with_reply(WebKitUserContentManager* manager, const char* name, const char* worldName)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager), FALSE);
    g_return_val_if_fail(name, FALSE);

    auto handler = WebScriptMessageHandler::create(makeUnique<ScriptMessageClientGtk>(manager, name, true), AtomString::fromUTF8(name), worldName ? webkitContentWorld(worldName) : API::ContentWorld::pageContentWorld());
    return manager->priv->userContentController->addUserScriptMessageHandler(handler.get());
}

/**
 * webkit_user_content_manager_register_script_message_handler_in_world:
 * @manager: A #WebKitUserContentManager
 * @name: Name of the script message channel
 * @world_name: the name of a #WebKitScriptWorld
 *
 * Registers a new user script message handler in script world.
 *
 * Registers a new user script message handler in script world with name @world_name.
 * See webkit_user_content_manager_register_script_message_handler() for full description.
 *
 * Registering a script message handler will fail if the requested
 * name has been already registered before.
 *
 * Returns: %TRUE if message handler was registered successfully, or %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean webkit_user_content_manager_register_script_message_handler_in_world(WebKitUserContentManager* manager, const char* name, const char* worldName)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager), FALSE);
    g_return_val_if_fail(name, FALSE);
    g_return_val_if_fail(worldName, FALSE);

    Ref<WebScriptMessageHandler> handler =
        WebScriptMessageHandler::create(makeUnique<ScriptMessageClientGtk>(manager, name, false), AtomString::fromUTF8(name), webkitContentWorld(worldName));
    return manager->priv->userContentController->addUserScriptMessageHandler(handler.get());
}

/**
 * webkit_user_content_manager_unregister_script_message_handler_in_world:
 * @manager: A #WebKitUserContentManager
 * @name: Name of the script message channel
 * @world_name: the name of a #WebKitScriptWorld
 *
 * Unregisters a previously registered message handler in script world with name @world_name.
 *
 * Note that this does *not* disconnect handlers for the
 * #WebKitUserContentManager::script-message-received signal;
 * they will be kept connected, but the signal will not be emitted
 * unless the handler name is registered again.
 *
 * See also webkit_user_content_manager_register_script_message_handler_in_world().
 *
 * Since: 2.22
 */
void webkit_user_content_manager_unregister_script_message_handler_in_world(WebKitUserContentManager* manager, const char* name, const char* worldName)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(name);
    g_return_if_fail(worldName);

    manager->priv->userContentController->removeUserMessageHandlerForName(String::fromUTF8(name), webkitContentWorld(worldName));
}

/**
 * webkit_user_content_manager_add_filter:
 * @manager: A #WebKitUserContentManager
 * @filter: A #WebKitUserContentFilter
 *
 * Adds a #WebKitUserContentFilter to the given #WebKitUserContentManager.
 *
 * The same #WebKitUserContentFilter can be reused with multiple
 * #WebKitUserContentManager instances.
 *
 * Filters need to be saved and loaded from #WebKitUserContentFilterStore.
 *
 * Since: 2.24
 */
void webkit_user_content_manager_add_filter(WebKitUserContentManager* manager, WebKitUserContentFilter* filter)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(filter);
#if ENABLE(CONTENT_EXTENSIONS)
    manager->priv->userContentController->addContentRuleList(webkitUserContentFilterGetContentRuleList(filter));
#endif
}

/**
 * webkit_user_content_manager_remove_filter:
 * @manager: A #WebKitUserContentManager
 * @filter: A #WebKitUserContentFilter
 *
 * Removes a filter from the given #WebKitUserContentManager.
 *
 * Since 2.24
 */
void webkit_user_content_manager_remove_filter(WebKitUserContentManager* manager, WebKitUserContentFilter* filter)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(filter);
#if ENABLE(CONTENT_EXTENSIONS)
    manager->priv->userContentController->removeContentRuleList(webkitUserContentFilterGetContentRuleList(filter).name());
#endif
}

/**
 * webkit_user_content_manager_remove_filter_by_id:
 * @manager: A #WebKitUserContentManager
 * @filter_id: Filter identifier
 *
 * Removes a filter by the given identifier.
 *
 * Removes a filter from the given #WebKitUserContentManager given the
 * identifier of a #WebKitUserContentFilter as returned by
 * webkit_user_content_filter_get_identifier().
 *
 * Since: 2.26
 */
void webkit_user_content_manager_remove_filter_by_id(WebKitUserContentManager* manager, const char* filterId)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    g_return_if_fail(filterId);
#if ENABLE(CONTENT_EXTENSIONS)
    manager->priv->userContentController->removeContentRuleList(String::fromUTF8(filterId));
#endif
}

/**
 * webkit_user_content_manager_remove_all_filters:
 * @manager: A #WebKitUserContentManager
 *
 * Removes all content filters from the given #WebKitUserContentManager.
 *
 * Since: 2.24
 */
void webkit_user_content_manager_remove_all_filters(WebKitUserContentManager* manager)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
#if ENABLE(CONTENT_EXTENSIONS)
    manager->priv->userContentController->removeAllContentRuleLists();
#endif
}

WebUserContentControllerProxy* webkitUserContentManagerGetUserContentControllerProxy(WebKitUserContentManager* manager)
{
    return manager->priv->userContentController.get();
}
