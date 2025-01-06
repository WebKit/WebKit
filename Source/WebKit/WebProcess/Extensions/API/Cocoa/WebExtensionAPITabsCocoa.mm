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
#import "WebExtensionAPIPort.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
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
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>

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

static NSString * const bypassCacheKey = @"bypassCache";

static NSString * const oldWindowIdKey = @"oldWindowId";
static NSString * const oldPositionKey = @"oldPosition";

static NSString * const newWindowIdKey = @"newWindowId";
static NSString * const newPositionKey = @"newPosition";

static NSString * const previousTabIdKey = @"previousTabId";
static NSString * const tabIdKey = @"tabId";

static NSString * const fromIndexKey = @"fromIndex";
static NSString * const toIndexKey = @"toIndex";

static NSString * const isWindowClosingKey = @"isWindowClosing";

static NSString * const tabIdsKey = @"tabIds";

static NSString * const formatKey = @"format";
static NSString * const pngValue = @"png";
static NSString * const jpegValue = @"jpeg";

static NSString * const qualityKey = @"quality";

static NSString * const frameIdKey = @"frameId";
static NSString * const documentIdKey = @"documentId";
static NSString * const nameKey = @"name";

static NSString * const allFramesKey = @"allFrames";
static NSString * const codeKey = @"code";
static NSString * const fileKey = @"file";
static NSString * const cssOriginKey = @"cssOrigin";

static NSString * const authorValue = @"author";
static NSString * const userValue = @"user";

static NSString * const emptyURLValue = @"";
static NSString * const emptyTitleValue = @"";
static NSString * const emptyDataURLValue = @"data:,";
static NSString * const unknownLanguageValue = @"und";

namespace WebKit {

NSDictionary *toWebAPI(const WebExtensionTabParameters& parameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/Tab

    auto *result = [NSMutableDictionary dictionary];

    if (parameters.identifier)
        result[idKey] = @(toWebAPI(parameters.identifier.value()));

    if (parameters.url)
        result[urlKey] = !parameters.url.value().isNull() ? (NSString *)parameters.url.value().string() : emptyURLValue;

    if (parameters.title)
        result[titleKey] = !parameters.title.value().isNull() ? (NSString *)parameters.title.value() : emptyTitleValue;

    if (parameters.windowIdentifier)
        result[windowIdKey] = @(toWebAPI(parameters.windowIdentifier.value()));

    if (parameters.index)
        result[indexKey] = toWebAPI(parameters.index.value());

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
        parameters.url = URL { extensionContext().baseURL(), url };

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
        parameters.index = index.unsignedIntegerValue;

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
            *outExceptionString = toErrorString(nil, windowIdKey, @"'%@' is not a window identifier", windowId);
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
            *outExceptionString = toErrorString(nil, statusKey, @"it must specify either 'loading' or 'complete'");
            return false;
        }
    }

    if (NSString *url = objectForKey<NSString>(options, urlKey, true))
        parameters.urlPatterns = { url };
    else if (NSArray *urls = objectForKey<NSArray>(options, urlKey, true))
        parameters.urlPatterns = makeVector<String>(urls);

    if (parameters.urlPatterns) {
        for (auto& patternString : parameters.urlPatterns.value()) {
            auto pattern = WebExtensionMatchPattern::getOrCreate(patternString);
            if (!pattern || !pattern->isSupported()) {
                *outExceptionString = toErrorString(nil, urlKey, @"'%@' is not a valid pattern", (NSString *)patternString);
                return false;
            }
        }
    }

    if (NSString *title = objectForKey<NSString>(options, titleKey, true))
        parameters.titlePattern = title;

    if (NSNumber *currentWindow = objectForKey<NSNumber>(options, currentWindowKey))
        parameters.currentWindow = currentWindow.boolValue;

    if (NSNumber *lastFocusedWindow = objectForKey<NSNumber>(options, lastFocusedWindowKey))
        parameters.frontmostWindow = lastFocusedWindow.boolValue;

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
            *outExceptionString = toErrorString(nil, formatKey, @"it must specify either 'png' or 'jpeg'");
            return false;
        }
    }

    if (NSNumber *quality = objectForKey<NSNumber>(options, qualityKey)) {
        if (quality.integerValue < 0 || quality.integerValue > 100) {
            *outExceptionString = toErrorString(nil, qualityKey, @"it must specify a value between 0 and 100");
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
        *outExceptionString = toErrorString(nil, sourceKey, @"it cannot specify both 'frameId' and 'documentId'");
        return false;
    }

    if (NSNumber *frameIdentifier = options[frameIdKey]) {
        auto identifier = toWebExtensionFrameIdentifier(frameIdentifier.doubleValue);
        if (!isValid(identifier)) {
            *outExceptionString = toErrorString(nil, frameIdKey, @"'%@' is not a frame identifier", frameIdentifier);
            return false;
        }

        targetParameters.frameIdentifier = identifier.value();
    }

    if (NSString *documentIdentifier = options[documentIdKey]) {
        auto parsedUUID = WTF::UUID::parse(String(documentIdentifier));
        if (!parsedUUID) {
            *outExceptionString = toErrorString(nil, documentIdKey, @"'%@' is not a document identifier", documentIdentifier);
            return false;
        }

        targetParameters.documentIdentifier = parsedUUID.value();
    }

    return true;
}

