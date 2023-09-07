/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPITabs.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionTabParameters.h"
#import "WebExtensionTabQueryParameters.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

static NSString * const idKey = @"id";
static NSString * const urlKey = @"url";
static NSString * const titleKey = @"title";

static NSString * const windowIdKey = @"windowId";
static NSString * const indexKey = @"index";
static NSString * const openerTabIdKey = @"openerTabId";

static NSString * const widthKey = @"width";
static NSString * const heightKey = @"height";

static NSString * const activeKey = @"active";
static NSString * const highlightedKey = @"highlighted";
static NSString * const selectedKey = @"selected";
static NSString * const incognitoKey = @"incognito";
static NSString * const pinnedKey = @"pinned";
static NSString * const audibleKey = @"audible";

static NSString * const mutedInfoKey = @"mutedInfo";
static NSString * const mutedKey = @"muted";

static NSString * const statusKey = @"status";
static NSString * const loadingKey = @"loading";
static NSString * const completeKey = @"complete";

static NSString * const isArticleKey = @"isArticle";
static NSString * const isInReaderModeKey = @"isInReaderMode";
static NSString * const openInReaderModeKey = @"openInReaderMode";

static NSString * const currentWindowKey = @"currentWindow";
static NSString * const hiddenKey = @"hidden";
static NSString * const lastFocusedWindowKey = @"lastFocusedWindow";
static NSString * const windowTypeKey = @"windowType";

static NSString * const emptyURLValue = @"";

