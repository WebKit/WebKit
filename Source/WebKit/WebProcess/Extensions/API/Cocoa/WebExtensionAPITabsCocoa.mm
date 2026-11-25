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
#import "WebExtensionAPIKeys.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import <JavaScriptCore/MathCommon.h>
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPIPort.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionMatchPattern.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionMessageTargetParameters.h"
#import "WebExtensionScriptInjectionParameters.h"
#import "WebExtensionScriptInjectionResultParameters.h"
#import "WebExtensionTabParameters.h"
#import "WebExtensionTabQueryParameters.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/UserGestureIndicator.h>
#import <wtf/Borrow.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>

namespace WebKit {

NSDictionary *toWebAPI(const WebExtensionTabParameters& parameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/Tab

    auto *result = [NSMutableDictionary dictionary];

    if (parameters.identifier)
        result[idKey] = @(toWebAPI(parameters.identifier.value()));

    if (parameters.url)
        result[urlKey] = !parameters.url.value().isNull() ? createNSString(parameters.url.value().string()).get() : emptyURLValue;

    if (parameters.title)
        result[titleKey] = !!parameters.title.value() ? createNSString(parameters.title.value()).get() : emptyTitleValue;

    if (parameters.windowIdentifier)
        result[windowIdKey] = @(toWebAPI(parameters.windowIdentifier.value()));

    if (parameters.index)
        result[indexKey] = toWebAPI(parameters.index.value());

    if (parameters.size) {
        auto size = parameters.size.value();
        result[widthKey] = @(size.width());
        result[heightKey] = @(size.height());
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

static inline size_t clampIndex(double index)
{
    return static_cast<size_t>(std::max(0.0, std::min(index, static_cast<double>(JSC::maxSafeInteger()))));
}

// Parses a tab identifier or array of tab identifiers into a validated list of WebExtensionTabIdentifiers.
static bool parseTabIdentifiers(NSObject *tabIDs, Vector<WebExtensionTabIdentifier>& identifiers, NSString *sourceKey, NSString **outExceptionString)
{
    if (!validateObject(tabIDs, sourceKey, [NSOrderedSet orderedSetWithObjects:NSNumber.class, @[ NSNumber.class ], nil], outExceptionString))
        return false;

    auto appendIdentifier = [&](NSNumber *tabID) -> bool {
        auto tabIdentifier = toWebExtensionTabIdentifier(tabID.doubleValue);
        if (!isValid(tabIdentifier)) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(sourceKey).get(), makeString("'"_s, StringImpl::create(tabID.description), "' is not a tab identifier"_s))).autorelease();
            return false;
        }

        identifiers.append(tabIdentifier.value());
        return true;
    };

    if (NSNumber *tabID = dynamic_objc_cast<NSNumber>(tabIDs))
        return appendIdentifier(tabID);

    if (NSArray *tabIDArray = dynamic_objc_cast<NSArray>(tabIDs)) {
        identifiers.reserveInitialCapacity(tabIDArray.count);

        for (NSNumber *tabID in tabIDArray) {
            if (!appendIdentifier(tabID))
                return false;
        }
    } else {
        // This should be unreachable if validateObject passed above, but handle it anyways
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(sourceKey).get(), makeString("an internal error occurred"_s))).autorelease();
        return false;
    }

    return true;
}

SUPPRESS_NODELETE NSDictionary *WebExtensionAPITabs::tabStatus()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/TabStatus

    static NSDictionary *statuses = @{
        @"LOADING": loadingKey,
        @"COMPLETE": completeKey,
    };

    return statuses;
}

SUPPRESS_NODELETE NSDictionary *WebExtensionAPITabs::windowType()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/WindowType

    static NSDictionary *types = @{
        @"NORMAL": normalKey,
        @"POPUP": popupKey,
    };

    return types;
}