bool WebExtensionAPITabs::parseConnectOptions(NSDictionary *options, std::optional<String>& name, WebExtensionMessageTargetParameters& targetParameters, NSString *sourceKey, NSString **outExceptionString)
{
    if (!parseSendMessageOptions(options, targetParameters, sourceKey, outExceptionString))
        return false;

    static NSDictionary<NSString *, id> *types = @{
        nameKey: NSString.class,
    };

    if (!validateDictionary(options, sourceKey, nil, types, outExceptionString))
        return false;

    if (NSString *nameString = options[nameKey])
        name = nameString;

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
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'file' and 'code'");
        return false;
    }

    if (!options[fileKey] && !options[codeKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it must specify either 'file' or 'code'");
        return false;
    }

    bool allFrames = boolForKey(options, allFramesKey, false);
    if (allFrames && options[frameIdKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'allFrames' and 'frameId'");
        return false;
    }

    if (options[frameIdKey] && options[documentIdKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'frameId' and 'documentId'");
        return false;
    }

    if (allFrames && options[documentIdKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'allFrames' and 'documentId'");
        return false;
    }

    if (NSString *filePath = options[fileKey])
        parameters.files = { filePath };

    if (NSString *code = options[codeKey])
        parameters.code = code;

    if (NSString *documentIdentifer = options[documentIdKey]) {
        auto parsedUUID = WTF::UUID::parse(String(documentIdentifer));
        if (!parsedUUID) {
            *outExceptionString = toErrorString(nil, documentIdKey, @"'%@' is not a valid document identifier", documentIdentifer);
            return false;
        }

        parameters.documentIdentifiers = { WTFMove(parsedUUID.value()) };
    }

    if (NSNumber *frameID = options[frameIdKey]) {
        auto frameIdentifier = toWebExtensionFrameIdentifier(frameID.doubleValue);
        if (!isValid(frameIdentifier)) {
            *outExceptionString = toErrorString(nil, frameIdKey, @"'%@' is not a frame identifier", frameID);
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
            *outExceptionString = toErrorString(nil, cssOriginKey, @"it must specify either 'author' or 'user'");
            return false;
        }
    }

    return true;
}

bool isValid(std::optional<WebExtensionTabIdentifier> identifier, NSString **outExceptionString)
{
    if (UNLIKELY(!isValid(identifier))) {
        if (isNone(identifier))
            *outExceptionString = toErrorString(nil, @"tabId", @"'tabs.TAB_ID_NONE' is not allowed");
        else if (identifier)
            *outExceptionString = toErrorString(nil, @"tabId", @"'%llu' is not a tab identifier", identifier.value().toUInt64());
        else
            *outExceptionString = toErrorString(nil, @"tabId", @"it is not a tab identifier");
        return false;
    }

    return true;
}

bool WebExtensionAPITabs::isPropertyAllowed(const ASCIILiteral& name, WebPage*)
{
    if (UNLIKELY(extensionContext().isUnsupportedAPI(propertyPath(), name)))
        return false;

    static NeverDestroyed<HashSet<AtomString>> removedInManifestVersion3 { HashSet { AtomString("executeScript"_s), AtomString("getSelected"_s), AtomString("insertCSS"_s), AtomString("removeCSS"_s) } };
    if (removedInManifestVersion3.get().contains(name))
        return !extensionContext().supportsManifestVersion(3);

    ASSERT_NOT_REACHED();
    return false;
}

void WebExtensionAPITabs::createTab(WebPageProxyIdentifier webPageProxyIdentifier, NSDictionary *properties, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/create

    WebExtensionTabParameters parameters;
    if (!parseTabCreateOptions(properties, parameters, @"properties", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsCreate(webPageProxyIdentifier, WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::query(WebPageProxyIdentifier webPageProxyIdentifier, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/query

    WebExtensionTabQueryParameters parameters;
    if (!parseTabQueryOptions(options, parameters, @"info", outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsQuery(webPageProxyIdentifier, WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::get(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/get

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGet(tabIdentifer.value()), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::getCurrent(WebPageProxyIdentifier webPageProxyIdentifier, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/getCurrent

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGetCurrent(webPageProxyIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsQuery(webPageProxyIdentifier, WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        auto& tabs = result.value();
        if (tabs.isEmpty()) {
            callback->call();
            return;
        }

        ASSERT(tabs.size() == 1);

        callback->call(toWebAPI(tabs.first()));
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsDuplicate(tabIdentifer.value(), WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsUpdate(webPageProxyIdentifier, tabIdentifer, WTFMove(parameters)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value()));
    }, extensionContext().identifier());
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

            identifiers.append(tabIdentifer.value());
        }
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsRemove(WTFMove(identifiers)), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
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

    using ReloadFromOrigin = WebExtensionContext::ReloadFromOrigin;

    NSNumber *bypassCacheNumber = objectForKey<NSNumber>(properties, bypassCacheKey);
    ReloadFromOrigin reloadFromOrigin = bypassCacheNumber.boolValue ? ReloadFromOrigin::Yes : ReloadFromOrigin::No;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsReload(webPageProxyIdentifier, tabIdentifer, reloadFromOrigin), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGoBack(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGoForward(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsGetZoom(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<double, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(@(result.value()));
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::setZoom(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, double zoomFactor, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/setZoom

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsSetZoom(webPageProxyIdentifier, tabIdentifer, zoomFactor), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsDetectLanguage(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        if (result.value().isEmpty()) {
            callback->call(unknownLanguageValue);
            return;
        }

        callback->call(result.value());
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::toggleReaderMode(WebPageProxyIdentifier webPageProxyIdentifier, double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/toggleReaderMode

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsToggleReaderMode(webPageProxyIdentifier, tabIdentifer), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsCaptureVisibleTab(webPageProxyIdentifier, windowIdentifier, imageFormat, imageQuality), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<URL, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        if (result.value().isEmpty()) {
            callback->call(emptyDataURLValue);
            return;
        }

        callback->call(result.value().string());
    }, extensionContext().identifier());
}

void WebExtensionAPITabs::sendMessage(WebFrame& frame, double tabID, NSString *messageJSON, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/sendMessage

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (!isValid(tabIdentifer, outExceptionString))
        return;

    if (messageJSON.length > webExtensionMaxMessageLength) {
        *outExceptionString = toErrorString(nil, @"message", @"it exceeded the maximum allowed length");
        return;
    }

    auto documentIdentifier = toDocumentIdentifier(frame);
    if (!documentIdentifier) {
        *outExceptionString = toErrorString(@"runtime.sendMessage()", nil, @"an unexpected error occured");
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
        documentIdentifier.value(),
    };

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsSendMessage(tabIdentifer.value(), messageJSON, targetParameters, senderParameters), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(parseJSON(result.value(), JSONOptions::FragmentsAllowed));
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
        *outExceptionString = toErrorString(nil, nil, @"an unexpected error occured");
        return nullptr;
    }

    std::optional<String> name;
    WebExtensionMessageTargetParameters targetParameters;
    if (!parseConnectOptions(options, name, targetParameters, @"options", outExceptionString))
        return nullptr;

    String resolvedName = name.value_or(nullString());

    WebExtensionMessageSenderParameters senderParameters {
        extensionContext().uniqueIdentifier(),
        std::nullopt, // tabParameters
        toWebExtensionFrameIdentifier(frame),
        frame.page()->webPageProxyIdentifier(),
        contentWorldType(),
        frame.url(),
        documentIdentifier.value(),
    };

    Ref port = WebExtensionAPIPort::create(*this, frame.protectedPage()->webPageProxyIdentifier(), WebExtensionContentWorldType::ContentScript, resolvedName);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsConnect(tabIdentifer.value(), port->channelIdentifier(), resolvedName, targetParameters, senderParameters), [=, this, protectedThis = Ref { *this }, globalContext = JSRetainPtr { JSContextGetGlobalContext(context) }](Expected<void, WebExtensionError>&& result) {
        if (result)
            return;

        port->setError(runtime().reportError(result.error(), globalContext.get()));
        port->disconnect();
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsExecuteScript(webPageProxyIdentifier, tabIdentifier, parameters), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Vector<WebExtensionScriptInjectionResultParameters>, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(toWebAPI(result.value(), true));
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsInsertCSS(webPageProxyIdentifier, tabIdentifier, parameters), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::TabsRemoveCSS(webPageProxyIdentifier, tabIdentifier, parameters), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPITabs::onActivated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onActivated

    if (!m_onActivated)
        m_onActivated = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnActivated);

    return *m_onActivated;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onAttached()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onAttached

    if (!m_onAttached)
        m_onAttached = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnAttached);

    return *m_onAttached;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onCreated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onCreated

    if (!m_onCreated)
        m_onCreated = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnCreated);

    return *m_onCreated;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onDetached()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onDetached

    if (!m_onDetached)
        m_onDetached = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnDetached);

    return *m_onDetached;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onHighlighted()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onHighlighted

    if (!m_onHighlighted)
        m_onHighlighted = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnHighlighted);

    return *m_onHighlighted;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onMoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onMoved

    if (!m_onMoved)
        m_onMoved = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnMoved);

    return *m_onMoved;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onRemoved()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onRemoved

    if (!m_onRemoved)
        m_onRemoved = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnRemoved);

    return *m_onRemoved;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onReplaced()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onReplaced

    if (!m_onReplaced)
        m_onReplaced = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnReplaced);

    return *m_onReplaced;
}

WebExtensionAPIEvent& WebExtensionAPITabs::onUpdated()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onUpdated

    if (!m_onUpdated)
        m_onUpdated = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TabsOnUpdated);

    return *m_onUpdated;
}

void WebExtensionContextProxy::dispatchTabsCreatedEvent(const WebExtensionTabParameters& parameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onCreated

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.tabs().onCreated().invokeListenersWithArgument(toWebAPI(parameters));
    });
}

void WebExtensionContextProxy::dispatchTabsUpdatedEvent(const WebExtensionTabParameters& parameters, const WebExtensionTabParameters& changedParameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onUpdated

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.tabs().onUpdated().invokeListenersWithArgument(@(toWebAPI(parameters.identifier.value())), toWebAPI(changedParameters), toWebAPI(parameters));
    });
}

void WebExtensionContextProxy::dispatchTabsReplacedEvent(WebExtensionTabIdentifier replacedTabIdentifier, WebExtensionTabIdentifier newTabIdentifier)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onReplaced

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.tabs().onReplaced().invokeListenersWithArgument(@(toWebAPI(newTabIdentifier)), @(toWebAPI(replacedTabIdentifier)));
    });
}

