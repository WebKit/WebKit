/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "ClassMethodSwizzler.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Scope.h>

static NSPoint s_overrideFullscreenScreenMouseLocation;

@interface NSWindow (FullscreenPointerLeaveTestSwizzler)
- (NSPoint)_fullscreenTest_mouseLocationOutsideOfEventStream;
@end

@implementation NSWindow (FullscreenPointerLeaveTestSwizzler)
- (NSPoint)_fullscreenTest_mouseLocationOutsideOfEventStream
{
    return [self convertPointFromScreen:s_overrideFullscreenScreenMouseLocation];
}
@end

@interface NSEvent (FullscreenPointerLeaveTestSwizzler)
+ (NSPoint)_fullscreenTest_mouseLocation;
@end

@implementation NSEvent (FullscreenPointerLeaveTestSwizzler)
+ (NSPoint)_fullscreenTest_mouseLocation
{
    return s_overrideFullscreenScreenMouseLocation;
}
@end

@interface FullscreenPointerLeaveTestDelegate : NSObject <_WKFullscreenDelegate>
- (bool)waitForDidEnterFullscreen;
- (bool)waitForDidExitFullscreen;
@end

@implementation FullscreenPointerLeaveTestDelegate {
    bool _didEnterFullscreen;
    bool _didExitFullscreen;
}

- (bool)waitForDidEnterFullscreen
{
    bool result = TestWebKitAPI::Util::runFor(&_didEnterFullscreen, 5_s);
    _didEnterFullscreen = false;
    return result;
}

- (bool)waitForDidExitFullscreen
{
    bool result = TestWebKitAPI::Util::runFor(&_didExitFullscreen, 5_s);
    _didExitFullscreen = false;
    return result;
}

- (void)_webViewDidEnterFullscreen:(NSView *)webView
{
    _didEnterFullscreen = true;
}

- (void)_webViewDidExitFullscreen:(NSView *)webView
{
    _didExitFullscreen = true;
}

@end

