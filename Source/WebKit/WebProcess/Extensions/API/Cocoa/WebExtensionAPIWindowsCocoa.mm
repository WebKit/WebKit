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
#import "WebExtensionAPIWindows.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPITabs.h"
#import "WebExtensionAPIWindowsEvent.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "WebProcess.h"

static NSString * const populateKey = @"populate";

static NSString * const windowTypesKey = @"windowTypes";
static NSString * const normalKey = @"normal";
static NSString * const popupKey = @"popup";

static NSString * const minimizedKey = @"minimized";
static NSString * const maximizedKey = @"maximized";
static NSString * const fullscreenKey = @"fullscreen";

static NSString * const focusedKey = @"focused";
static NSString * const incognitoKey = @"incognito";
static NSString * const stateKey = @"state";
static NSString * const typeKey = @"type";

static NSString * const topKey = @"top";
static NSString * const leftKey = @"left";
static NSString * const widthKey = @"width";
static NSString * const heightKey = @"height";

static NSString * const tabsKey = @"tabs";

static NSString * const idKey = @"id";
static NSString * const alwaysOnTopKey = @"alwaysOnTop";

static NSString * const urlKey = @"url";
static NSString * const tabIdKey = @"tabId";

namespace WebKit {

static inline NSString *toWebAPI(WebExtensionWindow::State state)
{
    switch (state) {
    case WebExtensionWindow::State::Normal:
        return normalKey;

    case WebExtensionWindow::State::Minimized:
        return minimizedKey;

    case WebExtensionWindow::State::Maximized:
        return maximizedKey;

    case WebExtensionWindow::State::Fullscreen:
        return fullscreenKey;
    }
}

static inline NSString *toWebAPI(WebExtensionWindow::Type type)
{
    switch (type) {
    case WebExtensionWindow::Type::Normal:
        return normalKey;

    case WebExtensionWindow::Type::Popup:
        return popupKey;
    }
}

static NSDictionary *toWebAPI(const WebExtensionWindowParameters& parameters)
{
    ASSERT(parameters.identifier);
    ASSERT(parameters.state);
    ASSERT(parameters.type);
    ASSERT(parameters.focused);
    ASSERT(parameters.privateBrowsing);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/Window

    NSMutableDictionary *result = [@{
        idKey: @(toWebAPI(parameters.identifier.value())),
        stateKey: toWebAPI(parameters.state.value()),
        typeKey: toWebAPI(parameters.type.value()),
        focusedKey: @(parameters.focused.value()),
        incognitoKey: @(parameters.privateBrowsing.value()),
        alwaysOnTopKey: @NO
    } mutableCopy];

    if (parameters.frame) {
        CGRect frame = parameters.frame.value();
        [result addEntriesFromDictionary:@{
            topKey: @(frame.origin.y),
            leftKey: @(frame.origin.x),
            widthKey: @(frame.size.width),
            heightKey: @(frame.size.height),
        }];
    }

    if (parameters.tabs) {
        NSMutableArray *tabs = [[NSMutableArray alloc] initWithCapacity:parameters.tabs.value().size()];

        for (auto& tab : parameters.tabs.value())
            [tabs addObject:toWebAPI(tab)];

        result[tabsKey] = [tabs copy];
    }

    return [result copy];
}

static inline std::optional<WebExtensionWindow::Type> toTypeImpl(NSString *type)
{
    if (!type)
        return std::nullopt;

    if ([type isEqualToString:normalKey])
        return WebExtensionWindow::Type::Normal;

    if ([type isEqualToString:popupKey])
        return WebExtensionWindow::Type::Popup;

    RELEASE_LOG_ERROR(Extensions, "Unknown window type: %{private}@", type);
    return std::nullopt;
}

static inline std::optional<WebExtensionWindow::State> toStateImpl(NSString *state)
{
    if (!state)
        return std::nullopt;

    if ([state isEqualToString:normalKey])
        return WebExtensionWindow::State::Normal;

    if ([state isEqualToString:minimizedKey])
        return WebExtensionWindow::State::Minimized;

    if ([state isEqualToString:maximizedKey])
        return WebExtensionWindow::State::Maximized;

    if ([state isEqualToString:fullscreenKey])
        return WebExtensionWindow::State::Fullscreen;

    RELEASE_LOG_DEBUG(Extensions, "Unknown window state: %{private}@", state);
    return std::nullopt;
}

bool WebExtensionAPIWindows::parsePopulateTabs(NSDictionary *options, PopulateTabs& populate, NSString *sourceKey, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        populateKey: @YES.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    populate = objectForKey<NSNumber>(options, populateKey).boolValue ? PopulateTabs::Yes : PopulateTabs::No;

    return true;
}

bool WebExtensionAPIWindows::parseWindowTypesFilter(NSDictionary *options, OptionSet<WindowTypeFilter>& windowTypeFilter, NSString *sourceKey, NSString **outExceptionString)
{
    // All windows match by default.
    windowTypeFilter = allWebExtensionWindowTypeFilters();

    static NSDictionary<NSString *, id> *types = @{
        windowTypesKey: @[ NSString.class ],
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    NSArray<NSString *> *windowTypes = objectForKey<NSArray>(options, windowTypesKey, false, NSString.class);
    if (!windowTypes)
        return true;

    if (![windowTypes containsObject:normalKey])
        windowTypeFilter.remove(WindowTypeFilter::Normal);

    if (![windowTypes containsObject:popupKey])
        windowTypeFilter.remove(WindowTypeFilter::Popup);

    if (!windowTypeFilter) {
        *outExceptionString = toErrorString(nil, windowTypesKey, @"it must specify either 'normal', 'popup', or both.");
        return false;
    }

    return true;
}

bool WebExtensionAPIWindows::parseWindowTypeFilter(NSString *windowType, OptionSet<WindowTypeFilter>& windowTypeFilter, NSString *sourceKey, NSString **outExceptionString)
{
    if ([windowType isEqualToString:normalKey])
        windowTypeFilter.add(WindowTypeFilter::Normal);
    else if ([windowType isEqualToString:popupKey])
        windowTypeFilter.add(WindowTypeFilter::Popup);

    if (!windowTypeFilter) {
        *outExceptionString = toErrorString(nil, sourceKey, @"it must specify either 'normal' or 'popup'");
        return false;
    }

    return true;
}

bool WebExtensionAPIWindows::parseWindowGetOptions(NSDictionary *options, PopulateTabs& populate, OptionSet<WindowTypeFilter>& filter, NSString *sourceKey, NSString **outExceptionString)
{
    if (!parsePopulateTabs(options, populate, sourceKey, outExceptionString))
        return false;

    if (!parseWindowTypesFilter(options, filter, sourceKey, outExceptionString))
        return false;

    return true;
}

bool WebExtensionAPIWindows::parseWindowCreateOptions(NSDictionary *options, WebExtensionWindowParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
    if (!parseWindowUpdateOptions(options, parameters, sourceKey, outExceptionString))
        return false;

    static NSDictionary<NSString *, id> *types = @{
        typeKey: NSString.class,
        incognitoKey: @YES.class,
        urlKey: [NSOrderedSet orderedSetWithObjects:NSString.class, @[ NSString.class ], nil],
        tabIdKey: NSNumber.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSString *type = objectForKey<NSString>(options, typeKey))
        parameters.type = toTypeImpl(type);

    if (NSNumber *incognito = objectForKey<NSNumber>(options, incognitoKey))
        parameters.privateBrowsing = incognito.boolValue;

    if (NSString *url = objectForKey<NSString>(options, urlKey, true)) {
        WebExtensionTabParameters tabParameters;
        tabParameters.url = URL { extensionContext().baseURL(), url };

        if (!tabParameters.url.value().isValid()) {
            *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid URL", url);
            return false;
        }

        parameters.tabs = { WTFMove(tabParameters) };
    } else if (NSArray *urls = objectForKey<NSArray>(options, urlKey, true)) {
        Vector<WebExtensionTabParameters> tabs;
        tabs.reserveInitialCapacity(urls.count);

        for (NSString *url in urls) {
            WebExtensionTabParameters tabParameters;
            tabParameters.url = URL { extensionContext().baseURL(), url };

            if (!tabParameters.url.value().isValid()) {
                *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid URL", url);
                return false;
            }

            tabs.append(WTFMove(tabParameters));
        }

        parameters.tabs = WTFMove(tabs);
    }

    if (auto tabIdentifier = toWebExtensionTabIdentifier(objectForKey<NSNumber>(options, tabIdKey).doubleValue); tabIdentifier && !isNone(tabIdentifier)) {
        if (!parameters.tabs)
            parameters.tabs = Vector<WebExtensionTabParameters> { };

        WebExtensionTabParameters tabParameters;
        tabParameters.identifier = tabIdentifier;
        parameters.tabs.value().append(WTFMove(tabParameters));
    }

    return true;
}

bool WebExtensionAPIWindows::parseWindowUpdateOptions(NSDictionary *options, WebExtensionWindowParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        stateKey: NSString.class,
        focusedKey: @YES.class,
        topKey: NSNumber.class,
        leftKey: NSNumber.class,
        widthKey: NSNumber.class,
        heightKey: NSNumber.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSString *state = objectForKey<NSString>(options, stateKey, true)) {
        parameters.state = toStateImpl(state);

        if (!parameters.state) {
            *outExceptionString = toErrorString(nil, stateKey, @"it must specify 'normal', 'minimized', 'maximized', or 'fullscreen'");
            return false;
        }
    }

    if (NSNumber *focused = objectForKey<NSNumber>(options, focusedKey))
        parameters.focused = focused.boolValue;

    NSNumber *left = objectForKey<NSNumber>(options, leftKey);
    NSNumber *top = objectForKey<NSNumber>(options, topKey);
    NSNumber *width = objectForKey<NSNumber>(options, widthKey);
    NSNumber *height = objectForKey<NSNumber>(options, heightKey);

    if (left || top || width || height) {
        if (parameters.state && parameters.state.value() != WebExtensionWindow::State::Normal) {
            *outExceptionString = toErrorString(nil, sourceKey, @"when 'top', 'left', 'width', or 'height' are specified, 'state' must specify 'normal'.");
            return false;
        }

        static constexpr CGFloat NaN = std::numeric_limits<CGFloat>::quiet_NaN();

        CGRect frame;
        frame.origin.x = left ? left.doubleValue : NaN;
        frame.origin.y = top ? top.doubleValue : NaN;
        frame.size.width = width ? width.doubleValue : NaN;
        frame.size.height = height ? height.doubleValue : NaN;
        parameters.frame = frame;
    }

    return true;
}

bool isValid(std::optional<WebExtensionWindowIdentifier> identifier, NSString **outExceptionString)
{
    if (UNLIKELY(!isValid(identifier))) {
        if (isNone(identifier))
            *outExceptionString = toErrorString(nil, @"windowId", @"'windows.WINDOW_ID_NONE' is not allowed");
        else if (identifier)
            *outExceptionString = toErrorString(nil, @"windowId", @"'%llu' is not a window identifier", identifier.value().toUInt64());
        else
            *outExceptionString = toErrorString(nil, @"windowId", @"it is not a window identifier");
        return false;
    }

    return true;
}

bool WebExtensionAPIWindows::isPropertyAllowed(const ASCIILiteral& name, WebPage*)
{
    if (UNLIKELY(extensionContext().isUnsupportedAPI(propertyPath(), name)))
        return false;

#if PLATFORM(MAC)
    return true;
#else
    // Opening, closing, and updating a window in not supported on iOS, iPadOS, or visionOS.
    static NeverDestroyed<HashSet<AtomString>> unsupported { HashSet { AtomString("create"_s), AtomString("remove"_s), AtomString("update"_s) } };
    return !unsupported.get().contains(name);
#endif
}

void WebExtensionAPIWindows::createWindow(NSDictionary *data, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/create

    WebExtensionWindowParameters parameters;
    if (!parseWindowCreateOptions(data, parameters, @"info", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WindowsCreate(WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionWindowParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIWindows::get(WebPage& page, double windowID, NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/get

    auto windowIdentifer = toWebExtensionWindowIdentifier(windowID);
    if (!isValid(windowIdentifer, outExceptionString))
        return;

    PopulateTabs populate;
    OptionSet<WindowTypeFilter> filter;
    if (!parseWindowGetOptions(info, populate, filter, @"info", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WindowsGet(page.webPageProxyIdentifier(), windowIdentifer.value(), filter, populate), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<WebExtensionWindowParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIWindows::getCurrent(WebPage& page, NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/getCurrent

    PopulateTabs populate;
    OptionSet<WindowTypeFilter> filter;
    if (!parseWindowGetOptions(info, populate, filter, @"info", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WindowsGet(page.webPageProxyIdentifier(), WebExtensionWindowConstants::CurrentIdentifier, filter, populate), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<WebExtensionWindowParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIWindows::getLastFocused(NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/getLastFocused

    PopulateTabs populate;
    OptionSet<WindowTypeFilter> filter;
    if (!parseWindowGetOptions(info, populate, filter, @"info", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WindowsGetLastFocused(filter, populate), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<WebExtensionWindowParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIWindows::getAll(NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/getAll

    PopulateTabs populate;
    OptionSet<WindowTypeFilter> filter;
    if (!parseWindowGetOptions(info, populate, filter, @"info", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WindowsGetAll(filter, populate), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebExtensionWindowParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIWindows::update(double windowID, NSDictionary *info, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/update

    auto windowIdentifer = toWebExtensionWindowIdentifier(windowID);
    if (!isValid(windowIdentifer, outExceptionString))
        return;

    WebExtensionWindowParameters parameters;
    if (!parseWindowUpdateOptions(info, parameters, @"properties", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WindowsUpdate(windowIdentifer.value(), WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<WebExtensionWindowParameters, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPIWindows::remove(double windowID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/remove

    auto windowIdentifer = toWebExtensionWindowIdentifier(windowID);
    if (!isValid(windowIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::WindowsRemove(windowIdentifer.value()), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

WebExtensionAPIWindowsEvent& WebExtensionAPIWindows::onCreated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onCreated

    if (!m_onCreated)
        m_onCreated = WebExtensionAPIWindowsEvent::create(*this, WebExtensionEventListenerType::WindowsOnCreated);

    return *m_onCreated;
}

WebExtensionAPIWindowsEvent& WebExtensionAPIWindows::onRemoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onRemoved

    if (!m_onRemoved)
        m_onRemoved = WebExtensionAPIWindowsEvent::create(*this, WebExtensionEventListenerType::WindowsOnRemoved);

    return *m_onRemoved;
}

WebExtensionAPIWindowsEvent& WebExtensionAPIWindows::onFocusChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onFocusChanged

    if (!m_onFocusChanged)
        m_onFocusChanged = WebExtensionAPIWindowsEvent::create(*this, WebExtensionEventListenerType::WindowsOnFocusChanged);

    return *m_onFocusChanged;
}

inline OptionSet<WebExtensionWindowTypeFilter> toWindowTypeFilter(WebExtensionWindow::Type type)
{
    switch (type) {
    case WebExtensionWindow::Type::Normal:
        return WebExtensionWindowTypeFilter::Normal;

    case WebExtensionWindow::Type::Popup:
        return WebExtensionWindowTypeFilter::Popup;
    }
}

void WebExtensionContextProxy::dispatchWindowsEvent(WebExtensionEventListenerType type, const std::optional<WebExtensionWindowParameters>& windowParameters)
{
    auto filter = windowParameters ? toWindowTypeFilter(windowParameters.value().type.value()) : allWebExtensionWindowTypeFilters();

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        auto& windowsObject = namespaceObject.windows();

        switch (type) {
        case WebExtensionEventListenerType::WindowsOnCreated:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onCreated
            ASSERT(windowParameters);
            windowsObject.onCreated().invokeListenersWithArgument(toWebAPI(windowParameters), filter);
            break;

        case WebExtensionEventListenerType::WindowsOnFocusChanged:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onFocusChanged
            ASSERT(!windowParameters || windowParameters.value().identifier);
            windowsObject.onFocusChanged().invokeListenersWithArgument(@(toWebAPI(windowParameters ? windowParameters.value().identifier.value() : WebExtensionWindowConstants::NoneIdentifier)), filter);
            break;

        case WebExtensionEventListenerType::WindowsOnRemoved:
            // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows/onRemoved
            ASSERT(windowParameters && windowParameters.value().identifier);
            windowsObject.onRemoved().invokeListenersWithArgument(@(toWebAPI(windowParameters.value().identifier.value())), filter);
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