void WebExtensionContextProxy::dispatchTabsDetachedEvent(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier oldWindowIdentifier, size_t oldIndex)
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
        namespaceObject.tabs().onDetached().invokeListenersWithArgument(@(toWebAPI(tabIdentifier)), detachInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsMovedEvent(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier windowIdentifier, size_t oldIndex, size_t newIndex)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onMoved

    auto *moveInfo = @{ windowIdKey: @(toWebAPI(windowIdentifier)), fromIndexKey: toWebAPI(oldIndex), toIndexKey: toWebAPI(newIndex) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.tabs().onMoved().invokeListenersWithArgument(@(toWebAPI(tabIdentifier)), moveInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsAttachedEvent(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier newWindowIdentifier, size_t newIndex)
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
        namespaceObject.tabs().onAttached().invokeListenersWithArgument(@(toWebAPI(tabIdentifier)), attachInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsActivatedEvent(WebExtensionTabIdentifier previousActiveTabIdentifier, WebExtensionTabIdentifier newActiveTabIdentifier, WebExtensionWindowIdentifier windowIdentifier)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onActivated

    auto *activateInfo = @{ previousTabIdKey: @(toWebAPI(previousActiveTabIdentifier)), tabIdKey: @(toWebAPI(newActiveTabIdentifier)), windowIdKey: @(toWebAPI(windowIdentifier)) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.tabs().onActivated().invokeListenersWithArgument(activateInfo);
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
        namespaceObject.tabs().onHighlighted().invokeListenersWithArgument(highlightInfo);
    });
}

void WebExtensionContextProxy::dispatchTabsRemovedEvent(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier windowIdentifier, WebExtensionContext::WindowIsClosing windowIsClosing)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs/onRemoved

    auto *removeInfo = @{ windowIdKey: @(toWebAPI(windowIdentifier)), isWindowClosingKey: @(windowIsClosing == WebExtensionContext::WindowIsClosing::Yes) };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.tabs().onRemoved().invokeListenersWithArgument(@(toWebAPI(tabIdentifier)), removeInfo);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