namespace TestWebKitAPI {

TEST(Fullscreen, PointerLeave)
{
    s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    auto resetOverride = makeScopeExit([] {
        s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    });
    InstanceMethodSwizzler windowSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_fullscreenTest_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler eventSwizzler([NSEvent class], @selector(mouseLocation), [NSEvent methodForSelector:@selector(_fullscreenTest_mouseLocation)]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    [webView _setWindowOcclusionDetectionEnabled:NO];

    bool pointerenter = false;
    bool pointerleave = false;
    bool fullscreenchange = false;
    [webView performAfterReceivingMessage:@"pointerenter" action:[&] { pointerenter = true; }];
    [webView performAfterReceivingMessage:@"pointerleave" action:[&] { pointerleave = true; }];
    [webView performAfterReceivingMessage:@"webkitfullscreenchange" action:[&] { fullscreenchange = true; }];
    [webView synchronouslyLoadHTMLString:
        @"<!DOCTYPE html>"
        @"<style>#target { width:100px; height: 100px; }</style>"
        @"<div id=target></div>"
        @"<script>"
        @"let eventToMessage = event => { window.webkit.messageHandlers.testHandler.postMessage(event.type); };"
        @"target.addEventListener('pointerenter', eventToMessage, {once:true});"
        @"target.addEventListener('pointerleave', eventToMessage, {once:true});"
        @"document.addEventListener('webkitfullscreenchange', eventToMessage, {once:true});"
        @"</script>"];

    RetainPtr fullscreenDelegate = adoptNS([[FullscreenPointerLeaveTestDelegate alloc] init]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    NSRect screenFrame = [[[webView window] screen] ?: [NSScreen mainScreen] frame];
    [[webView window] setFrame:NSMakeRect(screenFrame.origin.x + 100, screenFrame.origin.y + 100, 300, 300) display:YES];

    NSPoint hoverPointInWindow = NSMakePoint(50, 250);
    s_overrideFullscreenScreenMouseLocation = [[webView window] convertPointToScreen:hoverPointInWindow];
    [webView mouseMoveToPoint:hoverPointInWindow withFlags:0];
    EXPECT_TRUE(Util::runFor(&pointerenter, 5_s));

    // Place stationary cursor outside the fullscreen window so #target (which expands to 100%x100% via :fullscreen) deterministically receives pointerleave.
    s_overrideFullscreenScreenMouseLocation = NSMakePoint(screenFrame.origin.x - 50, screenFrame.origin.y + 200);

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.webkitRequestFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidEnterFullscreen]);
    EXPECT_TRUE(Util::runFor(&fullscreenchange, 5_s));
    EXPECT_TRUE(Util::runFor(&pointerleave, 5_s));

    [webView objectByEvaluatingJavaScript:@"document.webkitExitFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidExitFullscreen]);
}

TEST(Fullscreen, PointerLeaveOnButtonMoveInFullscreen)
{
    s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    auto resetOverride = makeScopeExit([] {
        s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    });
    InstanceMethodSwizzler windowMouseLocationSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_fullscreenTest_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler eventMouseLocationSwizzler([NSEvent class], @selector(mouseLocation), [NSEvent methodForSelector:@selector(_fullscreenTest_mouseLocation)]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300) configuration:configuration.get() addToWindow:YES]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    RetainPtr initialWindow = [webView window];
    RetainPtr screen = [initialWindow screen] ?: [NSScreen mainScreen];
    NSRect screenFrame = [screen frame];
    [initialWindow setFrame:NSMakeRect(screenFrame.origin.x + 120, screenFrame.origin.y + 140, 460, 400) display:YES];
    [webView setFrame:NSMakeRect(30, 20, 400, 300)];

    RetainPtr fullscreenDelegate = adoptNS([[FullscreenPointerLeaveTestDelegate alloc] init]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    bool pointerenter = false;
    bool pointerleave = false;
    bool mouseleave = false;
    [webView performAfterReceivingMessage:@"pointerenter" action:[&] { pointerenter = true; }];
    [webView performAfterReceivingMessage:@"pointerleave" action:[&] { pointerleave = true; }];
    [webView performAfterReceivingMessage:@"mouseleave" action:[&] { mouseleave = true; }];
    [webView synchronouslyLoadHTMLString:
        @"<!DOCTYPE html>"
        @"<style>"
        @"body, html { margin: 0; width: 100%; height: 100%; }"
        @"#player { position: relative; width: 300px; height: 200px; background: #111; }"
        @"#player:-webkit-full-screen { width: 100%; height: 100%; }"
        @"#fsButton { position: absolute; right: 10px; bottom: 10px; width: 60px; height: 40px; }"
        @"</style>"
        @"<div id=player><video></video><button id=fsButton>FS</button></div>"
        @"<script>"
        @"let mousemoveCount = 0;"
        @"let pointermoveCount = 0;"
        @"document.addEventListener('mousemove', () => { mousemoveCount++; });"
        @"document.addEventListener('pointermove', () => { pointermoveCount++; });"
        @"let eventToMessage = event => { window.webkit.messageHandlers.testHandler.postMessage(event.type); };"
        @"fsButton.addEventListener('pointerenter', eventToMessage, {once:true});"
        @"fsButton.addEventListener('pointerleave', eventToMessage, {once:true});"
        @"fsButton.addEventListener('mouseleave', eventToMessage, {once:true});"
        @"</script>"];

    NSPoint buttonPointInWindow = [webView convertPoint:NSMakePoint(260, 170) toView:nil];
    s_overrideFullscreenScreenMouseLocation = [initialWindow convertPointToScreen:buttonPointInWindow];

    [webView mouseMoveToPoint:buttonPointInWindow withFlags:0];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE(Util::runFor(&pointerenter, 5_s));
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"fsButton.matches(':hover')"] boolValue]);

    [webView objectByEvaluatingJavaScript:@"mousemoveCount = 0; pointermoveCount = 0;"];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"player.webkitRequestFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidEnterFullscreen]);
    EXPECT_TRUE(Util::runFor(&pointerleave, 5_s));
    EXPECT_TRUE(Util::runFor(&mouseleave, 5_s));
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"fsButton.matches(':hover')"] boolValue]);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"mousemoveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"pointermoveCount"] intValue], 0);

    [webView objectByEvaluatingJavaScript:@"document.webkitExitFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidExitFullscreen]);
}

