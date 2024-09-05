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
#import "WebExtensionAPIAction.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPITabs.h"
#import "WebExtensionAPIWindows.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

static NSString * const tabIdKey = @"tabId";
static NSString * const windowIdKey = @"windowId";

static NSString * const imageDataKey = @"imageData";
static NSString * const pathKey = @"path";
static NSString * const popupKey = @"popup";
static NSString * const textKey = @"text";
static NSString * const titleKey = @"title";

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static NSString * const variantsKey = @"variants";
static NSString * const colorSchemesKey = @"color_schemes";
static NSString * const lightKey = @"light";
static NSString * const darkKey = @"dark";
static NSString * const anyKey = @"any";
#endif

namespace WebKit {

bool WebExtensionAPIAction::parseActionDetails(NSDictionary *details, std::optional<WebExtensionWindowIdentifier>& windowIdentifier, std::optional<WebExtensionTabIdentifier>& tabIdentifier, NSString **outExceptionString)
{
    static NSDictionary<NSString *, id> *types = @{
        tabIdKey: NSNumber.class,
        windowIdKey: NSNumber.class,
    };

    if (!validateDictionary(details, @"details", nil, types, outExceptionString))
        return false;

    if (details[tabIdKey] && details[windowIdKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'tabId' and 'windowID'");
        return false;
    }

    if (NSNumber *tabID = details[tabIdKey]) {
        tabIdentifier = toWebExtensionTabIdentifier(tabID.doubleValue);
        if (!isValid(tabIdentifier, outExceptionString))
            return false;
    }

    if (NSNumber *windowID = details[windowIdKey]) {
        windowIdentifier = toWebExtensionWindowIdentifier(windowID.doubleValue);
        if (!isValid(windowIdentifier, outExceptionString))
            return false;
    }

    return true;
}

void WebExtensionAPIAction::getTitle(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/getTitle

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionGetTitle(windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(result.value());
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::setTitle(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/setTitle

    static NSArray<NSString *> *requiredKeys = @[ titleKey ];

    static NSDictionary<NSString *, id> *types = @{
        titleKey: [NSOrderedSet orderedSetWithObjects:NSString.class, NSNull.class, nil],
    };

    if (!validateDictionary(details, @"details", requiredKeys, types, outExceptionString))
        return;

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    String title = nullString();
    if (NSString *string = objectForKey<NSString>(details, titleKey, false))
        title = string;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetTitle(windowIdentifier, tabIdentifier, title), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::getBadgeText(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/getBadgeText

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionGetBadgeText(windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(result.value());
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::setBadgeText(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/setBadgeText

    static NSArray<NSString *> *requiredKeys = @[ textKey ];

    static NSDictionary<NSString *, id> *types = @{
        textKey: [NSOrderedSet orderedSetWithObjects:NSString.class, NSNull.class, nil],
    };

    if (!validateDictionary(details, @"details", requiredKeys, types, outExceptionString))
        return;

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    String text = nullString();
    if (NSString *string = objectForKey<NSString>(details, textKey, false))
        text = string;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetBadgeText(windowIdentifier, tabIdentifier, text), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::getBadgeBackgroundColor(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/getBadgeBackgroundColor

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    // FIXME: <rdar://problem/57666368> Implement getting/setting the extension toolbar item's badge background color.

    callback->call(@[ @255, @0, @0, @255 ]);
}

void WebExtensionAPIAction::setBadgeBackgroundColor(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/setBadgeBackgroundColor

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    // FIXME: <rdar://problem/57666368> Implement getting/setting the extension toolbar item's badge background color.

    callback->call();
}

void WebExtensionAPIAction::enable(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/enable

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetEnabled(tabIdentifer, true), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::disable(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/disable

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetEnabled(tabIdentifer, false), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::isEnabled(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/isEnabled

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionGetEnabled(windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<bool, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(@(result.value()));
    }, extensionContext().identifier());
}

static NSString *dataURLFromImageData(JSValue *imageData, size_t *outWidth, NSString *sourceKey, NSString **outExceptionString)
{
    if (outWidth)
        *outWidth = 0;

    static NSString * const notImageDataError = @"it is not an ImageData object";

    auto *imageDataConstructor = imageData.context[@"ImageData"];
    if (![imageData isInstanceOf:imageDataConstructor]) {
        *outExceptionString = toErrorString(nil, sourceKey, notImageDataError);
        return nil;
    }

    auto *dataObject = imageData[@"data"];
    if (!dataObject.isObject) {
        *outExceptionString = toErrorString(nil, sourceKey, notImageDataError);
        return nil;
    }

    auto context = imageData.context.JSGlobalContextRef;
    auto dataObjectRef = JSValueToObject(context, dataObject.JSValueRef, nullptr);

    auto dataArrayType = JSValueGetTypedArrayType(context, dataObjectRef, nullptr);
    if (dataArrayType != kJSTypedArrayTypeUint8ClampedArray) {
        *outExceptionString = toErrorString(nil, sourceKey, notImageDataError);
        return nil;
    }

    auto *pixelData = [[NSData alloc] initWithBytes:JSObjectGetTypedArrayBytesPtr(context, dataObjectRef, nullptr) length:JSObjectGetTypedArrayByteLength(context, dataObjectRef, nullptr)];
    auto width = imageData[@"width"].toNumber.unsignedLongValue;
    auto height = imageData[@"height"].toNumber.unsignedLongValue;
    auto *colorSpaceString = imageData[@"colorSpace"].toString;

    ASSERT([colorSpaceString isEqualToString:@"srgb"] || [colorSpaceString isEqualToString:@"display-p3"]);

    constexpr size_t bitsPerComponent = 8;
    constexpr size_t bitsPerPixel = 32;
    const size_t bytesPerRow = 4 * width;
    auto colorSpace = CGColorSpaceCreateWithName([colorSpaceString isEqualToString:@"display-p3"] ? kCGColorSpaceDisplayP3 : kCGColorSpaceSRGB);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
    constexpr CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast;
#pragma clang diagnostic pop

    constexpr CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    constexpr CGFloat *decode = nullptr;
    constexpr bool shouldInterpolate = YES;

    auto dataProvider = CGDataProviderCreateWithCFData((__bridge CFDataRef)pixelData);
    auto cgImage = CGImageCreate(width, height, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpace, bitmapInfo, dataProvider, decode, shouldInterpolate, renderingIntent);
    CGDataProviderRelease(dataProvider);
    CGColorSpaceRelease(colorSpace);

    if (!cgImage) {
        *outExceptionString = toErrorString(nil, sourceKey, notImageDataError);
        return nil;
    }

#if USE(APPKIT)
    auto *imageRep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
    CGImageRelease(cgImage);

    auto *pngData = [imageRep representationUsingType:NSBitmapImageFileTypePNG properties:@{ }];
#else
    UIImage *image = [UIImage imageWithCGImage:cgImage];
    CGImageRelease(cgImage);

    auto *pngData = UIImagePNGRepresentation(image);
#endif

    if (!pngData.length) {
        *outExceptionString = toErrorString(nil, sourceKey, notImageDataError);
        return nil;
    }

    auto *base64String = [pngData base64EncodedStringWithOptions:0];
    if (!base64String.length) {
        *outExceptionString = toErrorString(nil, sourceKey, notImageDataError);
        return nil;
    }

    if (outWidth)
        *outWidth = width;

    return [NSString stringWithFormat:@"data:image/png;base64,%@", base64String];
}

bool WebExtensionAPIAction::isValidDimensionKey(NSString *dimension)
{
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if ([dimension isEqualToString:anyKey])
        return true;
#endif

    double value = dimension.doubleValue;
    if (!value)
        return false;

    if (!std::isfinite(value) || value <= 0 || value >= static_cast<double>(std::numeric_limits<size_t>::max()))
        return false;

    double integral;
    if (std::modf(value, &integral) != 0.0) {
        // Only integral numbers can be used.
        return false;
    }

    return true;
}

NSString *WebExtensionAPIAction::parseIconPath(NSString *path, const URL& baseURL)
{
    // Resolve paths as relative against the base URL, unless it is a data URL.
    if ([path hasPrefix:@"data:"])
        return path;
    return URL { baseURL, path }.path().toString();
}

NSMutableDictionary *WebExtensionAPIAction::parseIconPathsDictionary(NSDictionary *input, const URL& baseURL, bool forVariants, NSString *inputKey, NSString **outExceptionString)
{
    auto *result = [NSMutableDictionary dictionaryWithCapacity:input.count];

    for (NSString *key in input) {
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (forVariants && [key isEqualToString:colorSchemesKey])
            continue;
#endif

        if (!isValidDimensionKey(key)) {
            if (outExceptionString)
                *outExceptionString = toErrorString(nullptr, inputKey, @"'%@' is not a valid dimension", key);
            return nil;
        }

        NSString *path = input[key];
        if (!validateObject(path, [NSString stringWithFormat:@"%@[%@]", inputKey, key], NSString.class, outExceptionString))
            return nil;

        result[key] = parseIconPath(path, baseURL);
    }

    return result;
}

NSMutableDictionary *WebExtensionAPIAction::parseIconImageDataDictionary(NSDictionary *input, bool forVariants, NSString *inputKey, NSString **outExceptionString)
{
    auto *result = [NSMutableDictionary dictionaryWithCapacity:input.count];

    for (NSString *key in input) {
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (forVariants && [key isEqualToString:colorSchemesKey])
            continue;
#endif

        if (!isValidDimensionKey(key)) {
            if (outExceptionString)
                *outExceptionString = toErrorString(nullptr, inputKey, @"'%@' is not a valid dimension", key);
            return nil;
        }

        id value = input[key];
        if (!validateObject(value, [NSString stringWithFormat:@"%@[%@]", inputKey, key], JSValue.class, outExceptionString))
            return nil;

        auto *dataURLString = dataURLFromImageData(value, nullptr, key, outExceptionString);
        if (!dataURLString)
            return nil;

        result[key] = dataURLString;
    }

    return result;
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
NSArray *WebExtensionAPIAction::parseIconVariants(NSArray *input, const URL& baseURL, NSString *inputKey, NSString **outExceptionString)
{
    auto *result = [NSMutableArray arrayWithCapacity:input.count];

    NSString *firstExceptionString;
    for (NSUInteger index = 0; index < input.count; ++index) {
        NSDictionary *dictionary = input[index];
        auto *compositeKey = [NSString stringWithFormat:@"%@[%lu]", inputKey, index];

        // Try parsing the variant as image data first.
        auto *parsedDictionary = parseIconImageDataDictionary(dictionary, true, compositeKey, !firstExceptionString ? &firstExceptionString : nullptr);

        // If image data failed, try parsing as paths.
        if (!parsedDictionary)
            parsedDictionary = parseIconPathsDictionary(dictionary, baseURL, true, compositeKey, !firstExceptionString ? &firstExceptionString : nullptr);

        // If all types failed, continue.
        if (!parsedDictionary) {
            ASSERT(firstExceptionString);
            continue;
        }

        if (NSArray *colorSchemes = dictionary[colorSchemesKey]) {
            auto *colorSchemesCompositeKey = [NSString stringWithFormat:@"%@['%@']", compositeKey, colorSchemesKey];
            if (!validateObject(colorSchemes, colorSchemesCompositeKey, @[ NSString.class ], !firstExceptionString ? &firstExceptionString : nullptr))
                continue;

            if (![colorSchemes containsObject:lightKey] && ![colorSchemes containsObject:darkKey]) {
                if (!firstExceptionString)
                    firstExceptionString = toErrorString(nil, colorSchemesCompositeKey, @"it must specify either 'light' or 'dark'");
                continue;
            }

            parsedDictionary[colorSchemesKey] = colorSchemes;
        }

        ASSERT(parsedDictionary);
        [result addObject:parsedDictionary];
    }

    if (input.count && !result.count) {
        // An exception is only set if no valid icon variants were found,
        // maintaining flexibility for future support of different inputs.
        if (!firstExceptionString)
            firstExceptionString = toErrorString(nil, inputKey, @"it didn't contain any valid icon variants");
        if (outExceptionString)
            *outExceptionString = firstExceptionString;
        return nil;
    }

    return [result copy];
}
#endif // ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)

void WebExtensionAPIAction::setIcon(WebFrame& frame, NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/setIcon
    // Icon Variants: https://github.com/w3c/webextensions/blob/main/proposals/dark_mode_extension_icons.md

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    static NSDictionary<NSString *, id> *types = @{
        pathKey: [NSOrderedSet orderedSetWithObjects:NSString.class, NSDictionary.class, NSNull.class, nil],
        imageDataKey: [NSOrderedSet orderedSetWithObjects:JSValue.class, NSDictionary.class, NSNull.class, nil],
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        variantsKey: [NSOrderedSet orderedSetWithObjects:@[ NSDictionary.class ], NSNull.class, nil],
#endif
    };

    if (!validateDictionary(details, @"details", nil, types, outExceptionString))
        return;

    if (details[pathKey] && details[imageDataKey]) {
        *outExceptionString = toErrorString(nil, @"details", @"it cannot specify both 'path' and 'imageData'");
        return;
    }

    NSDictionary *iconDictionary;

    if (auto *imageData = objectForKey<JSValue>(details, imageDataKey)) {
        size_t width;
        auto *dataURLString = dataURLFromImageData(imageData, &width, imageDataKey, outExceptionString);
        if (!dataURLString)
            return;

        iconDictionary = @{ @(width).stringValue: dataURLString };
    }

    if (auto *images = objectForKey<NSDictionary>(details, imageDataKey)) {
        iconDictionary = parseIconImageDataDictionary(images, false, imageDataKey, outExceptionString);
        if (!iconDictionary)
            return;
    }

    if (auto *path = objectForKey<NSString>(details, pathKey)) {
        // Chrome documentation states that 'details.path = foo' is equivalent to 'details.path = { '16': foo }'.
        // Documentation: https://developer.chrome.com/docs/extensions/reference/action/#method-setIcon
        iconDictionary = @{ @"16": parseIconPath(path, frame.url()) };
    }

    if (auto *paths = objectForKey<NSDictionary>(details, pathKey)) {
        iconDictionary = parseIconPathsDictionary(paths, frame.url(), false, pathKey, outExceptionString);
        if (!iconDictionary)
            return;
    }

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    NSArray *iconVariants;
    if (auto *variants = objectForKey<NSArray>(details, variantsKey)) {
        iconVariants = parseIconVariants(variants, frame.url(), variantsKey, outExceptionString);
        if (!iconVariants)
            return;
    }

    auto *iconsJSON = encodeJSONString(iconVariants ?: iconDictionary, JSONOptions::FragmentsAllowed);
#else
    auto *iconsJSON = encodeJSONString(iconDictionary);
#endif

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetIcon(windowIdentifier, tabIdentifier, iconsJSON), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::getPopup(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/getPopup

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionGetPopup(windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<String, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call(result.value());
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::setPopup(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/setPopup

    static NSArray<NSString *> *requiredKeys = @[ popupKey ];

    static NSDictionary<NSString *, id> *types = @{
        popupKey: [NSOrderedSet orderedSetWithObjects:NSString.class, NSNull.class, nil],
    };

    if (!validateDictionary(details, @"details", requiredKeys, types, outExceptionString))
        return;

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    String popup = nullString();
    if (NSString *string = objectForKey<NSString>(details, popupKey, false))
        popup = string;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetPopup(windowIdentifier, tabIdentifier, popup), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIAction::openPopup(WebPage& page, NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/openPopup

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionOpenPopup(page.webPageProxyIdentifier(), windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPIAction::onClicked()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/onClicked

    if (!m_onClicked)
        m_onClicked = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::ActionOnClicked);

    return *m_onClicked;
}

void WebExtensionContextProxy::dispatchActionClickedEvent(const std::optional<WebExtensionTabParameters>& tabParameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/onClicked

    auto *tab = tabParameters ? toWebAPI(tabParameters.value()) : nil;

    enumerateFramesAndNamespaceObjects([&](auto& frame, auto& namespaceObject) {
        RefPtr coreFrame = frame.protectedCoreLocalFrame();
        WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, coreFrame ? coreFrame->document() : nullptr);
        namespaceObject.action().onClicked().invokeListenersWithArgument(tab);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