bool WebExtensionAPITabs::parseTabCreateOptions(NSDictionary *options, WebExtensionTabParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
    if (!parseTabUpdateOptions(options, parameters, sourceKey, outExceptionString))
        return false;

    static NSDictionary<NSString *, id> *types = @{
        indexKey: NSNumber.class,
        openInReaderModeKey: @YES.class,
        titleKey: NSString.class,
        windowIdKey: NSNumber.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSNumber *windowId = objectForKey<NSNumber>(options, windowIdKey)) {
        parameters.windowIdentifier = toWebExtensionWindowIdentifier(windowId.doubleValue);

        if (!parameters.windowIdentifier || !isValid(parameters.windowIdentifier.value())) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(windowIdKey).get(), makeString("'"_s, StringImpl::create([windowId description]), "' is not a window identifier"_s))).autorelease();
            return false;
        }
    }

    if (NSNumber *index = objectForKey<NSNumber>(options, indexKey))
        parameters.index = clampIndex(index.doubleValue);

    if (NSNumber *openInReaderMode = objectForKey<NSNumber>(options, openInReaderModeKey))
        parameters.showingReaderMode = openInReaderMode.boolValue;

    if (NSString *title = objectForKey<NSString>(options, titleKey))
        parameters.title = StringImpl::create(title);

    return true;
}

bool WebExtensionAPITabs::parseTabUpdateOptions(NSDictionary *options, WebExtensionTabParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        activeKey: @YES.class,
        highlightedKey: @YES.class,
        mutedKey: @YES.class,
        openerTabIdKey: NSNumber.class,
        pinnedKey: @YES.class,
        selectedKey: @YES.class,
        urlKey: NSString.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSString *url = objectForKey<NSString>(options, urlKey)) {
        parameters.url = URL { extensionContext().baseURL(), StringImpl::create(url).get() };

        if (!parameters.url.value().isValid()) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(urlKey).get(), makeString("'"_s, StringImpl::create(url), "' is not a valid URL"_s))).autorelease();
            return false;
        }
    }

    if (NSNumber *openerTabId = objectForKey<NSNumber>(options, openerTabIdKey)) {
        parameters.parentTabIdentifier = toWebExtensionTabIdentifier(openerTabId.doubleValue);

        if (!parameters.parentTabIdentifier || !isValid(parameters.parentTabIdentifier.value())) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(openerTabIdKey).get(), makeString("'"_s, StringImpl::create([openerTabId description]), "' is not a tab identifier"_s))).autorelease();
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

bool WebExtensionAPITabs::parseTabDuplicateOptions(NSDictionary *options, WebExtensionTabParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        activeKey: @YES.class,
        indexKey: NSNumber.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSNumber *active = objectForKey<NSNumber>(options, activeKey))
        parameters.active = active.boolValue;

    if (NSNumber *index = objectForKey<NSNumber>(options, indexKey))
        parameters.index = clampIndex(index.doubleValue);

    return true;
}

bool WebExtensionAPITabs::parseTabQueryOptions(NSDictionary *options, WebExtensionTabQueryParameters& parameters, NSString *sourceKey, NSString **outExceptionString)
{
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

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSNumber *windowId = objectForKey<NSNumber>(options, windowIdKey)) {
        parameters.windowIdentifier = toWebExtensionWindowIdentifier(windowId.doubleValue);

        if (!parameters.windowIdentifier || !isValid(parameters.windowIdentifier.value())) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(windowIdKey).get(), makeString("'"_s, StringImpl::create([windowId description]), "' is not a window identifier"_s))).autorelease();
            return false;
        }

        if (isCurrent(parameters.windowIdentifier)) {
            parameters.windowIdentifier = std::nullopt;
            parameters.currentWindow = true;
        }
    }

    if (NSString *windowType = objectForKey<NSString>(options, windowTypeKey)) {
        OptionSet<WebExtensionWindow::TypeFilter> windowTypeFilter;
        if (!WebExtensionAPIWindows::parseWindowTypeFilter(windowType, windowTypeFilter, windowTypeKey, outExceptionString))
            return false;

        parameters.windowType = windowTypeFilter;
    }

    if (NSString *status = objectForKey<NSString>(options, statusKey)) {
        if ([status isEqualToString:loadingKey])
            parameters.loading = true;
        else if ([status isEqualToString:completeKey])
            parameters.loading = false;
        else {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(statusKey).get(), StringImpl::create(@"it must specify either 'loading' or 'complete'").get())).autorelease();
            return false;
        }
    }

    if (NSString *url = objectForKey<NSString>(options, urlKey, true))
        parameters.urlPatterns = { StringImpl::create(url) };
    else if (NSArray *urls = objectForKey<NSArray>(options, urlKey, true))
        parameters.urlPatterns = makeVector<RefPtr<StringImpl>>(urls);

    if (parameters.urlPatterns) {
        for (auto& patternString : borrow(parameters.urlPatterns.value())) {
            auto pattern = WebExtensionMatchPattern::getOrCreate(patternString);
            if (!pattern || !pattern->isSupported()) {
                *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(urlKey).get(), makeString("'"_s, patternString, "' is not a valid pattern"_s))).autorelease();
                return false;
            }
        }
    }

    if (NSString *title = objectForKey<NSString>(options, titleKey, true))
        parameters.titlePattern = StringImpl::create(title);

    if (NSNumber *currentWindow = objectForKey<NSNumber>(options, currentWindowKey))
        parameters.currentWindow = currentWindow.boolValue;

    if (NSNumber *lastFocusedWindow = objectForKey<NSNumber>(options, lastFocusedWindowKey))
        parameters.frontmostWindow = lastFocusedWindow.boolValue;

    if (NSNumber *audible = objectForKey<NSNumber>(options, audibleKey))
        parameters.audible = audible.boolValue;

    if (NSNumber *hidden = objectForKey<NSNumber>(options, hiddenKey))
        parameters.hidden = hidden.boolValue;

    if (NSNumber *index = objectForKey<NSNumber>(options, indexKey))
        parameters.index = clampIndex(index.doubleValue);

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

