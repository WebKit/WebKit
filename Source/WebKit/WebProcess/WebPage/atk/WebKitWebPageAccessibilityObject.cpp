/*
 * Copyright (C) 2012, 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitWebPageAccessibilityObject.h"

#if ENABLE(ACCESSIBILITY)

#include "WebPage.h"
#include <WebCore/AXObjectCache.h>
#include <WebCore/AccessibilityScrollView.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/ScrollView.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;
using namespace WebCore;

struct _WebKitWebPageAccessibilityObjectPrivate {
    WebPage* page;
};

WEBKIT_DEFINE_TYPE(WebKitWebPageAccessibilityObject, webkit_web_page_accessibility_object, ATK_TYPE_PLUG)

static void coreRootObjectWrapperDetachedCallback(AtkObject* wrapper, const char*, gboolean value, AtkObject* atkObject)
{
    if (!value)
        return;

    g_signal_emit_by_name(atkObject, "children-changed::remove", 0, wrapper);
}

static AccessibilityObjectWrapper* rootWebAreaWrapper(AXCoreObject& rootObject)
{
    if (!rootObject.isScrollView())
        return nullptr;

    if (auto* webAreaObject = rootObject.webAreaObject())
        return webAreaObject->wrapper();

    return nullptr;
}

static AtkObject* accessibilityRootObjectWrapper(AtkObject* atkObject)
{
    if (!AXObjectCache::accessibilityEnabled())
        AXObjectCache::enableAccessibility();

    auto* accessible = WEBKIT_WEB_PAGE_ACCESSIBILITY_OBJECT(atkObject);
    if (!accessible->priv->page)
        return nullptr;

    Page* corePage = accessible->priv->page->corePage();
    if (!corePage)
        return nullptr;

    Frame& coreFrame = corePage->mainFrame();
    if (!coreFrame.document())
        return nullptr;

    AXObjectCache* cache = coreFrame.document()->axObjectCache();
    if (!cache)
        return nullptr;

    AXCoreObject* coreRootObject = cache->rootObject();
    if (!coreRootObject)
        return nullptr;

    auto* wrapper = ATK_OBJECT(coreRootObject->wrapper());
    if (!wrapper)
        return nullptr;

    if (atk_object_peek_parent(wrapper) != ATK_OBJECT(accessible)) {
        atk_object_set_parent(wrapper, ATK_OBJECT(accessible));
        g_signal_emit_by_name(accessible, "children-changed::add", 0, wrapper);

        if (auto* webAreaWrapper = rootWebAreaWrapper(*coreRootObject)) {
            g_signal_connect_object(webAreaWrapper, "state-change::defunct",
                G_CALLBACK(coreRootObjectWrapperDetachedCallback), accessible, static_cast<GConnectFlags>(0));
        }
    }

    return wrapper;
}

static void webkitWebPageAccessibilityObjectInitialize(AtkObject* atkObject, gpointer data)
{
    if (ATK_OBJECT_CLASS(webkit_web_page_accessibility_object_parent_class)->initialize)
        ATK_OBJECT_CLASS(webkit_web_page_accessibility_object_parent_class)->initialize(atkObject, data);

    WEBKIT_WEB_PAGE_ACCESSIBILITY_OBJECT(atkObject)->priv->page = reinterpret_cast<WebPage*>(data);
    atk_object_set_role(atkObject, ATK_ROLE_FILLER);
}

static gint webkitWebPageAccessibilityObjectGetIndexInParent(AtkObject*)
{
    // An AtkPlug is the only child an AtkSocket can have.
    return 0;
}

static gint webkitWebPageAccessibilityObjectGetNChildren(AtkObject* atkObject)
{
    return accessibilityRootObjectWrapper(atkObject) ? 1 : 0;
}

static AtkObject* webkitWebPageAccessibilityObjectRefChild(AtkObject* atkObject, gint index)
{
    // It's supposed to have either one child or zero.
    if (index && index != 1)
        return nullptr;

    if (auto* rootObjectWrapper = accessibilityRootObjectWrapper(atkObject))
        return ATK_OBJECT(g_object_ref(rootObjectWrapper));

    return nullptr;
}

static void webkit_web_page_accessibility_object_class_init(WebKitWebPageAccessibilityObjectClass* klass)
{
    AtkObjectClass* atkObjectClass = ATK_OBJECT_CLASS(klass);
    // No need to implement get_parent() here since this is a subclass
    // of AtkPlug and all the logic related to that function will be
    // implemented by the ATK bridge.
    atkObjectClass->initialize = webkitWebPageAccessibilityObjectInitialize;
    atkObjectClass->get_index_in_parent = webkitWebPageAccessibilityObjectGetIndexInParent;
    atkObjectClass->get_n_children = webkitWebPageAccessibilityObjectGetNChildren;
    atkObjectClass->ref_child = webkitWebPageAccessibilityObjectRefChild;
}

AtkObject* webkitWebPageAccessibilityObjectNew(WebPage* page)
{
    AtkObject* object = ATK_OBJECT(g_object_new(WEBKIT_TYPE_WEB_PAGE_ACCESSIBILITY_OBJECT, nullptr));
    atk_object_initialize(object, page);
    return object;
}

#endif // ENABLE(ACCESSIBILITY)