namespace WebKit {

NSDictionary *toWebAPI(const WebExtensionTabParameters& parameters)
{
    ASSERT(parameters.identifier);

    auto *result = [NSMutableDictionary dictionary];

    result[idKey] = @(toWebAPI(parameters.identifier.value()));

    if (parameters.url)
        result[urlKey] = (NSString *)parameters.url.value().string();
    else
        result[urlKey] = emptyURLValue;

    if (parameters.title)
        result[titleKey] = (NSString *)parameters.title.value();

    if (parameters.windowIdentifier)
        result[windowIdKey] = @(toWebAPI(parameters.windowIdentifier.value()));

    if (parameters.index)
        result[indexKey] = @(parameters.index.value());

    if (parameters.size) {
        auto size = parameters.size.value();
        result[widthKey] = @(size.width);
        result[heightKey] = @(size.height);
    }

    if (parameters.parentTabIdentifier)
        result[openerTabIdKey] = @(toWebAPI(parameters.parentTabIdentifier.value()));

    if (parameters.active)
        result[activeKey] = @(parameters.active.value());

    if (parameters.selected) {
        result[selectedKey] = @(parameters.selected.value());
        result[highlightedKey] = @(parameters.selected.value());
    }

    if (parameters.pinned)
        result[pinnedKey] = @(parameters.pinned.value());

    if (parameters.audible)
        result[audibleKey] = @(parameters.audible.value());

    if (parameters.muted)
        result[mutedInfoKey] = @{ mutedKey: @(parameters.muted.value()) };

    if (parameters.loading)
        result[statusKey] = parameters.loading.value() ? loadingKey : completeKey;

    if (parameters.privateBrowsing)
        result[incognitoKey] = @(parameters.privateBrowsing.value());

    if (parameters.readerModeAvailable)
        result[isArticleKey] = @(parameters.readerModeAvailable.value());

    if (parameters.showingReaderMode)
        result[isInReaderModeKey] = @(parameters.showingReaderMode.value());

    return [result copy];
}

bool WebExtensionAPITabs::parseTabCreateOptions(NSDictionary *options, WebExtensionTabParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
    if (!parseTabUpdateOptions(options, parameters, sourceKey, outExceptionString))
        return false;

    static NSArray<NSString *> *optionalKeys = @[
        indexKey,
        openInReaderModeKey,
        titleKey,
        windowIdKey,
    ];

    static NSDictionary<NSString *, id> *types = @{
        indexKey: NSNumber.class,
        openInReaderModeKey: @YES.class,
        titleKey: NSString.class,
        windowIdKey: NSNumber.class,
    };

    if (!validateDictionary(options, sourceKey, nil, optionalKeys, types, outExceptionString))
        return false;

    if (NSNumber *windowId = objectForKey<NSNumber>(options, windowIdKey)) {
        parameters.windowIdentifier = toWebExtensionWindowIdentifier(windowId.doubleValue);

        if (!parameters.windowIdentifier || !isValid(parameters.windowIdentifier.value())) {
            *outExceptionString = toErrorString(nil, windowIdKey, @"'%@' is not a window identifier", windowId);
            return false;
        }
    }

    if (NSNumber *index = objectForKey<NSNumber>(options, indexKey))
        parameters.index = index.unsignedIntegerValue;

    if (NSNumber *openInReaderMode = objectForKey<NSNumber>(options, openInReaderModeKey))
        parameters.showingReaderMode = openInReaderMode.boolValue;

    if (NSString *title = objectForKey<NSString>(options, titleKey))
        parameters.title = title;

    return true;
}

bool WebExtensionAPITabs::parseTabUpdateOptions(NSDictionary *options, WebExtensionTabParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
    static NSArray<NSString *> *optionalKeys = @[
        activeKey,
        highlightedKey,
        mutedKey,
        openerTabIdKey,
        pinnedKey,
        selectedKey,
        urlKey,
    ];

    static NSDictionary<NSString *, id> *types = @{
        activeKey: @YES.class,
        highlightedKey: @YES.class,
        mutedKey: @YES.class,
        openerTabIdKey: NSNumber.class,
        pinnedKey: @YES.class,
        selectedKey: @YES.class,
        urlKey: NSString.class,
    };

    if (!validateDictionary(options, sourceKey, nil, optionalKeys, types, outExceptionString))
        return false;

    if (NSString *url = objectForKey<NSString>(options, urlKey)) {
        parameters.url = URL { url };

        if (!parameters.url.value().isValid()) {
            *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid URL", url);
            return false;
        }
    }

    if (NSNumber *openerTabId = objectForKey<NSNumber>(options, openerTabIdKey)) {
        parameters.parentTabIdentifier = toWebExtensionTabIdentifier(openerTabId.doubleValue);

        if (!parameters.parentTabIdentifier || !isValid(parameters.parentTabIdentifier.value())) {
            *outExceptionString = toErrorString(nil, openerTabIdKey, @"'%@' is not a tab identifier", openerTabId);
            return false;
        }
    }

    if (NSNumber *active = objectForKey<NSNumber>(options, activeKey))
        parameters.active = active.boolValue;

    if (NSNumber *pinned = objectForKey<NSNumber>(options, pinnedKey))
        parameters.pinned = pinned.boolValue;

    if (NSNumber *muted = objectForKey<NSNumber>(options, mutedKey))
        parameters.muted = muted.boolValue;

    if (NSNumber *selected = objectForKey<NSNumber>(options, selectedKey))
        parameters.selected = selected.boolValue;

    if (NSNumber *highlighted = objectForKey<NSNumber>(options, highlightedKey))
        parameters.selected = highlighted.boolValue;

    return true;
}

bool WebExtensionAPITabs::parseTabQueryOptions(NSDictionary *options, WebExtensionTabQueryParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
    static NSArray<NSString *> *optionalKeys = @[
        activeKey,
        audibleKey,
        currentWindowKey,
        hiddenKey,
        highlightedKey,
        indexKey,
        lastFocusedWindowKey,
        mutedKey,
        pinnedKey,
        selectedKey,
        statusKey,
        titleKey,
        urlKey,
        windowIdKey,
        windowTypeKey,
    ];

    static NSDictionary<NSString *, id> *types = @{
        activeKey: @YES.class,
        audibleKey: @YES.class,
        currentWindowKey: @YES.class,
        hiddenKey: @YES.class,
        highlightedKey: @YES.class,
        indexKey: NSNumber.class,
        lastFocusedWindowKey: @YES.class,
        mutedKey: @YES.class,
        pinnedKey: @YES.class,
        selectedKey: @YES.class,
        statusKey: NSString.class,
        titleKey: NSString.class,
        urlKey: [NSOrderedSet orderedSetWithObjects:NSString.class, @[ NSString.class ], nil],
        windowIdKey: NSNumber.class,
        windowTypeKey: NSString.class,
    };

    if (!validateDictionary(options, sourceKey, nil, optionalKeys, types, outExceptionString))
        return false;

    if (NSNumber *windowId = objectForKey<NSNumber>(options, windowIdKey)) {
        parameters.windowIdentifier = toWebExtensionWindowIdentifier(windowId.doubleValue);

        if (!parameters.windowIdentifier || !isValid(parameters.windowIdentifier.value())) {
            *outExceptionString = toErrorString(nil, windowIdKey, @"'%@' is not a window identifier", windowId);
            return false;
        }
    }

    if (NSString *windowType = objectForKey<NSString>(options, windowTypeKey)) {
        OptionSet<WebExtensionWindow::TypeFilter> windowTypeFilter;
        if (!WebExtensionAPIWindows::parseWindowTypeFilter(windowType, windowTypeFilter, windowTypeKey, outExceptionString))
            return false;

        parameters.windowType = windowTypeFilter;
    }

    if (NSNumber *currentWindow = objectForKey<NSNumber>(options, currentWindowKey))
        parameters.currentWindow = currentWindow.boolValue;

    if (NSNumber *lastFocusedWindow = objectForKey<NSNumber>(options, lastFocusedWindowKey))
        parameters.frontmostWindow = lastFocusedWindow.boolValue;

    if (NSString *url = objectForKey<NSString>(options, urlKey, true))
        parameters.urlPatterns = { url };
    else if (NSArray *urls = objectForKey<NSArray>(options, urlKey, true))
        parameters.urlPatterns = makeVector<String>(urls);

    if (NSString *title = objectForKey<NSString>(options, titleKey, true))
        parameters.titlePattern = title;

    if (NSNumber *audible = objectForKey<NSNumber>(options, audibleKey))
        parameters.audible = audible.boolValue;

    if (NSNumber *hidden = objectForKey<NSNumber>(options, hiddenKey))
        parameters.hidden = hidden.boolValue;

    if (NSNumber *index = objectForKey<NSNumber>(options, indexKey))
        parameters.index = index.unsignedIntegerValue;

    if (NSNumber *active = objectForKey<NSNumber>(options, activeKey))
        parameters.active = active.boolValue;

    if (NSNumber *pinned = objectForKey<NSNumber>(options, pinnedKey))
        parameters.pinned = pinned.boolValue;

    if (NSNumber *selected = objectForKey<NSNumber>(options, selectedKey))
        parameters.selected = selected.boolValue;

    if (NSNumber *highlighted = objectForKey<NSNumber>(options, highlightedKey))
        parameters.selected = highlighted.boolValue;

    if (NSNumber *muted = objectForKey<NSNumber>(options, mutedKey))
        parameters.muted = muted.boolValue;

    return true;
}

static bool isValid(std::optional<WebExtensionTabIdentifier> identifier, NSString **outExceptionString)
{
    if (UNLIKELY(!isValid(identifier))) {
        if (isNone(identifier))
            *outExceptionString = toErrorString(nil, @"tabID", @"'tabs.TAB_ID_NONE' is not allowed");
        else if (identifier)
            *outExceptionString = toErrorString(nil, @"tabID", @"'%llu' is not a tab identifier", identifier.value().toUInt64());
        else
            *outExceptionString = toErrorString(nil, @"tabID", @"it is not a tab identifier");
        return false;
    }

    return true;
}

bool WebExtensionAPITabs::isPropertyAllowed(String name, WebPage*)
{
    // FIXME: <https://webkit.org/b/260994> Implement.

    return true;
}

void WebExtensionAPITabs::createTab(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/create

    WebExtensionTabParameters parameters;
    if (!parseTabCreateOptions(options, parameters, @"properties", outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::query(NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/query

    WebExtensionTabQueryParameters parameters;
    if (!parseTabQueryOptions(options, parameters, @"info", outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::get(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/get

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::getCurrent(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getCurrent

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::getSelected(double windowID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getSelected

    auto windowIdentifer = toWebExtensionWindowIdentifier(windowID);
    if (!isValid(windowIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::duplicate(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/duplicate

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::update(double tabID, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/update

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    WebExtensionTabParameters parameters;
    if (!parseTabUpdateOptions(options, parameters, @"properties", outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::remove(NSObject *tabIDs, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/remove

    if (!validateObject(tabIDs, @"tabIDs", [NSOrderedSet orderedSetWithObjects:NSNumber.class, @[ NSNumber.class ], nil], outExceptionString))
        return;

    Vector<WebExtensionTabIdentifier> identifiers;

    if (NSNumber *tabID = dynamic_objc_cast<NSNumber>(tabIDs)) {
        auto tabIdentifer = toWebExtensionTabIdentifier(tabID.doubleValue);
        if (!isValid(tabIdentifer)) {
            *outExceptionString = toErrorString(nil, @"tabIDs", @"'%@' is not a tab identifier", tabID);
            return;
        }

        identifiers.append(tabIdentifer.value());
    } else if (NSArray *tabIDArray = dynamic_objc_cast<NSArray>(tabIDs)) {
        identifiers.reserveInitialCapacity(tabIDArray.count);

        for (NSNumber *tabID in tabIDArray) {
            auto tabIdentifer = toWebExtensionTabIdentifier(tabID.doubleValue);
            if (!isValid(tabIdentifer)) {
                *outExceptionString = toErrorString(nil, @"tabIDs", @"'%@' is not a tab identifier", tabID);
                return;
            }

            identifiers.uncheckedAppend(tabIdentifer.value());
        }
    }

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::reload(double tabID, NSDictionary *reloadProperties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/reload

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::goBack(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/goBack

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::goForward(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/goForward

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::getZoom(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getZoom

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::setZoom(double tabID, double zoomFactor, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/setZoom

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::detectLanguage(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/detectLanguage

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

void WebExtensionAPITabs::toggleReaderMode(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/toggleReaderMode

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/260994> Implement.
}

WebExtensionAPIEvent& WebExtensionAPITabs::onActivated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onActivated

    if (!m_onActivated)
        m_onActivated = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnActivated);

    return *m_onActivated;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onAttached()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onAttached

    if (!m_onAttached)
        m_onAttached = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnAttached);

    return *m_onAttached;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onCreated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onCreated

    if (!m_onCreated)
        m_onCreated = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnCreated);

    return *m_onCreated;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onDetached()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onDetached

    if (!m_onDetached)
        m_onDetached = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnDetached);

    return *m_onDetached;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onHighlighted()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onHighlighted

    if (!m_onHighlighted)
        m_onHighlighted = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnHighlighted);

    return *m_onHighlighted;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onMoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onMoved

    if (!m_onMoved)
        m_onMoved = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnMoved);

    return *m_onMoved;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onRemoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onRemoved

    if (!m_onRemoved)
        m_onRemoved = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnRemoved);

    return *m_onRemoved;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onReplaced()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onReplaced

    if (!m_onReplaced)
        m_onReplaced = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnReplaced);

    return *m_onReplaced;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onUpdated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onUpdated

    if (!m_onUpdated)
        m_onUpdated = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::TabsOnUpdated);

    return *m_onUpdated;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
