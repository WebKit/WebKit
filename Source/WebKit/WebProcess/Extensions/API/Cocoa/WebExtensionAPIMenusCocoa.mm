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
#import "WebExtensionAPIMenus.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPIAction.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPITabs.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionMenuItemContextParameters.h"
#import "WebExtensionMenuItemParameters.h"
#import "WebExtensionTabParameters.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/cocoa/VectorCocoa.h>

static NSString * const checkedKey = @"checked";
static NSString * const contextsKey = @"contexts";
static NSString * const commandKey = @"command";
static NSString * const documentURLPatternsKey = @"documentUrlPatterns";
static NSString * const enabledKey = @"enabled";
static NSString * const iconsKey = @"icons";
static NSString * const idKey = @"id";
static NSString * const onclickKey = @"onclick";
static NSString * const parentIdKey = @"parentId";
static NSString * const targetURLPatternsKey = @"targetUrlPatterns";
static NSString * const titleKey = @"title";
static NSString * const typeKey = @"type";
static NSString * const visibleKey = @"visible";

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static NSString * const iconVariantsKey = @"icon_variants";
#endif

static NSString * const normalKey = @"normal";
static NSString * const checkboxKey = @"checkbox";
static NSString * const radioKey = @"radio";
static NSString * const separatorKey = @"separator";

static NSString * const allKey = @"all";

static NSString * const editableKey = @"editable";
static NSString * const frameIDKey = @"frameId";
static NSString * const frameURLKey = @"frameUrl";
static NSString * const linkTextKey = @"linkText";
static NSString * const linkURLKey = @"linkUrl";
static NSString * const mediaTypeKey = @"mediaType";
static NSString * const menuItemIDKey = @"menuItemId";
static NSString * const pageURLKey = @"pageUrl";
static NSString * const parentMenuItemIDKey = @"parentMenuItemId";
static NSString * const selectionTextKey = @"selectionText";
static NSString * const srcURLKey = @"srcUrl";
static NSString * const wasCheckedKey = @"wasChecked";

static NSString * const imageKey = @"image";
static NSString * const audioKey = @"audio";
static NSString * const videoKey = @"video";