TEST(Fullscreen, PointerLeaveOnFullBleedPlayerWithObscuredContentInsets)
{
    s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    auto resetOverride = makeScopeExit([] {
        s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    });
    InstanceMethodSwizzler windowMouseLocationSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_fullscreenTest_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler eventMouseLocationSwizzler([NSEvent class], @selector(mouseLocation), [NSEvent methodForSelector:@selector(_fullscreenTest_mouseLocation)]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300) configuration:configuration.get() addToWindow:YES]);
    [webView _setWindowOcclusionDetectionEnabled:NO];
    RetainPtr initialWindow = [webView window];
    RetainPtr screen = [initialWindow screen] ?: [NSScreen mainScreen];
    NSRect screenFrame = [screen frame];
    [initialWindow setFrame:NSMakeRect(screenFrame.origin.x + 160, screenFrame.origin.y + 180, 400, 300) display:YES];
    [webView _setObscuredContentInsets:NSEdgeInsetsMake(60, 0, 0, 0) immediate:YES];

    RetainPtr fullscreenDelegate = adoptNS([[FullscreenPointerLeaveTestDelegate alloc] init]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    bool pointerenter = false;
    bool pointerleave = false;
    bool mouseleave = false;
    [webView performAfterReceivingMessage:@"pointerenter" action:[&] { pointerenter = true; }];
    [webView performAfterReceivingMessage:@"pointerleave" action:[&] { pointerleave = true; }];
    [webView performAfterReceivingMessage:@"mouseleave" action:[&] { mouseleave = true; }];
    [webView synchronouslyLoadHTMLString:
        @"<!DOCTYPE html>"
        @"<style>"
        @"body, html { margin: 0; width: 100%; height: 100%; }"
        @"#player { position: fixed; inset: 0; background: #111; }"
        @"#fsButton { position: absolute; right: 12px; bottom: 12px; width: 48px; height: 48px; }"
        @"</style>"
        @"<div id=player><video></video><button id=fsButton>FS</button></div>"
        @"<script>"
        @"let mousemoveCount = 0;"
        @"let pointermoveCount = 0;"
        @"document.addEventListener('mousemove', () => { mousemoveCount++; });"
        @"document.addEventListener('pointermove', () => { pointermoveCount++; });"
        @"let eventToMessage = event => { window.webkit.messageHandlers.testHandler.postMessage(event.type); };"
        @"fsButton.addEventListener('pointerenter', eventToMessage, {once:true});"
        @"fsButton.addEventListener('pointerleave', eventToMessage, {once:true});"
        @"fsButton.addEventListener('mouseleave', eventToMessage, {once:true});"
        @"</script>"];

    // With top obscured inset = 60 in a 400x300 view, unobscured height is 240; #fsButton center is client (364, 204) -> flipped view (364, 264).
    NSPoint buttonPointInWindow = [webView convertPoint:NSMakePoint(364, 264) toView:nil];
    s_overrideFullscreenScreenMouseLocation = [initialWindow convertPointToScreen:buttonPointInWindow];

    [webView mouseMoveToPoint:buttonPointInWindow withFlags:0];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE(Util::runFor(&pointerenter, 5_s));
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"fsButton.matches(':hover')"] boolValue]);

    [webView objectByEvaluatingJavaScript:@"mousemoveCount = 0; pointermoveCount = 0;"];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"player.webkitRequestFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidEnterFullscreen]);
    EXPECT_TRUE(Util::runFor(&pointerleave, 5_s));
    EXPECT_TRUE(Util::runFor(&mouseleave, 5_s));
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"fsButton.matches(':hover')"] boolValue]);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"mousemoveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"pointermoveCount"] intValue], 0);

    [webView objectByEvaluatingJavaScript:@"document.webkitExitFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidExitFullscreen]);
}

