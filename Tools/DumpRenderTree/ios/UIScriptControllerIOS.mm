/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "UIScriptController.h"

#if PLATFORM(IOS)

#import "DumpRenderTreeBrowserView.h"
#import "UIScriptContext.h"
#import <WebCore/FloatRect.h>
#import <wtf/MainThread.h>

extern DumpRenderTreeBrowserView *gWebBrowserView;
extern DumpRenderTreeWebScrollView *gWebScrollView;

namespace WTR {

void UIScriptController::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptController::zoomToScale(double scale, JSValueRef callback)
{
    RefPtr<UIScriptController> protectedThis(this);
    unsigned callbackID = protectedThis->context()->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    dispatch_async(dispatch_get_main_queue(), ^{
        [gWebScrollView zoomToScale:scale animated:YES completionHandler:^{
            if (!protectedThis->context())
                return;
            protectedThis->context()->asyncTaskComplete(callbackID);
        }];
    });
}

double UIScriptController::zoomScale() const
{
    return gWebScrollView.zoomScale;
}

void UIScriptController::touchDownAtPoint(long x, long y, long touchCount, JSValueRef callback)
{
}

void UIScriptController::liftUpAtPoint(long x, long y, long touchCount, JSValueRef callback)
{
}

void UIScriptController::singleTapAtPoint(long x, long y, JSValueRef callback)
{
}

void UIScriptController::doubleTapAtPoint(long x, long y, JSValueRef callback)
{
}

void UIScriptController::dragFromPointToPoint(long startX, long startY, long endX, long endY, double durationSeconds, JSValueRef callback)
{
}

void UIScriptController::stylusDownAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
}

void UIScriptController::stylusMoveToPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
}

void UIScriptController::stylusUpAtPoint(long x, long y, JSValueRef callback)
{
}

void UIScriptController::stylusTapAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
}

void UIScriptController::typeCharacterUsingHardwareKeyboard(JSStringRef character, JSValueRef callback)
{
}

void UIScriptController::keyDownUsingHardwareKeyboard(JSStringRef character, JSValueRef callback)
{
}

void UIScriptController::keyUpUsingHardwareKeyboard(JSStringRef character, JSValueRef callback)
{
}

void UIScriptController::dismissFormAccessoryView()
{
}

void UIScriptController::selectFormAccessoryPickerRow(long rowIndex)
{
}

void UIScriptController::scrollToOffset(long, long)
{
}

void UIScriptController::keyboardAccessoryBarNext()
{
}

void UIScriptController::keyboardAccessoryBarPrevious()
{
}

double UIScriptController::minimumZoomScale() const
{
    return gWebScrollView.minimumZoomScale;
}

double UIScriptController::maximumZoomScale() const
{
    return gWebScrollView.maximumZoomScale;
}

JSObjectRef UIScriptController::contentVisibleRect() const
{
    CGRect contentVisibleRect = [gWebBrowserView documentVisibleRect];
    WebCore::FloatRect rect(contentVisibleRect.origin.x, contentVisibleRect.origin.y, contentVisibleRect.size.width, contentVisibleRect.size.height);
    return m_context->objectFromRect(rect);
}

void UIScriptController::platformSetDidStartFormControlInteractionCallback()
{
}

void UIScriptController::platformSetDidEndFormControlInteractionCallback()
{
}

void UIScriptController::platformSetWillBeginZoomingCallback()
{
}

void UIScriptController::platformSetDidEndZoomingCallback()
{
}

void UIScriptController::platformSetDidShowKeyboardCallback()
{
}

void UIScriptController::platformSetDidHideKeyboardCallback()
{
}

void UIScriptController::platformSetDidEndScrollingCallback()
{
}

void UIScriptController::platformClearAllCallbacks()
{
}

}

#endif // PLATFORM(IOS)
