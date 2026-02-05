/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKWebProcessPlugInFrameInternal.h"

#import "APIJSHandle.h"
#import "FrameInfoData.h"
#import "WKNSArray.h"
#import "WKNSURLExtras.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import "WKWebProcessPlugInCSSStyleDeclarationHandleInternal.h"
#import "WKWebProcessPlugInHitTestResultInternal.h"
#import "WKWebProcessPlugInNodeHandleInternal.h"
#import "WKWebProcessPlugInRangeHandleInternal.h"
#import "WKWebProcessPlugInScriptWorldInternal.h"
#import "WebProcess.h"
#import "_WKFrameHandleInternal.h"
#import "_WKJSHandleInternal.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSGlobalObject.h>
#import <JavaScriptCore/JSValue.h>
#import <WebCore/CertificateInfo.h>
#import <WebCore/DOMWrapperWorld.h>
#import <WebCore/DocumentInlines.h>
#import <WebCore/DocumentSecurityOrigin.h>
#import <WebCore/IntPoint.h>
#import <WebCore/JSWebKitJSHandle.h>
#import <WebCore/LinkIconCollector.h>
#import <WebCore/LinkIconType.h>
#import <WebCore/LocalFrameInlines.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/AlignedStorage.h>
#import <wtf/cocoa/VectorCocoa.h>

@implementation WKWebProcessPlugInFrame {
    AlignedStorage<WebKit::WebFrame> _frame;
}

static Ref<WebKit::WebFrame> protectedFrame(WKWebProcessPlugInFrame *frame)
{
    return *frame->_frame;
}

+ (instancetype)lookUpFrameFromHandle:(_WKFrameHandle *)handle
{
    auto frameID = handle->_frameHandle->frameID();
    return wrapper(frameID ? WebKit::WebProcess::singleton().webFrame(*frameID) : nullptr);
}

+ (instancetype)lookUpFrameFromJSContext:(JSContext *)context
{
    return wrapper(WebKit::WebFrame::frameForContext(context.JSGlobalContextRef)).autorelease();
}

+ (instancetype)lookUpContentFrameFromWindowOrFrameElement:(JSValue *)value
{
    return wrapper(WebKit::WebFrame::contentFrameForWindowOrFrameElement(value.context.JSGlobalContextRef, value.JSValueRef)).autorelease();
}

// FIXME: Remove this once it is no longer helpful to help Safari transition away from the injected bundle.
+ (_WKJSHandle *)jsHandleFromValue:(JSValue *)value withContext:(JSContext *)context
{
#if PLATFORM(MAC)
    RELEASE_ASSERT(WTF::MacApplication::isSafari() || applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
#else
    RELEASE_ASSERT(WTF::IOSApplication::isMobileSafari() || applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s);
#endif
    JSObjectRef object = JSValueToObject(context.JSGlobalContextRef, value.JSValueRef, 0);
    JSC::JSGlobalObject* globalObject = ::toJS(context.JSGlobalContextRef);
    JSC::JSObject* jsObject = ::toJS(globalObject, object).toObject(globalObject);

    if (auto* info = jsDynamicCast<WebCore::JSWebKitJSHandle*>(jsObject)) {
        RELEASE_ASSERT(globalObject->template inherits<WebCore::JSDOMGlobalObject>());
        auto* domGlobalObject = jsCast<WebCore::JSDOMGlobalObject*>(globalObject);
        RefPtr document = dynamicDowncast<WebCore::Document>(domGlobalObject->scriptExecutionContext());
        RefPtr frame = WebKit::WebFrame::webFrame(document->frameID());
        RefPtr world = WebKit::InjectedBundleScriptWorld::get(Ref { domGlobalObject->world() });
        Ref ref { info->wrapped() };
        WebKit::JSHandleInfo handleInfo { ref->identifier(), world->identifier(), frame->info(), ref->windowFrameIdentifier() };
        return wrapper(API::JSHandle::create(WTF::move(handleInfo))).autorelease();
    }
    return nil;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKWebProcessPlugInFrame.class, self))
        return;
    SUPPRESS_UNCOUNTED_ARG _frame->~WebFrame();
    [super dealloc];
}

- (JSContext *)jsContextForWorld:(WKWebProcessPlugInScriptWorld *)world
{
    return [JSContext contextWithJSGlobalContextRef:protectedFrame(self)->jsContextForWorld(Ref { [world _scriptWorld] }.ptr())];
}

- (JSContext *)jsContextForServiceWorkerWorld:(WKWebProcessPlugInScriptWorld *)world
{
    if (auto context = protectedFrame(self)->jsContextForServiceWorkerWorld(Ref { [world _scriptWorld] }.ptr()))
        return [JSContext contextWithJSGlobalContextRef:context];
    return nil;
}

- (WKWebProcessPlugInHitTestResult *)hitTest:(CGPoint)point
{
    return wrapper(protectedFrame(self)->hitTest(WebCore::IntPoint(point))).autorelease();
}