bool WebExtensionAPITabs::parseCaptureVisibleTabOptions(NSDictionary *options, WebExtensionTab::ImageFormat& imageFormat, uint8_t& imageQuality, NSString *sourceKey, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extensionTypes/ImageDetails

    // Default to PNG image format.
    imageFormat = WebExtensionTab::ImageFormat::PNG;

    // Default to 92 for JPEG.
    imageQuality = 92;

    static NSDictionary<NSString *, id> *types = @{
        formatKey: NSString.class,
        qualityKey: NSNumber.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSString *format = objectForKey<NSString>(options, formatKey)) {
        if ([format isEqualToString:pngValue])
            imageFormat = WebExtensionTab::ImageFormat::PNG;
        else if ([format isEqualToString:jpegValue])
            imageFormat = WebExtensionTab::ImageFormat::JPEG;
        else {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(formatKey).get(), StringImpl::create(@"it must specify either 'png' or 'jpeg'").get())).autorelease();
            return false;
        }
    }

    if (NSNumber *quality = objectForKey<NSNumber>(options, qualityKey)) {
        if (quality.doubleValue < 0 || quality.doubleValue > 100) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(qualityKey).get(), StringImpl::create(@"it must specify a value between 0 and 100").get())).autorelease();
            return false;
        }

        imageQuality = quality.unsignedCharValue;
    }

    return true;
}

bool WebExtensionAPITabs::parseSendMessageOptions(NSDictionary *options, WebExtensionMessageTargetParameters& targetParameters, NSString *sourceKey, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        frameIdKey: NSNumber.class,
        documentIdKey: NSString.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (options[frameIdKey] && options[documentIdKey]) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(sourceKey).get(), StringImpl::create(@"it cannot specify both 'frameId' and 'documentId'").get())).autorelease();
        return false;
    }

    if (NSNumber *frameIdentifier = options[frameIdKey]) {
        auto identifier = toWebExtensionFrameIdentifier(frameIdentifier.doubleValue);
        if (!isValid(identifier)) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(frameIdKey).get(), makeString("'"_s, StringImpl::create([frameIdentifier description]), "' is not a frame identifier"_s))).autorelease();
            return false;
        }

        targetParameters.frameIdentifier = identifier.value();
    }

    if (NSString *documentIdentifier = options[documentIdKey]) {
        auto parsedUUID = WTF::UUID::parse(StringImpl::create(documentIdentifier).get());
        if (!parsedUUID) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(documentIdKey).get(), makeString("'"_s, StringImpl::create(documentIdentifier), "' is not a document identifier"_s))).autorelease();
            return false;
        }

        targetParameters.documentIdentifier = parsedUUID.value();
    }

    return true;
}

