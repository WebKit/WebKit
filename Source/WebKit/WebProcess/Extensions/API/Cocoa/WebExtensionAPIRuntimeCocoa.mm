/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "WebExtensionAPIRuntime.h"
#import "WebExtensionAPIKeys.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPIEvent.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPIPort.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionControllerProxy.h"
#import "WebExtensionFrameIdentifier.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionMessageTargetParameters.h"
#import "WebExtensionUtilities.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/LocalFrameInlines.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/UserGestureIndicator.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/text/MakeString.h>


namespace WebKit {

enum class IsDefaultReply : bool { No, Yes };
using ReplyCallbackAggregator = EagerCallbackAggregator<void(id, IsDefaultReply)>;

}

@interface _WKReplyCallbackAggregator : NSObject

- (instancetype)initWithAggregator:(WebKit::ReplyCallbackAggregator&)aggregator;

@property (nonatomic, readonly) WebKit::ReplyCallbackAggregator& aggregator;

@end

@implementation _WKReplyCallbackAggregator {
    RefPtr<WebKit::ReplyCallbackAggregator> _aggregator;
}

- (instancetype)initWithAggregator:(WebKit::ReplyCallbackAggregator&)aggregator
{
    if (!(self = [super init]))
        return nil;

    _aggregator = aggregator;

    return self;
}

- (WebKit::ReplyCallbackAggregator&)aggregator
{
    return *_aggregator;
}

@end

namespace WebKit {

JSValueRef toWebAPI(JSContextRef context, const WebExtensionMessageSenderParameters& parameters)
{
    JSObjectRef result = JSObjectMake(context, 0, 0);

    if (parameters.extensionUniqueIdentifier)
        JSObjectSetProperty(context, result, toJSString(idKey).get(), toJSValueRef(context, parameters.extensionUniqueIdentifier.value()), 0, nullptr);

    if (parameters.tabParameters)
        JSObjectSetProperty(context, result, toJSString(tabKey).get(), toJSValueRef(context, toWebAPI(parameters.tabParameters.value())), 0, nullptr);

    // The frame identifier is only included when tab is included.
    if (parameters.frameIdentifier && parameters.tabParameters)
        JSObjectSetProperty(context, result, toJSString(frameIdKey).get(), JSValueMakeNumber(context, toWebAPI(parameters.frameIdentifier.value())), 0, nullptr);

    if (parameters.url.isValid()) {
        JSObjectSetProperty(context, result, toJSString(urlKey).get(), toJSValueRef(context, parameters.url.string()), 0, nullptr);
        JSObjectSetProperty(context, result, toJSString(originKey).get(), toJSValueRef(context, WebCore::SecurityOrigin::create(parameters.url)->toString()), 0, nullptr);
    }

    if (parameters.documentIdentifier.isValid())
        JSObjectSetProperty(context, result, toJSString(documentIdKey).get(), toJSValueRef(context, parameters.documentIdentifier.toString()), 0, nullptr);

    return result;
}

void WebExtensionContextProxy::internalDispatchRuntimeMessageEvent(WebExtensionContentWorldType contentWorldType, const String& messageJSON, const std::optional<WebExtensionMessageTargetParameters>& targetParameters, const WebExtensionMessageSenderParameters& senderParameters, bool userGesture, CompletionHandler<void(String&& replyJSON)>&& completionHandler)
{
    if (!hasDOMWrapperWorld(contentWorldType)) {
        // A null reply to the completionHandler means no listeners replied.
        completionHandler({ });
        return;
    }

    id message = parseJSON(messageJSON.createNSString().get(), JSONOptions::FragmentsAllowed);
    auto sourceContentWorldType = senderParameters.contentWorldType;

    auto callbackAggregator = ReplyCallbackAggregator::create([completionHandler = WTF::move(completionHandler)](JSValue *replyMessage, IsDefaultReply defaultReply) mutable {
        if (defaultReply == IsDefaultReply::Yes) {
            // A null reply to the completionHandler means no listeners replied.
            completionHandler({ });
            return;
        }

        auto *replyMessageJSON = encodeJSONString(replyMessage, JSONOptions::FragmentsAllowed);
        if (replyMessageJSON.length > webExtensionMaxMessageLength)
            replyMessageJSON = @"";

        // Ensure a real reply is never null, so the completionHandler can make the distinction.
        if (!replyMessageJSON)
            replyMessageJSON = @"";

        completionHandler(replyMessageJSON);
    }, nil, IsDefaultReply::Yes);

    // This ObjC wrapper is need for the inner reply block, which is required to be a compiled block.
    auto *callbackAggregatorWrapper = [[_WKReplyCallbackAggregator alloc] initWithAggregator:callbackAggregator];

    bool anyListenerHandledMessage = false;
    enumerateFramesAndNamespaceObjects([&, callbackAggregatorWrapper = RetainPtr { callbackAggregatorWrapper }](WebFrame& frame, WebExtensionAPINamespace& namespaceObject) {
        // Don't send the message to any listeners in the sender's page.
        if (senderParameters.pageProxyIdentifier == frame.page()->webPageProxyIdentifier())
            return;

        // Skip all frames that don't match the target parameters.
        if (!matchesTarget(frame, targetParameters))
            return;

        WebExtensionAPIEvent::ListenerVector listeners;
        if (sourceContentWorldType == WebExtensionContentWorldType::WebPage)
            listeners = protect(namespaceObject.runtime())->onMessageExternal().listeners();
        else
            listeners = protect(namespaceObject.runtime())->onMessage().listeners();

        if (listeners.isEmpty())
            return;

        std::optional<WebCore::UserGestureIndicator> gestureIndicator;
        if (userGesture) {
            RefPtr coreFrame = frame.coreLocalFrame();
            gestureIndicator.emplace(WebCore::IsProcessingUserGesture::Yes, protect(coreFrame ? coreFrame->document() : nullptr));
        }

        for (auto& listener : listeners) {
            // Using BlockPtr for this call does not work, since JSValue needs a compiled block
            // with a signature to translate the JS function arguments. Having the block capture
            // callbackAggregatorWrapper ensures that callbackAggregator remains in scope.
            auto senderInfo = toWebAPI(listener->globalContext(), senderParameters);
            auto returnValue = listener->call(toJSValueRef(listener->globalContext(), message), senderInfo, toJSValueRef(listener->globalContext(), ^(JSValue *replyMessage) {
                protect(callbackAggregatorWrapper.get().aggregator).get()(replyMessage, IsDefaultReply::No);
            }));

            if (JSValueIsBoolean(listener->globalContext(), returnValue) && JSValueToBoolean(listener->globalContext(), returnValue)) {
                anyListenerHandledMessage = true;
                continue;
            }

            JSValue *value = toJSValue(listener->globalContext(), returnValue);
            if (!isThenable(value.context.JSGlobalContextRef, value.JSValueRef))
                continue;

            anyListenerHandledMessage = true;

            auto resolveBlock = ^(JSValue *replyMessage) {
                protect(callbackAggregatorWrapper.get().aggregator).get()(replyMessage, IsDefaultReply::No);
            };

            auto rejectBlock = ^(JSValue *error) {
                return;
            };

            [value invokeMethod:@"then" withArguments:@[ resolveBlock, rejectBlock ]];
        }
    }, toDOMWrapperWorld(contentWorldType));

    if (!anyListenerHandledMessage)
        callbackAggregator.get()(nil, IsDefaultReply::Yes);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
