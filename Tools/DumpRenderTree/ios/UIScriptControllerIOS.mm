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
#import "UIScriptControllerIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "DumpRenderTreeBrowserView.h"
#import "UIScriptContext.h"
#import <WebCore/FloatRect.h>
#import <wtf/MainThread.h>

extern DumpRenderTreeBrowserView *gWebBrowserView;
extern DumpRenderTreeWebScrollView *gWebScrollView;

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerIOS(context));
}

void UIScriptControllerIOS::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerIOS::zoomToScale(double scale, JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    RefPtr<UIScriptController> protectedThis(this);
    dispatch_async(dispatch_get_main_queue(), ^{
        [gWebScrollView zoomToScale:scale animated:YES completionHandler:^{
            if (!m_context)
                return;
            m_context->asyncTaskComplete(callbackID);
        }];
    });
}

double UIScriptControllerIOS::zoomScale() const
{
    return gWebScrollView.zoomScale;
}

static CGPoint contentOffsetBoundedInValidRange(UIScrollView *scrollView, CGPoint contentOffset)
{
    UIEdgeInsets contentInsets = scrollView.contentInset;
    CGSize contentSize = scrollView.contentSize;
    CGSize scrollViewSize = scrollView.bounds.size;

    CGFloat maxHorizontalOffset = contentSize.width + contentInsets.right - scrollViewSize.width;
    contentOffset.x = std::min(maxHorizontalOffset, contentOffset.x);
    contentOffset.x = std::max(-contentInsets.left, contentOffset.x);

    CGFloat maxVerticalOffset = contentSize.height + contentInsets.bottom - scrollViewSize.height;
    contentOffset.y = std::min(maxVerticalOffset, contentOffset.y);
    contentOffset.y = std::max(-contentInsets.top, contentOffset.y);
    return contentOffset;
}

double UIScriptControllerIOS::contentOffsetX() const
{
    return [gWebScrollView contentOffset].x;
}

double UIScriptControllerIOS::contentOffsetY() const
{
    return [gWebScrollView contentOffset].y;
}

void UIScriptControllerIOS::scrollToOffset(long x, long y)
{
    [gWebScrollView setContentOffset:contentOffsetBoundedInValidRange(gWebScrollView, CGPointMake(x, y)) animated:YES];
}

void UIScriptControllerIOS::immediateScrollToOffset(long x, long y)
{
    [gWebScrollView setContentOffset:contentOffsetBoundedInValidRange(gWebScrollView, CGPointMake(x, y)) animated:NO];
}

void UIScriptControllerIOS::immediateZoomToScale(double scale)
{
    [gWebScrollView setZoomScale:scale animated:NO];
}

double UIScriptControllerIOS::minimumZoomScale() const
{
    return gWebScrollView.minimumZoomScale;
}

double UIScriptControllerIOS::maximumZoomScale() const
{
    return gWebScrollView.maximumZoomScale;
}

JSObjectRef UIScriptControllerIOS::contentVisibleRect() const
{
    CGRect contentVisibleRect = [gWebBrowserView documentVisibleRect];
    WebCore::FloatRect rect(contentVisibleRect.origin.x, contentVisibleRect.origin.y, contentVisibleRect.size.width, contentVisibleRect.size.height);
    return m_context->objectFromRect(rect);
}

}

#endif // PLATFORM(IOS_FAMILY)