bool WebExtensionAPITabs::parseConnectOptions(NSDictionary *options, std::optional<RefPtr<StringImpl>>& name, WebExtensionMessageTargetParameters& targetParameters, NSString *sourceKey, NSString **outExceptionString)
{
    if (!parseSendMessageOptions(options, targetParameters, sourceKey, outExceptionString))
        return false;

    static NSDictionary<NSString *, id> *types = @{
        nameKey: NSString.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSString *nameString = options[nameKey])
        name = StringImpl::create(nameString);

    return true;
}

bool WebExtensionAPITabs::parseScriptOptions(NSDictionary *options, WebExtensionScriptInjectionParameters& parameters, NSString **outExceptionString)
{
    static auto *keyTypes = @{
        allFramesKey: @YES.class,
        codeKey: NSString.class,
        documentIdKey: NSString.class,
        fileKey: NSString.class,
        frameIdKey: NSNumber.class,
        tabIdKey: NSNumber.class,
    };

    if (!validateDictionary(options, @"details", nil, keyTypes, outExceptionString))
        return false;

    if (options[fileKey] && options[codeKey]) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"details").get(), StringImpl::create(@"it cannot specify both 'file' and 'code'").get())).autorelease();
        return false;
    }

    if (!options[fileKey] && !options[codeKey]) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"details").get(), StringImpl::create(@"it must specify either 'file' or 'code'").get())).autorelease();
        return false;
    }

    bool allFrames = boolForKey(options, allFramesKey, false);
    if (allFrames && options[frameIdKey]) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"details").get(), StringImpl::create(@"it cannot specify both 'allFrames' and 'frameId'").get())).autorelease();
        return false;
    }

    if (options[frameIdKey] && options[documentIdKey]) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"details").get(), StringImpl::create(@"it cannot specify both 'frameId' and 'documentId'").get())).autorelease();
        return false;
    }

    if (allFrames && options[documentIdKey]) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"details").get(), StringImpl::create(@"it cannot specify both 'allFrames' and 'documentId'").get())).autorelease();
        return false;
    }

    if (NSString *filePath = options[fileKey])
        parameters.files = { StringImpl::create(filePath) };

    if (NSString *code = options[codeKey])
        parameters.code = StringImpl::create(code);

    if (NSString *documentIdentifer = options[documentIdKey]) {
        auto parsedUUID = WTF::UUID::parse(StringImpl::create(documentIdentifer).get());
        if (!parsedUUID) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(documentIdKey).get(), makeString("'"_s, StringImpl::create(documentIdentifer), "' is not a valid document identifier"_s))).autorelease();
            return false;
        }

        parameters.documentIdentifiers = { WTF::move(parsedUUID.value()) };
    }

    if (NSNumber *frameID = options[frameIdKey]) {
        auto frameIdentifier = toWebExtensionFrameIdentifier(frameID.doubleValue);
        if (!isValid(frameIdentifier)) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(frameIdKey).get(), makeString("'"_s, StringImpl::create([frameID description]), "' is not a frame identifier"_s))).autorelease();
            return false;
        }

        parameters.frameIdentifiers = { frameIdentifier.value() };
    } else if (!allFrames && !parameters.documentIdentifiers)
        parameters.frameIdentifiers = { WebExtensionFrameConstants::MainFrameIdentifier };

    if (NSString *origin = options[cssOriginKey]) {
        if ([origin isEqualToString:userValue])
            parameters.styleLevel = WebCore::UserStyleLevel::User;
        else if ([origin isEqualToString:authorValue])
            parameters.styleLevel = WebCore::UserStyleLevel::Author;
        else {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(cssOriginKey).get(), StringImpl::create(@"it must specify either 'author' or 'user'").get())).autorelease();
            return false;
        }
    }

    return true;
}

bool isValid(std::optional<WebExtensionTabIdentifier> identifier, NSString **outExceptionString)
{
    if (!isValid(identifier)) [[unlikely]] {
        if (isNone(identifier))
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"tabId").get(), StringImpl::create(@"'tabs.TAB_ID_NONE' is not allowed").get())).autorelease();
        else if (identifier)
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"tabId").get(), makeString("'"_s, identifier.value().toUInt64(), "' is not a tab identifier"_s))).autorelease();
        else
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"tabId").get(), StringImpl::create(@"it is not a tab identifier").get())).autorelease();
        return false;
    }

    return true;
}