TEST(Fullscreen, PointerLeaveAfterIconSwapWhenWindowOffsetOnScreen)
{
    s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    auto resetOverride = makeScopeExit([] {
        s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    });
    InstanceMethodSwizzler windowMouseLocationSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_fullscreenTest_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler eventMouseLocationSwizzler([NSEvent class], @selector(mouseLocation), [NSEvent methodForSelector:@selector(_fullscreenTest_mouseLocation)]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300) configuration:configuration.get() addToWindow:YES]);
    [webView _setWindowOcclusionDetectionEnabled:NO];

    RetainPtr fullscreenDelegate = adoptNS([[FullscreenPointerLeaveTestDelegate alloc] init]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    NSRect screenFrame = [[[webView window] screen] ?: [NSScreen mainScreen] frame];
    // Offset window by (200, 150) on screen so X_mon = 200 + 355 = 555 > 400 (outside pre-fullscreen LocalFrameView at t+37ms).
    [[webView window] setFrame:NSMakeRect(screenFrame.origin.x + 200, screenFrame.origin.y + 150, 400, 300) display:YES];

    bool pointerenter = false;
    bool pointerleave = false;
    bool mouseleave = false;
    bool fullscreenchange = false;
    [webView performAfterReceivingMessage:@"pointerenter" action:[&] { pointerenter = true; }];
    [webView performAfterReceivingMessage:@"pointerleave" action:[&] { pointerleave = true; }];
    [webView performAfterReceivingMessage:@"mouseleave" action:[&] { mouseleave = true; }];
    [webView performAfterReceivingMessage:@"webkitfullscreenchange" action:[&] { fullscreenchange = true; }];

    [webView synchronouslyLoadHTMLString:
        @"<!DOCTYPE html>"
        @"<style>"
        @"html, body { margin: 0; width: 100%; height: 100%; }"
        @"#player { position: fixed; inset: 0; background: #000; }"
        @"</style>"
        @"<div id='player'><video id='vid' style='width:100%;height:100%;'></video><div id='controlsHost'></div></div>"
        @"<script>"
        @"const host = document.getElementById('controlsHost');"
        @"const root = host.attachShadow({ mode: 'open' });"
        @"root.innerHTML = `<div id='header' style='position:fixed;top:20px;right:20px;width:50px;height:50px;'>"
        @"  <button id='fsBtn' style='width:50px;height:50px;padding:0;'><span id='iconShape'><div id='iconDiv' style='width:50px;height:50px;'></div></span></button>"
        @"</div>`;"
        @"const header = root.getElementById('header');"
        @"const fsBtn = root.getElementById('fsBtn');"
        @"const iconShape = root.getElementById('iconShape');"
        @"window.tooltipVisible = false;"
        @"window.headerLocked = false;"
        @"window.mouseMoveCount = 0;"
        @"document.addEventListener('mousemove', () => { window.mouseMoveCount++; }, true);"
        @"document.addEventListener('pointermove', () => { window.mouseMoveCount++; }, true);"
        @"header.addEventListener('mouseenter', () => { window.headerLocked = true; });"
        @"header.addEventListener('mouseleave', () => { window.headerLocked = false; });"
        @"fsBtn.addEventListener('mouseenter', () => { window.tooltipVisible = true; });"
        @"fsBtn.addEventListener('pointerenter', () => { window.webkit.messageHandlers.testHandler.postMessage('pointerenter'); }, { once: true });"
        @"fsBtn.addEventListener('pointerleave', () => { window.webkit.messageHandlers.testHandler.postMessage('pointerleave'); }, { once: true });"
        @"fsBtn.addEventListener('mouseleave', () => { window.tooltipVisible = false; window.webkit.messageHandlers.testHandler.postMessage('mouseleave'); }, { once: true });"
        @"document.addEventListener('webkitfullscreenchange', () => {"
        @"  while (iconShape.firstChild) iconShape.removeChild(iconShape.firstChild);"
        @"  const exitIcon = document.createElement('div');"
        @"  exitIcon.style.cssText = 'width:50px;height:50px;';"
        @"  iconShape.appendChild(exitIcon);"
        @"  window.webkit.messageHandlers.testHandler.postMessage('webkitfullscreenchange');"
        @"}, { once: true });"
        @"window.isFsBtnHovered = () => fsBtn.matches(':hover');"
        @"</script>"];

    // Top-right button is at x in [330, 380], y_flipped in [20, 70] -> y_unflipped in [230, 280] for a 300px tall window.
    NSPoint buttonPointInWindow = NSMakePoint(355, 255);
    s_overrideFullscreenScreenMouseLocation = [[webView window] convertPointToScreen:buttonPointInWindow];

    [webView mouseMoveToPoint:buttonPointInWindow withFlags:0];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE(Util::runFor(&pointerenter, 5_s));
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.tooltipVisible && window.headerLocked"] boolValue]);
    [webView objectByEvaluatingJavaScript:@"window.mouseMoveCount = 0;"];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.documentElement.webkitRequestFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidEnterFullscreen]);
    EXPECT_TRUE(Util::runFor(&fullscreenchange, 5_s));
    EXPECT_TRUE(Util::runFor(&pointerleave, 5_s));
    EXPECT_TRUE(Util::runFor(&mouseleave, 5_s));
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.tooltipVisible"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.headerLocked"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.isFsBtnHovered()"] boolValue]);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.mouseMoveCount"] intValue], 0);

    [webView objectByEvaluatingJavaScript:@"document.webkitExitFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidExitFullscreen]);
}

