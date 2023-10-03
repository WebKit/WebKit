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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionGetTitle(windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> title, std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        ASSERT(title);

        callback->call((NSString *)title.value());
    }, extensionContext().identifier().toUInt64());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetTitle(windowIdentifier, tabIdentifier, title), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIAction::getBadgeText(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/getBadgeText

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionGetBadgeText(windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> badgeText, std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        ASSERT(badgeText);

        callback->call((NSString *)badgeText.value());
    }, extensionContext().identifier().toUInt64());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetBadgeText(windowIdentifier, tabIdentifier, text), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        callback->call();
    }, extensionContext().identifier().toUInt64());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetEnabled(tabIdentifer, true), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIAction::disable(double tabID, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/disable

    auto tabIdentifer = toWebExtensionTabIdentifier(tabID);
    if (tabIdentifer && !isValid(tabIdentifer, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetEnabled(tabIdentifer, false), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIAction::isEnabled(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/isEnabled

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionGetEnabled(windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<bool> enabled, std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        ASSERT(enabled);

        callback->call(@(enabled.value()));
    }, extensionContext().identifier().toUInt64());
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

static bool isValidDimensionKey(NSString *dimension)
{
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

void WebExtensionAPIAction::setIcon(JSContextRef, NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/setIcon

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    static NSDictionary<NSString *, id> *types = @{
        pathKey: [NSOrderedSet orderedSetWithObjects:NSString.class, NSDictionary.class, NSNull.class, nil],
        imageDataKey: [NSOrderedSet orderedSetWithObjects:JSValue.class, NSDictionary.class, NSNull.class, nil],
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
        auto *mutableIconDictionary = [NSMutableDictionary dictionaryWithCapacity:images.count];

        for (NSString *key in images) {
            if (!isValidDimensionKey(key)) {
                *outExceptionString = toErrorString(nil, imageDataKey, @"'%@' in not a valid dimension", key);
                return;
            }

            if (!validateObject(images[key], key, JSValue.class, outExceptionString))
                return;

            JSValue *imageData = images[key];
            auto *dataURLString = dataURLFromImageData(imageData, nullptr, key, outExceptionString);
            if (!dataURLString)
                return;

            mutableIconDictionary[key] = dataURLString;
        }

        iconDictionary = [mutableIconDictionary copy];
    }

    if (auto *path = objectForKey<NSString>(details, pathKey)) {
        // Chrome documentation states that 'details.path = foo' is equivalent to 'details.path = { '16': foo }'.
        // Documentation: https://developer.chrome.com/docs/extensions/reference/action/#method-setIcon
        iconDictionary = @{ @"16": path };
    }

    if (auto *paths = objectForKey<NSDictionary>(details, pathKey)) {
        for (NSString *key in paths) {
            if (!isValidDimensionKey(key)) {
                *outExceptionString = toErrorString(nil, imageDataKey, @"'%@' in not a valid dimension", key);
                return;
            }

            if (!validateObject(paths[key], key, NSString.class, outExceptionString))
                return;
        }

        iconDictionary = paths;
    }

    auto *iconDictionaryJSON = encodeJSONString(iconDictionary);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetIcon(windowIdentifier, tabIdentifier, iconDictionaryJSON), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIAction::getPopup(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/getPopup

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionGetPopup(windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> popupPath, std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        ASSERT(popupPath);

        callback->call((NSString *)popupPath.value());
    }, extensionContext().identifier().toUInt64());
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

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionSetPopup(windowIdentifier, tabIdentifier, popup), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIAction::openPopup(WebPage* page, NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/openPopup

    std::optional<WebExtensionWindowIdentifier> windowIdentifier;
    std::optional<WebExtensionTabIdentifier> tabIdentifier;
    if (!parseActionDetails(details, windowIdentifier, tabIdentifier, outExceptionString))
        return;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ActionOpenPopup(page->webPageProxyIdentifier(), windowIdentifier, tabIdentifier), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<String> error) {
        if (error) {
            callback->reportError(error.value());
            return;
        }

        callback->call();
    }, extensionContext().identifier().toUInt64());
}

WebExtensionAPIEvent& WebExtensionAPIAction::onClicked()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/onClicked

    if (!m_onClicked)
        m_onClicked = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::ActionOnClicked);

    return *m_onClicked;
}

void WebExtensionContextProxy::dispatchActionClickedEvent(const std::optional<WebExtensionTabParameters>& tabParameters)
{
    auto *tab = tabParameters ? toWebAPI(tabParameters.value()) : nil;

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action/onClicked
        namespaceObject.action().onClicked().invokeListenersWithArgument(tab);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