bool WebExtensionAPITabs::isPropertyAllowed(const ASCIILiteral& name, WebPage*)
{
    RefPtr context = extensionContext();
    if (context->isUnsupportedAPI(propertyPath(), name)) [[unlikely]]
        return false;

    static NeverDestroyed<HashSet<RefPtr<AtomStringImpl>>> removedInManifestVersion3 { HashSet { "executeScript"_atom, "getSelected"_atom, "insertCSS"_atom, "removeCSS"_atom } };
    if (removedInManifestVersion3.get().contains(AtomStringImpl::add(name).ptr()))
        return !context->supportsManifestVersion(3);

    ASSERT_NOT_REACHED();
    return false;
}

void WebExtensionAPITabs::createTab(WebPageProxyIdentifier webPageProxyIdentifier, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/create

    WebExtensionTabParameters parameters;
    if (!parseTabCreateOptions(properties, parameters, @"properties", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsCreate(webPageProxyIdentifier, WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::query(WebPageProxyIdentifier webPageProxyIdentifier, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/query

    WebExtensionTabQueryParameters parameters;
    if (!parseTabQueryOptions(options, parameters, @"info", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsQuery(webPageProxyIdentifier, WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::get(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/get

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGet(tabIdentifer.value()), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::getCurrent(WebPageProxyIdentifier webPageProxyIdentifier, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getCurrent

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGetCurrent(webPageProxyIdentifier), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::getSelected(WebPageProxyIdentifier webPageProxyIdentifier, double windowID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getSelected

    auto windowIdentifer = toWebExtensionWindowIdentifier(windowID);
    if (windowIdentifer && !isValid(windowIdentifer, outExceptionString))
        return;

    WebExtensionTabQueryParameters parameters;
    parameters.windowIdentifier = windowIdentifer.value_or(WebExtensionWindowConstants::CurrentIdentifier);
    parameters.active = true;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsQuery(webPageProxyIdentifier, WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        auto& tabs = result.value();
        if (tabs.isEmpty()) {
            callback->call();
            return;
        }

        ASSERT(tabs.size() == 1);

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(borrow(tabs)->first())));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::duplicate(double tabID, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/duplicate

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    WebExtensionTabParameters parameters;
    if (properties && !parseTabDuplicateOptions(properties, parameters, @"properties", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsDuplicate(tabIdentifer.value(), WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::update(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/update

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebExtensionTabParameters parameters;
    if (!parseTabUpdateOptions(properties, parameters, @"properties", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsUpdate(webPageProxyIdentifier, tabIdentifer, WTF::move(parameters)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value())));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::move(NSObject *tabIDs, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/move

    Vector<WebExtensionTabIdentifier> identifiers;
    if (!parseTabIdentifiers(tabIDs, identifiers, @"tabIDs", outExceptionString))
        return;

    if (identifiers.isEmpty()) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"tabIDs").get(), "no tabs were specified"_si)).autorelease();
        return;
    }

    static NSDictionary<NSString *, id> *types = @{
        windowIdKey: NSNumber.class,
        indexKey: NSNumber.class,
    };

    if (!validateDictionary(properties, @"properties", @[ indexKey ], types, outExceptionString))
        return;

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    if (NSNumber *windowId = objectForKey<NSNumber>(properties, windowIdKey)) {
        windowIdentifier = toWebExtensionWindowIdentifier(windowId.doubleValue);

        if (!windowIdentifier || !isValid(windowIdentifier.value())) {
            *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(windowIdKey).get(), makeString("'"_s, StringImpl::create(windowId.description), "' is not a window identifier"_s))).autorelease();
            return;
        }
    }

    double index = objectForKey<NSNumber>(properties, indexKey).doubleValue;

    double integral;
    if (std::modf(index, &integral) != 0.0) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(indexKey).get(), "it must be an integer"_si)).autorelease();
        return;
    }

    if (index < -1) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(indexKey).get(), "it must be -1 or greater"_si)).autorelease();
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsMove(WTF::move(identifiers), windowIdentifier, index), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        auto& movedTabs = result.value();

        if (movedTabs.size() == 1)
            callback->call(toJSValueRef(callback->globalContext(), toWebAPI(borrow(movedTabs)[0])));
        else
            callback->call(toJSValueRef(callback->globalContext(), toWebAPI(movedTabs)));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::remove(NSObject *tabIDs, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/remove

    Vector<WebExtensionTabIdentifier> identifiers;
    if (!parseTabIdentifiers(tabIDs, identifiers, @"tabIDs", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsRemove(WTF::move(identifiers)), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::reload(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/reload

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    static NSDictionary<NSString *, id> *types = @{
        bypassCacheKey: @YES.class,
    };

    if (properties && !validateDictionary(properties, @"properties", nil, types, outExceptionString))
        return;

    NSNumber *bypassCacheNumber = objectForKey<NSNumber>(properties, bypassCacheKey);
    ReloadFromOrigin reloadFromOrigin = bypassCacheNumber.boolValue ? ReloadFromOrigin::Yes : ReloadFromOrigin::No;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsReload(webPageProxyIdentifier, tabIdentifer, reloadFromOrigin), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::goBack(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/goBack

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGoBack(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::goForward(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/goForward

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGoForward(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::getZoom(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getZoom

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGetZoom(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<double, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(JSValueMakeNumber(callback->globalContext(), result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::setZoom(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, double zoomFactor, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/setZoom

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsSetZoom(webPageProxyIdentifier, tabIdentifer, zoomFactor), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::detectLanguage(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/detectLanguage

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsDetectLanguage(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<RefPtr<StringImpl>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        if (isEmpty(result.value())) {
            callback->call(JSValueMakeString(callback->globalContext(), toJSString(StringImpl::create(unknownLanguageValue).get()).get()));
            return;
        }

        callback->call(JSValueMakeString(callback->globalContext(), toJSString(result.value()).get()));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::toggleReaderMode(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/toggleReaderMode

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsToggleReaderMode(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::captureVisibleTab(WebPageProxyIdentifier webPageProxyIdentifier, double windowID, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/captureVisibleTab

    auto windowIdentifier = toWebExtensionWindowIdentifier(windowID);
    if (windowIdentifier && !isValid(windowIdentifier, outExceptionString))
        return;

    WebExtensionTab::ImageFormat imageFormat;
    uint8_t imageQuality;

    if (!parseCaptureVisibleTabOptions(options, imageFormat, imageQuality, @"options", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsCaptureVisibleTab(webPageProxyIdentifier, windowIdentifier, imageFormat, imageQuality), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<URL, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        if (result.value().isEmpty()) {
            callback->call(JSValueMakeString(callback->globalContext(), toJSString(StringImpl::create(emptyDataURLValue).get()).get()));
            return;
        }

        callback->call(JSValueMakeString(callback->globalContext(), toJSString(result.value().string()).get()));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::sendMessage(WebFrame& frame, double tabID, const WebExtensionMessage& message, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/sendMessage

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    if (message.length() > webExtensionMaxMessageLength) {
        *outExceptionString = createNSString(toErrorString(nullString(), StringImpl::create(@"message").get(), StringImpl::create(@"it exceeded the maximum allowed length").get())).autorelease();
        return;
    }

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = createNSString(toErrorString(StringImpl::create(@"runtime.sendMessage()").get(), nullString(), StringImpl::create(@"an unexpected error occured").get())).autorelease();
        return;
    }

    WebExtensionMessageTargetParameters targetParameters;
    if (!parseSendMessageOptions(options, targetParameters, @"options", outExceptionString))
        return;

    WebExtensionMessageSenderParameters senderParameters {
        extensionContext().uniqueIdentifier(),
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(frame),
        frame.page()->webPageProxyIdentifier(),
        contentWorldType(),
        frame.url(),
        documentIdentifier,
        std::nullopt, // windowIdentifier
    };

    bool userGesture = WebCore::UserGestureIndicator::processingUserGesture();
    // The message keeps any Blob it references alive, so hold it until the receiver replies.
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsSendMessage(tabIdentifer.value(), message, targetParameters, senderParameters, userGesture), [protectedThis = Ref { *this }, callback = WTF::move(callback), message](std::expected<WebExtensionMessage, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(result->toJSValue(callback->globalContext()));
    }, extensionContext().identifier());
}

RefPtr<WebExtensionAPIPort> WebExtensionAPITabs::connect(WebFrame& frame, JSContextRef context, double tabID, NSDictionary *options, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/connect

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return nullptr;

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = createNSString(toErrorString(nullString(), nullString(), StringImpl::create(@"an unexpected error occured").get())).autorelease();
        return nullptr;
    }

    std::optional<RefPtr<StringImpl>> name;
    WebExtensionMessageTargetParameters targetParameters;
    if (!parseConnectOptions(options, name, targetParameters, @"options", outExceptionString))
        return nullptr;

    RefPtr<StringImpl> resolvedName = name.value_or(nullString());

    WebExtensionMessageSenderParameters senderParameters {
        extensionContext().uniqueIdentifier(),
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(frame),
        frame.page()->webPageProxyIdentifier(),
        contentWorldType(),
        frame.url(),
        documentIdentifier,
        std::nullopt, // windowIdentifier
    };

    Ref port = WebExtensionAPIPort::create(*this, frame.page()->webPageProxyIdentifier(), WebExtensionContentWorldType::ContentScript, resolvedName);

    bool userGesture = WebCore::UserGestureIndicator::processingUserGesture();
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsConnect(tabIdentifer.value(), port->channelIdentifier(), resolvedName, targetParameters, senderParameters, userGesture), [=, this, protectedThis = Ref { *this }, globalContext = JSRetainPtr { JSContextGetGlobalContext(context) }](std::expected<void, WebExtensionError>&& result) {
        if (result)
            return;

        port->setError(globalContext.get(), protect(runtime())->reportError(result.error().get(), globalContext.get()));
        port->fireDisconnectEventIfNeeded();
    }, extensionContext().identifier());

    return port;
}

void WebExtensionAPITabs::executeScript(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, NSDictionary *options, Ref<WebExtensionCallbackHandler> && callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/executeScript

    auto tabIdentifier = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifier && !isValid(tabIdentifier, outExceptionString))
        return;

    WebExtensionScriptInjectionParameters parameters;
    if (options && !parseScriptOptions(options, parameters, outExceptionString))
        return;

    bool userGesture = WebCore::UserGestureIndicator::processingUserGesture();
    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsExecuteScript(webPageProxyIdentifier, tabIdentifier, parameters, userGesture), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<Vector<WebExtensionScriptInjectionResultParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call(toJSValueRef(callback->globalContext(), toWebAPI(result.value(), true)));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::insertCSS(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, NSDictionary *options, Ref<WebExtensionCallbackHandler> && callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/insertCSS

    auto tabIdentifier = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifier && !isValid(tabIdentifier, outExceptionString))
        return;

    WebExtensionScriptInjectionParameters parameters;
    if (options && !parseScriptOptions(options, parameters, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsInsertCSS(webPageProxyIdentifier, tabIdentifier, parameters), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::removeCSS(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, NSDictionary *options, Ref<WebExtensionCallbackHandler> && callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/removeCSS

    auto tabIdentifier = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifier && !isValid(tabIdentifier, outExceptionString))
        return;

    WebExtensionScriptInjectionParameters parameters;
    if (options && !parseScriptOptions(options, parameters, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsRemoveCSS(webPageProxyIdentifier, tabIdentifier, parameters), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPITabs::onActivated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onActivated

    if (!m_onActivated)
        lazyInitialize(m_onActivated, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnActivated));

    return *m_onActivated;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onAttached()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onAttached

    if (!m_onAttached)
        lazyInitialize(m_onAttached, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnAttached));

    return *m_onAttached;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onCreated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onCreated

    if (!m_onCreated)
        lazyInitialize(m_onCreated, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnCreated));

    return *m_onCreated;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onDetached()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onDetached

    if (!m_onDetached)
        lazyInitialize(m_onDetached, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnDetached));

    return *m_onDetached;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onHighlighted()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onHighlighted

    if (!m_onHighlighted)
        lazyInitialize(m_onHighlighted, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnHighlighted));

    return *m_onHighlighted;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onMoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onMoved

    if (!m_onMoved)
        lazyInitialize(m_onMoved, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnMoved));

    return *m_onMoved;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onRemoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onRemoved

    if (!m_onRemoved)
        lazyInitialize(m_onRemoved, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnRemoved));

    return *m_onRemoved;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onReplaced()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onReplaced

    if (!m_onReplaced)
        lazyInitialize(m_onReplaced, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnReplaced));

    return *m_onReplaced;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onUpdated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onUpdated

    if (!m_onUpdated)
        lazyInitialize(m_onUpdated, WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnUpdated));

    return *m_onUpdated;
}

void WebExtensionContextProxy::dispatchTabsCreatedEvent(const WebExtensionTabParameters& parameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onCreated

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onCreated())->invokeListenersWithArgument(toWebAPI(parameters));
    });
}

void WebExtensionContextProxy::dispatchTabsUpdatedEvent(const WebExtensionTabParameters& parameters, const WebExtensionTabParameters& changedParameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onUpdated

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onUpdated())->invokeListenersWithArgument(@(toWebAPI(parameters.identifier.value())), toWebAPI(changedParameters), toWebAPI(parameters));
    });
}

void WebExtensionContextProxy::dispatchTabsReplacedEvent(WebExtensionTabIdentifier replacedTabIdentifier, WebExtensionTabIdentifier newTabIdentifier)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onReplaced

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onReplaced())->invokeListenersWithArgument(@(toWebAPI(newTabIdentifier)), @(toWebAPI(replacedTabIdentifier)));
    });
}

void WebExtensionContextProxy::dispatchTabsDetachedEvent(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier oldWindowIdentifier, uint64_t oldIndex)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onDetached

    for (auto entry : m_tabPageMap) {
        if (!entry.value.first || entry.value.first.value() != tabIdentifier)
            continue;

        // Clear the window id until it is reattached.
        entry.value.second = std::nullopt;
        break;
    }

    auto *detachInfo = @{ oldWindowIdKey: @(toWebAPI(oldWindowIdentifier)), oldPositionKey: toWebAPI(oldIndex) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onDetached())->invokeListenersWithArgument(@(toWebAPI(tabIdentifier)), detachInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsMovedEvent(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier windowIdentifier, uint64_t oldIndex, uint64_t newIndex)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onMoved

    auto *moveInfo = @{ windowIdKey: @(toWebAPI(windowIdentifier)), fromIndexKey: toWebAPI(oldIndex), toIndexKey: toWebAPI(newIndex) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onMoved())->invokeListenersWithArgument(@(toWebAPI(tabIdentifier)), moveInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsAttachedEvent(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier newWindowIdentifier, uint64_t newIndex)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onAttached

    for (auto entry : m_tabPageMap) {
        if (!entry.value.first || entry.value.first.value() != tabIdentifier)
            continue;

        entry.value.second = newWindowIdentifier;
        break;
    }

    auto *attachInfo = @{ newWindowIdKey: @(toWebAPI(newWindowIdentifier)), newPositionKey: toWebAPI(newIndex) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onAttached())->invokeListenersWithArgument(@(toWebAPI(tabIdentifier)), attachInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsActivatedEvent(WebExtensionTabIdentifier previousActiveTabIdentifier, WebExtensionTabIdentifier newActiveTabIdentifier, WebExtensionWindowIdentifier windowIdentifier)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onActivated

    auto *activateInfo = @{ previousTabIdKey: @(toWebAPI(previousActiveTabIdentifier)), tabIdKey: @(toWebAPI(newActiveTabIdentifier)), windowIdKey: @(toWebAPI(windowIdentifier)) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onActivated())->invokeListenersWithArgument(activateInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsHighlightedEvent(const Vector<WebExtensionTabIdentifier>& tabs, WebExtensionWindowIdentifier windowIdentifier)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onHighlighted

    auto *tabIds = createNSArray(tabs, [](auto& tabIdentifier) {
        return @(toWebAPI(tabIdentifier));
    }).get();

    auto *highlightInfo = @{ windowIdKey: @(toWebAPI(windowIdentifier)), tabIdsKey: tabIds };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onHighlighted())->invokeListenersWithArgument(highlightInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsRemovedEvent(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier windowIdentifier, WebExtensionContextWindowIsClosing windowIsClosing)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onRemoved

    auto *removeInfo = @{ windowIdKey: @(toWebAPI(windowIdentifier)), isWindowClosingKey: @(windowIsClosing == WebExtensionContextWindowIsClosing::Yes) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        protect(protect(namespaceObject.tabs())->onRemoved())->invokeListenersWithArgument(@(toWebAPI(tabIdentifier)), removeInfo);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