TEST(Fullscreen, ControlsOverlayDoesNotReceiveSpuriousPointerLeaveWhenWindowOffsetOnScreen)
{
    s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    auto resetOverride = makeScopeExit([] {
        s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    });
    InstanceMethodSwizzler windowMouseLocationSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_fullscreenTest_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler eventMouseLocationSwizzler([NSEvent class], @selector(mouseLocation), [NSEvent methodForSelector:@selector(_fullscreenTest_mouseLocation)]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300) configuration:configuration.get() addToWindow:YES]);
    [webView _setWindowOcclusionDetectionEnabled:NO];

    RetainPtr fullscreenDelegate = adoptNS([[FullscreenPointerLeaveTestDelegate alloc] init]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    NSRect screenFrame = [[[webView window] screen] ?: [NSScreen mainScreen] frame];
    // Offset window by (300, 150) so X_mon = 300 + 200 = 500 > 400 (outside pre-fullscreen 400x300 view during transition, but inside fullscreen #controls).
    [[webView window] setFrame:NSMakeRect(screenFrame.origin.x + 300, screenFrame.origin.y + 150, 400, 300) display:YES];

    bool pointerenter = false;
    bool fullscreenchange = false;
    bool windowResized = false;
    [webView performAfterReceivingMessage:@"pointerenter" action:[&] { pointerenter = true; }];
    [webView performAfterReceivingMessage:@"webkitfullscreenchange" action:[&] { fullscreenchange = true; }];
    [webView performAfterReceivingMessage:@"resized" action:[&] { windowResized = true; }];

    [webView synchronouslyLoadHTMLString:
        @"<!DOCTYPE html>"
        @"<style>"
        @"html, body { margin: 0; width: 100%; height: 100%; }"
        @"#target { position: relative; width: 400px; height: 300px; background: #111; }"
        @"#target:-webkit-full-screen { width: 100%; height: 100%; }"
        @"#controls { position: absolute; inset: 0; }"
        @"</style>"
        @"<div id='target'><div id='controls'>Controls</div></div>"
        @"<script>"
        @"const target = document.getElementById('target');"
        @"const controls = document.getElementById('controls');"
        @"window.pointerLeaveCount = 0;"
        @"window.mouseLeaveCount = 0;"
        @"window.mouseMoveCount = 0;"
        @"controls.addEventListener('pointerenter', () => window.webkit.messageHandlers.testHandler.postMessage('pointerenter'), { once: true });"
        @"controls.addEventListener('pointerleave', () => { window.pointerLeaveCount++; });"
        @"controls.addEventListener('mouseleave', () => { window.mouseLeaveCount++; });"
        @"document.addEventListener('mousemove', () => { window.mouseMoveCount++; }, true);"
        @"document.addEventListener('pointermove', () => { window.mouseMoveCount++; }, true);"
        @"document.addEventListener('webkitfullscreenchange', () => window.webkit.messageHandlers.testHandler.postMessage('webkitfullscreenchange'), { once: true });"
        @"window.addEventListener('resize', () => window.webkit.messageHandlers.testHandler.postMessage('resized'), { once: true });"
        @"</script>"];

    NSPoint centerPointInWindow = NSMakePoint(200, 150);
    s_overrideFullscreenScreenMouseLocation = [[webView window] convertPointToScreen:centerPointInWindow];

    [webView mouseMoveToPoint:centerPointInWindow withFlags:0];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE(Util::runFor(&pointerenter, 5_s));
    [webView objectByEvaluatingJavaScript:@"window.mouseMoveCount = 0;"];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.webkitRequestFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidEnterFullscreen]);
    EXPECT_TRUE(Util::runFor(&fullscreenchange, 5_s));
    EXPECT_TRUE(Util::runFor(&windowResized, 5_s));
    Util::runFor(250_ms);

    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.pointerLeaveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.mouseLeaveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.mouseMoveCount"] intValue], 0);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"controls.matches(':hover')"] boolValue]);

    [webView objectByEvaluatingJavaScript:@"document.webkitExitFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidExitFullscreen]);
}