namespace WebKit {

static id toMenuIdentifierWebAPI(const String& identifier)
{
    bool validNumber;
    double number = identifier.toDouble(&validNumber);
    if (validNumber)
        return @(number);
    return identifier;
}

bool WebExtensionAPIMenus::parseCreateAndUpdateProperties(ForUpdate forUpdate, NSDictionary *properties, const URL& baseURL, std::optional<WebExtensionMenuItemParameters>& outParameters, RefPtr<WebExtensionCallbackHandler>& outClickCallback, NSString **outExceptionString)
{
    static NSArray<NSString *> *requiredKeys = @[
        titleKey,
    ];

    static NSDictionary<NSString *, id> *types = @{
        checkedKey: @YES.class,
        commandKey: NSString.class,
        contextsKey: @[ NSString.class ],
        documentURLPatternsKey: @[ NSString.class ],
        enabledKey: @YES.class,
        iconsKey: [NSOrderedSet orderedSetWithObjects:NSString.class, NSDictionary.class, NSNull.class, nil],
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        iconVariantsKey: [NSOrderedSet orderedSetWithObjects:@[ NSDictionary.class ], NSNull.class, nil],
#endif
        idKey: [NSOrderedSet orderedSetWithObjects:NSString.class, NSNumber.class, nil],
        onclickKey: JSValue.class,
        parentIdKey: [NSOrderedSet orderedSetWithObjects:NSString.class, NSNumber.class, nil],
        targetURLPatternsKey: @[ NSString.class ],
        titleKey: NSString.class,
        typeKey: NSString.class,
        visibleKey: @YES.class,
    };

    bool isSeparator = [objectForKey<NSString>(properties, typeKey) isEqualToString:separatorKey];
    if (!validateDictionary(properties, @"properties", isSeparator || forUpdate == ForUpdate::Yes ? nil : requiredKeys, types, outExceptionString))
        return false;

    WebExtensionMenuItemParameters parameters;

    if (NSString *type = properties[typeKey]) {
        if ([type isEqualToString:normalKey])
            parameters.type = WebExtensionMenuItemType::Normal;
        else if ([type isEqualToString:checkboxKey])
            parameters.type = WebExtensionMenuItemType::Checkbox;
        else if ([type isEqualToString:radioKey])
            parameters.type = WebExtensionMenuItemType::Radio;
        else if ([type isEqualToString:separatorKey])
            parameters.type = WebExtensionMenuItemType::Separator;
        else {
            *outExceptionString = toErrorString(nil, typeKey, @"it must specify either 'normal', 'checkbox', 'radio', or 'separator'");
            return false;
        }
    }

    if (NSArray *contexts = properties[contextsKey]) {
        static NeverDestroyed<HashMap<String, WebExtensionMenuItemContextType>> typeMap = HashMap<String, WebExtensionMenuItemContextType> {
            { "action"_s, WebExtensionMenuItemContextType::Action },
            { "audio"_s, WebExtensionMenuItemContextType::Audio },
            { "browser_action"_s, WebExtensionMenuItemContextType::Action },
            { "editable"_s, WebExtensionMenuItemContextType::Editable },
            { "frame"_s, WebExtensionMenuItemContextType::Frame },
            { "image"_s, WebExtensionMenuItemContextType::Image },
            { "link"_s, WebExtensionMenuItemContextType::Link },
            { "page"_s, WebExtensionMenuItemContextType::Page },
            { "page_action"_s, WebExtensionMenuItemContextType::Action },
            { "selection"_s, WebExtensionMenuItemContextType::Selection },
            { "tab"_s, WebExtensionMenuItemContextType::Tab },
            { "video"_s, WebExtensionMenuItemContextType::Video },
        };

        parameters.contexts = OptionSet<WebExtensionMenuItemContextType> { };

        for (NSString *context in contexts) {
            if ([context isEqualToString:allKey]) {
                parameters.contexts = allWebExtensionMenuItemContextTypes();
                break;
            }

            if (!typeMap.get().contains(context)) {
                // Don't error on unknown contexts, to allow different values that we don't support from other browsers.
                continue;
            }

            if ([context isEqualToString:@"action"] && !extensionContext().supportsManifestVersion(3)) {
                *outExceptionString = toErrorString(nil, contextsKey, @"'%@' is not a valid context", context);
                return false;
            }

            if (([context isEqualToString:@"browser_action"] || [context isEqualToString:@"page_action"]) && extensionContext().supportsManifestVersion(3)) {
                *outExceptionString = toErrorString(nil, contextsKey, @"'%@' is not a valid context", context);
                return false;
            }

            parameters.contexts.value().add(typeMap.get().get(context));
        }
    }

    if (NSArray *documentPatterns = properties[documentURLPatternsKey]) {
        for (NSString *patternString in documentPatterns) {
            auto pattern = WebExtensionMatchPattern::getOrCreate(patternString);
            if (!pattern || !pattern->isSupported()) {
                *outExceptionString = toErrorString(nil, documentURLPatternsKey, @"'%@' is not a valid pattern", patternString);
                return false;
            }
        }

        parameters.documentURLPatterns = makeVector<String>(documentPatterns);
    }

    if (NSArray *targetPatterns = properties[targetURLPatternsKey]) {
        for (NSString *patternString in targetPatterns) {
            auto pattern = WebExtensionMatchPattern::getOrCreate(patternString);

            // Any valid pattern is allowed, not just supported schemes.
            if (!pattern || !pattern->isValid()) {
                *outExceptionString = toErrorString(nil, targetURLPatternsKey, @"'%@' is not a valid pattern", patternString);
                return false;
            }
        }

        parameters.targetURLPatterns = makeVector<String>(targetPatterns);
    }

    if (NSString *identifier = objectForKey<NSString>(properties, idKey, false)) {
        if (!identifier.length) {
            *outExceptionString = toErrorString(nil, idKey, @"it must not be empty");
            return false;
        }

        parameters.identifier = identifier;
    } else if (NSNumber *identifier = objectForKey<NSNumber>(properties, idKey))
        parameters.identifier = identifier.stringValue;

    if (NSString *parentIdentifier = objectForKey<NSString>(properties, parentIdKey, false)) {
        if (!parentIdentifier.length) {
            *outExceptionString = toErrorString(nil, parentIdKey, @"it must not be empty");
            return false;
        }

        parameters.parentIdentifier = parentIdentifier;
    } else if (NSNumber *parentIdentifier = objectForKey<NSNumber>(properties, parentIdKey))
        parameters.parentIdentifier = parentIdentifier.stringValue;

    if (NSString *title = properties[titleKey]) {
        if (!title.length && parameters.type != WebExtensionMenuItemType::Separator) {
            *outExceptionString = toErrorString(nil, titleKey, @"it must not be empty");
            return false;
        }

        parameters.title = title;
    }

    if (JSValue *clickCallback = properties[onclickKey]) {
        if (!clickCallback._isFunction) {
            *outExceptionString = toErrorString(nil, onclickKey, @"it must be a function");
            return false;
        }

        outClickCallback = WebExtensionCallbackHandler::create(clickCallback);
    }

    NSDictionary *iconDictionary;

    if (auto *iconPath = objectForKey<NSString>(properties, iconsKey))
        iconDictionary = @{ @"16": WebExtensionAPIAction::parseIconPath(iconPath, baseURL) };

    if (auto *iconPaths = objectForKey<NSDictionary>(properties, iconsKey)) {
        iconDictionary = WebExtensionAPIAction::parseIconPathsDictionary(iconPaths, baseURL, false, iconsKey, outExceptionString);
        if (!iconDictionary)
            return false;
    }

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    NSArray *iconVariants;
    if (auto *variants = objectForKey<NSArray>(properties, iconVariantsKey, false)) {
        iconVariants = WebExtensionAPIAction::parseIconVariants(variants, baseURL, iconVariantsKey, outExceptionString);
        if (!iconVariants)
            return false;
    }

    // Icon variants takes precedence over the old icons key, even if empty.
    if (iconVariants || iconDictionary.count)
        parameters.iconsJSON = encodeJSONString(iconVariants ?: iconDictionary, JSONOptions::FragmentsAllowed);
#else
    if (iconDictionary.count)
        parameters.iconsJSON = encodeJSONString(iconDictionary);
#endif

    // An explicit null icon variants or icons will clear the current icon.
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (properties[iconVariantsKey] && objectForKey<NSNull>(properties, iconVariantsKey))
        parameters.iconsJSON = emptyString();
    else
#endif
    if (properties[iconsKey] && objectForKey<NSNull>(properties, iconsKey))
        parameters.iconsJSON = emptyString();

    if (NSString *command = properties[commandKey]) {
        if (!command.length) {
            *outExceptionString = toErrorString(nil, commandKey, @"it must not be empty");
            return false;
        }

        parameters.command = command;
    }

    if (NSNumber *checked = properties[checkedKey])
        parameters.checked = checked.boolValue;

    if (NSNumber *enabled = properties[enabledKey])
        parameters.enabled = enabled.boolValue;

    if (NSNumber *visible = properties[visibleKey])
        parameters.visible = visible.boolValue;

    outParameters = parameters;

    return true;
}

id WebExtensionAPIMenus::createMenu(WebPage& page, WebFrame& frame, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/menus/create

    m_pageProxyIdentifier = page.webPageProxyIdentifier();

    std::optional<WebExtensionMenuItemParameters> parameters;
    RefPtr<WebExtensionCallbackHandler> clickCallback;
    if (!parseCreateAndUpdateProperties(ForUpdate::No, properties, frame.url(), parameters, clickCallback, outExceptionString))
        return nil;

    if (parameters.value().identifier.isEmpty())
        parameters.value().identifier = createVersion4UUIDString();

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::MenusCreate(parameters.value()), [this, protectedThis = Ref { *this }, callback = WTFMove(callback), clickCallback = WTFMove(clickCallback), identifier = parameters.value().identifier](Expected<void, WebExtensionError>&& result) mutable {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        if (clickCallback) {
            if (m_clickHandlerMap.isEmpty())
                WebProcess::singleton().send(Messages::WebExtensionContext::AddListener(*m_pageProxyIdentifier, WebExtensionEventListenerType::MenusOnClicked, contentWorldType()), extensionContext().identifier());

            m_clickHandlerMap.set(identifier, clickCallback.releaseNonNull());
        }

        callback->call();
    }, extensionContext().identifier());

