/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "WebKitJavascriptResultPrivate.h"
#include "WebKitUserContentManagerPrivate.h"
#include "WebKitUserContentPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebScriptMessageHandler.h"
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
 * SECTION:WebKitUserContentManager
 * @short_description: Manages user-defined content which affects web pages.
 * @title: WebKitUserContentManager
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

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void webkit_user_content_manager_class_init(WebKitUserContentManagerClass* klass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass);

    /**
     * WebKitUserContentManager::script-message-received:
     * @manager: the #WebKitUserContentManager
     * @js_result: the #WebKitJavascriptResult holding the value received from the JavaScript world.
     *
     * This signal is emitted when JavaScript in a web view calls
     * <code>window.webkit.messageHandlers.&lt;name&gt;.postMessage()</code>, after registering
     * <code>&lt;name&gt;</code> using
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
 * webkit_user_content_manager_remove_all_scripts:
 * @manager: A #WebKitUserContentManager
 *
 * Removes all user scripts from the given #WebKitUserContentManager
 *
 * Since: 2.6
 */
void webkit_user_content_manager_remove_all_scripts(WebKitUserContentManager* manager)
{
    g_return_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(manager));
    manager->priv->userContentController->removeAllUserScripts();
}

class ScriptMessageClientGtk final : public WebScriptMessageHandler::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScriptMessageClientGtk(WebKitUserContentManager* manager, const char* handlerName)
        : m_handlerName(g_quark_from_string(handlerName))
        , m_manager(manager)
    {
    }

    void didPostMessage(WebPageProxy&, const FrameInfoData&, WebCore::SerializedScriptValue& serializedScriptValue) override
    {
        WebKitJavascriptResult* jsResult = webkitJavascriptResultCreate(serializedScriptValue);
        g_signal_emit(m_manager, signals[SCRIPT_MESSAGE_RECEIVED], m_handlerName, jsResult);
        webkit_javascript_result_unref(jsResult);
    }

    virtual ~ScriptMessageClientGtk() { }

private:
    GQuark m_handlerName;
    WebKitUserContentManager* m_manager;
};

/**
 * webkit_user_content_manager_register_script_message_handler:
 * @manager: A #WebKitUserContentManager
 * @name: Name of the script message channel
 *
 * Registers a new user script message handler. After it is registered,
 * scripts can use `window.webkit.messageHandlers.&lt;name&gt;.postMessage(value)`
 * to send messages. Those messages are received by connecting handlers
 * to the #WebKitUserContentManager::script-message-received signal. The
 * handler name is used as the detail of the signal. To avoid race
 * conditions between registering the handler name, and starting to
 * receive the signals, it is recommended to connect to the signal
 * *before* registering the handler name:
 *
 * <informalexample><programlisting>
 * WebKitWebView *view = webkit_web_view_new ();
 * WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager ();
 * g_signal_connect (manager, "script-message-received::foobar",
 *                   G_CALLBACK (handle_script_message), NULL);
 * webkit_user_content_manager_register_script_message_handler (manager, "foobar");
 * </programlisting></informalexample>
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
        WebScriptMessageHandler::create(makeUnique<ScriptMessageClientGtk>(manager, name), String::fromUTF8(name), API::UserContentWorld::normalWorld());
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
    manager->priv->userContentController->removeUserMessageHandlerForName(String::fromUTF8(name), API::UserContentWorld::normalWorld());
}

/**
 * webkit_user_content_manager_register_script_message_handler_in_world:
 * @manager: A #WebKitUserContentManager
 * @name: Name of the script message channel
 * @world_name: the name of a #WebKitScriptWorld
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
        WebScriptMessageHandler::create(makeUnique<ScriptMessageClientGtk>(manager, name), String::fromUTF8(name), webkitUserContentWorld(worldName));
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

    manager->priv->userContentController->removeUserMessageHandlerForName(String::fromUTF8(name), webkitUserContentWorld(worldName));
}

/**
 * webkit_user_content_manager_add_filter:
 * @manager: A #WebKitUserContentManager
 * @filter: A #WebKitUserContentFilter
 *
 * Adds a #WebKitUserContentFilter to the given #WebKitUserContentManager.
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
    manager->priv->userContentController->addContentRuleList(webkitUserContentFilterGetContentRuleList(filter));
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
    manager->priv->userContentController->removeContentRuleList(webkitUserContentFilterGetContentRuleList(filter).name());
}

/**
 * webkit_user_content_manager_remove_filter_by_id:
 * @manager: A #WebKitUserContentManager
 * @filter_id: Filter identifier
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
    manager->priv->userContentController->removeContentRuleList(String::fromUTF8(filterId));
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
    manager->priv->userContentController->removeAllContentRuleLists();
}

WebUserContentControllerProxy* webkitUserContentManagerGetUserContentControllerProxy(WebKitUserContentManager* manager)
{
    return manager->priv->userContentController.get();
}