TEST(Fullscreen, PointerLeaveOnExitButtonWhenRestoredWindowDoesNotContainCursor)
{
    s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    auto resetOverride = makeScopeExit([] {
        s_overrideFullscreenScreenMouseLocation = NSZeroPoint;
    });
    InstanceMethodSwizzler windowMouseLocationSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_fullscreenTest_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler eventMouseLocationSwizzler([NSEvent class], @selector(mouseLocation), [NSEvent methodForSelector:@selector(_fullscreenTest_mouseLocation)]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300) configuration:configuration.get() addToWindow:YES]);
    [webView _setWindowOcclusionDetectionEnabled:NO];

    RetainPtr fullscreenDelegate = adoptNS([[FullscreenPointerLeaveTestDelegate alloc] init]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    NSRect screenFrame = [[[webView window] screen] ?: [NSScreen mainScreen] frame];
    [[webView window] setFrame:NSMakeRect(screenFrame.origin.x + 50, screenFrame.origin.y + 50, 400, 300) display:YES];

    bool pointerenter = false;
    bool pointerleave = false;
    bool mouseleave = false;
    [webView performAfterReceivingMessage:@"pointerenter" action:[&] { pointerenter = true; }];
    [webView performAfterReceivingMessage:@"pointerleave" action:[&] { pointerleave = true; }];
    [webView performAfterReceivingMessage:@"mouseleave" action:[&] { mouseleave = true; }];

    [webView synchronouslyLoadHTMLString:
        @"<!DOCTYPE html>"
        @"<style>"
        @"html, body { margin: 0; width: 100%; height: 100%; }"
        @"#player { position: fixed; inset: 0; background: #111; }"
        @"#exitBtn { position: absolute; right: 16px; bottom: 16px; width: 60px; height: 40px; }"
        @"</style>"
        @"<div id='player'><button id='exitBtn'>Exit</button></div>"
        @"<script>"
        @"const player = document.getElementById('player');"
        @"const exitBtn = document.getElementById('exitBtn');"
        @"window.mouseMoveCount = 0;"
        @"document.addEventListener('mousemove', () => { window.mouseMoveCount++; }, true);"
        @"exitBtn.addEventListener('pointerenter', () => window.webkit.messageHandlers.testHandler.postMessage('pointerenter'), { once: true });"
        @"exitBtn.addEventListener('pointerleave', () => window.webkit.messageHandlers.testHandler.postMessage('pointerleave'), { once: true });"
        @"exitBtn.addEventListener('mouseleave', () => window.webkit.messageHandlers.testHandler.postMessage('mouseleave'), { once: true });"
        @"</script>"];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"player.webkitRequestFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidEnterFullscreen]);

    // Hover #exitBtn at bottom-right of the fullscreen window, which lies outside the restored 400x300 window at (50, 50).
    RetainPtr fullscreenWindow = [webView window];
    NSPoint exitBtnPointInFullscreenWindow = NSMakePoint([fullscreenWindow frame].size.width - 36, 36);
    s_overrideFullscreenScreenMouseLocation = [fullscreenWindow convertPointToScreen:exitBtnPointInFullscreenWindow];

    [webView mouseMoveToPoint:exitBtnPointInFullscreenWindow withFlags:0];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE(Util::runFor(&pointerenter, 5_s));
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"exitBtn.matches(':hover')"] boolValue]);
    [webView objectByEvaluatingJavaScript:@"window.mouseMoveCount = 0;"];

    [webView objectByEvaluatingJavaScript:@"document.webkitExitFullscreen()"];
    EXPECT_TRUE([fullscreenDelegate waitForDidExitFullscreen]);
    EXPECT_TRUE(Util::runFor(&pointerleave, 5_s));
    EXPECT_TRUE(Util::runFor(&mouseleave, 5_s));
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"exitBtn.matches(':hover')"] boolValue]);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.mouseMoveCount"] intValue], 0);
}

}

#endif