    return toMenuIdentifierWebAPI(parameters.value().identifier);
}

void WebExtensionAPIMenus::update(WebPage& page, WebFrame& frame, id identifier, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/menus/update

    m_pageProxyIdentifier = page.webPageProxyIdentifier();

    if (!validateObject(identifier, @"identifier", [NSOrderedSet orderedSetWithObjects:NSString.class, NSNumber.class, nil], outExceptionString))
        return;

    std::optional<WebExtensionMenuItemParameters> parameters;
    RefPtr<WebExtensionCallbackHandler> clickCallback;
    if (!parseCreateAndUpdateProperties(ForUpdate::Yes, properties, frame.url(), parameters, clickCallback, outExceptionString))
        return;

    if (NSNumber *identifierNumber = dynamic_objc_cast<NSNumber>(identifier))
        identifier = identifierNumber.stringValue;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::MenusUpdate(identifier, parameters.value()), [this, protectedThis = Ref { *this }, callback = WTFMove(callback), clickCallback = WTFMove(clickCallback), newIdentifier = parameters.value().identifier, oldIdentifier = String(identifier)](Expected<void, WebExtensionError>&& result) mutable {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        if (newIdentifier.isEmpty())
            newIdentifier = oldIdentifier;

        if (oldIdentifier != newIdentifier || clickCallback) {
            if (clickCallback)
                m_clickHandlerMap.remove(oldIdentifier);
            else
                clickCallback = m_clickHandlerMap.take(oldIdentifier);

            if (clickCallback) {
                if (m_clickHandlerMap.isEmpty())
                    WebProcess::singleton().send(Messages::WebExtensionContext::AddListener(*m_pageProxyIdentifier, WebExtensionEventListenerType::MenusOnClicked, contentWorldType()), extensionContext().identifier());

                m_clickHandlerMap.set(newIdentifier, clickCallback.releaseNonNull());
            }
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIMenus::remove(id identifier, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/menus/remove

    if (!validateObject(identifier, @"identifier", [NSOrderedSet orderedSetWithObjects:NSString.class, NSNumber.class, nil], outExceptionString))
        return;

    if (NSNumber *identifierNumber = dynamic_objc_cast<NSNumber>(identifier))
        identifier = identifierNumber.stringValue;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::MenusRemove(identifier), [this, protectedThis = Ref { *this }, callback = WTFMove(callback), identifier = String(identifier)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        m_clickHandlerMap.remove(identifier);

        if (m_clickHandlerMap.isEmpty())
            WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(*m_pageProxyIdentifier, WebExtensionEventListenerType::MenusOnClicked, contentWorldType(), 1), extensionContext().identifier());

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIMenus::removeAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/menus/removeAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::MenusRemoveAll(), [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        if (!m_clickHandlerMap.isEmpty()) {
            m_clickHandlerMap.clear();

            WebProcess::singleton().send(Messages::WebExtensionContext::RemoveListener(*m_pageProxyIdentifier, WebExtensionEventListenerType::MenusOnClicked, contentWorldType(), 1), extensionContext().identifier());
        }

        callback->call();
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPIMenus::onClicked()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/menus/onClicked

    if (!m_onClicked)
        m_onClicked = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::MenusOnClicked);

    return *m_onClicked;
}

void WebExtensionContextProxy::dispatchMenusClickedEvent(const WebExtensionMenuItemParameters& menuItemParameters, bool wasChecked, const WebExtensionMenuItemContextParameters& contextParameters, const std::optional<WebExtensionTabParameters>& tabParameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/menus/onClicked

    ASSERT(menuItemParameters.type);

    auto *info = [NSMutableDictionary dictionary];

    info[menuItemIDKey] = toMenuIdentifierWebAPI(menuItemParameters.identifier);

    if (menuItemParameters.parentIdentifier)
        info[parentMenuItemIDKey] = toMenuIdentifierWebAPI(menuItemParameters.parentIdentifier.value());

    if (isCheckedType(menuItemParameters.type.value())) {
        info[checkedKey] = @(menuItemParameters.checked.value_or(false));
        info[wasCheckedKey] = @(wasChecked);
    }

    if (!contextParameters.selectionString.isNull())
        info[selectionTextKey] = contextParameters.selectionString;

    if (!contextParameters.sourceURL.isNull()) {
        info[srcURLKey] = contextParameters.sourceURL.string();

        if (contextParameters.types.contains(WebExtensionMenuItemContextType::Audio))
            info[mediaTypeKey] = audioKey;
        else if (contextParameters.types.contains(WebExtensionMenuItemContextType::Image))
            info[mediaTypeKey] = imageKey;
        else if (contextParameters.types.contains(WebExtensionMenuItemContextType::Video))
            info[mediaTypeKey] = videoKey;
    }

    if (!contextParameters.linkURL.isNull())
        info[linkURLKey] = contextParameters.linkURL.string();

    if (!contextParameters.linkText.isNull())
        info[linkTextKey] = contextParameters.linkText;

    if (contextParameters.frameIdentifier && !contextParameters.frameURL.isNull()) {
        info[editableKey] = @(contextParameters.editable);
        info[frameIDKey] = @(toWebAPI(contextParameters.frameIdentifier.value()));

        if (isMainFrame(contextParameters.frameIdentifier))
            info[pageURLKey] = contextParameters.frameURL.string();
        else
            info[frameURLKey] = contextParameters.frameURL.string();
    }

    auto *tab = tabParameters ? toWebAPI(tabParameters.value()) : nil;

    enumerateFramesAndNamespaceObjects([&](auto& frame, auto& namespaceObject) {
        RefPtr coreFrame = frame.protectedCoreLocalFrame();
        WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, coreFrame ? coreFrame->document() : nullptr);

        if (RefPtr clickHandler = namespaceObject.menus().clickHandlers().get(menuItemParameters.identifier))
            clickHandler->call(info, tab);

        namespaceObject.menus().onClicked().invokeListenersWithArgument(info, tab);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