- (WKWebProcessPlugInHitTestResult *)hitTest:(CGPoint)point options:(WKHitTestOptions)options
{
    auto types = WebKit::WebFrame::defaultHitTestRequestTypes();
    if (options & WKHitTestOptionAllowUserAgentShadowRootContent)
        types.remove(WebCore::HitTestRequest::Type::DisallowUserAgentShadowContent);
    return wrapper(protectedFrame(self)->hitTest(WebCore::IntPoint(point), types)).autorelease();
}

- (JSValue *)jsCSSStyleDeclarationForCSSStyleDeclarationHandle:(WKWebProcessPlugInCSSStyleDeclarationHandle *)cssStyleDeclarationHandle inWorld:(WKWebProcessPlugInScriptWorld *)world
{
    JSValueRef valueRef = protectedFrame(self)->jsWrapperForWorld(Ref { [cssStyleDeclarationHandle _cssStyleDeclarationHandle] }.ptr(), Ref { [world _scriptWorld] }.ptr());
    return [JSValue valueWithJSValueRef:valueRef inContext:retainPtr([self jsContextForWorld:world]).get()];
}

- (JSValue *)jsNodeForNodeHandle:(WKWebProcessPlugInNodeHandle *)nodeHandle inWorld:(WKWebProcessPlugInScriptWorld *)world
{
    JSValueRef valueRef = protectedFrame(self)->jsWrapperForWorld(Ref { [nodeHandle _nodeHandle] }.ptr(), Ref { [world _scriptWorld] }.ptr());
    return [JSValue valueWithJSValueRef:valueRef inContext:retainPtr([self jsContextForWorld:world]).get()];
}

- (JSValue *)jsRangeForRangeHandle:(WKWebProcessPlugInRangeHandle *)rangeHandle inWorld:(WKWebProcessPlugInScriptWorld *)world
{
    JSValueRef valueRef = protectedFrame(self)->jsWrapperForWorld(Ref { [rangeHandle _rangeHandle] }.ptr(), Ref { [world _scriptWorld] }.ptr());
    return [JSValue valueWithJSValueRef:valueRef inContext:retainPtr([self jsContextForWorld:world]).get()];
}

- (WKWebProcessPlugInBrowserContextController *)_browserContextController
{
    Ref frame = *_frame;
    if (!frame->page())
        return nil;
    return WebKit::wrapper(*frame->page());
}

- (NSURL *)URL
{
    return protectedFrame(self)->url().createNSURL().autorelease();
}

- (NSArray *)childFrames
{
    return WebKit::wrapper(protectedFrame(self)->childFrames()).autorelease();
}

- (BOOL)containsAnyFormElements
{
    return !!protectedFrame(self)->containsAnyFormElements();
}

- (BOOL)isMainFrame
{
    return !!protectedFrame(self)->isMainFrame();
}

- (_WKFrameHandle *)handle
{
    return wrapper(API::FrameHandle::create(_frame->frameID())).autorelease();
}

- (NSString *)_securityOrigin
{
    RefPtr coreFrame = protectedFrame(self)->coreLocalFrame();
    if (!coreFrame)
        return nil;
    return protect(protect(coreFrame->document())->securityOrigin())->toString().createNSString().autorelease();
}

static RetainPtr<NSArray> collectIcons(WebCore::LocalFrame* frame, OptionSet<WebCore::LinkIconType> iconTypes)
{
    if (!frame)
        return @[];
    RefPtr document = frame->document();
    if (!document)
        return @[];
    return createNSArray(WebCore::LinkIconCollector(*document).iconsOfTypes(iconTypes), [] (auto&& icon) {
        return icon.url.createNSURL();
    });
}

- (NSArray *)appleTouchIconURLs
{
    return collectIcons(protect(protectedFrame(self)->coreLocalFrame()).get(), { WebCore::LinkIconType::TouchIcon, WebCore::LinkIconType::TouchPrecomposedIcon }).autorelease();
}

- (NSArray *)faviconURLs
{
    return collectIcons(protect(protectedFrame(self)->coreLocalFrame()).get(), WebCore::LinkIconType::Favicon).autorelease();
}

- (WKWebProcessPlugInFrame *)_parentFrame
{
    return wrapper(protectedFrame(self)->parentFrame()).autorelease();
}

- (BOOL)_hasCustomContentProvider
{
    Ref frame = *_frame;
    if (!frame->isMainFrame())
        return false;

    return protect(frame->page())->mainFrameHasCustomContentProvider();
}

- (NSArray *)_certificateChain
{
    return (NSArray *)WebCore::CertificateInfo::certificateChainFromSecTrust(protectedFrame(self)->certificateInfo().trust().get()).autorelease();
}

- (SecTrustRef)_serverTrust
{
    return protectedFrame(self)->certificateInfo().trust().get();
}

- (NSURL *)_provisionalURL
{
    return [NSURL _web_URLWithWTFString:protectedFrame(self)->provisionalURL()];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_frame;
}

@end
