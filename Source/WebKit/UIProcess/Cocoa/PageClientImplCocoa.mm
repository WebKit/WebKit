/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "PageClientImplCocoa.h"

#import "WKWebViewInternal.h"
#import <WebCore/AlternativeTextUIController.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

PageClientImplCocoa::PageClientImplCocoa(WKWebView *webView)
    : m_webView { webView }
    , m_alternativeTextUIController { makeUnique<WebCore::AlternativeTextUIController>() }
{
}

PageClientImplCocoa::~PageClientImplCocoa() = default;

void PageClientImplCocoa::topContentInsetDidChange()
{
    [m_webView _recalculateViewportSizesWithMinimumViewportInset:[m_webView minimumViewportInset] maximumViewportInset:[m_webView maximumViewportInset] throwOnInvalidInput:NO];
}

void PageClientImplCocoa::themeColorWillChange()
{
    [m_webView willChangeValueForKey:@"themeColor"];

    // FIXME: Remove old `-[WKWebView _themeColor]` SPI <rdar://76662644>
    [m_webView willChangeValueForKey:@"_themeColor"];
}

void PageClientImplCocoa::themeColorDidChange()
{
    [m_webView didChangeValueForKey:@"themeColor"];

    // FIXME: Remove old `-[WKWebView _themeColor]` SPI <rdar://76662644>
    [m_webView didChangeValueForKey:@"_themeColor"];
}

void PageClientImplCocoa::underPageBackgroundColorWillChange()
{
    [m_webView willChangeValueForKey:@"underPageBackgroundColor"];
}

void PageClientImplCocoa::underPageBackgroundColorDidChange()
{
    [m_webView didChangeValueForKey:@"underPageBackgroundColor"];
}

void PageClientImplCocoa::pageExtendedBackgroundColorWillChange()
{
    // FIXME: Remove old `-[WKWebView _pageExtendedBackgroundColor]` SPI <rdar://77789732>
    [m_webView willChangeValueForKey:@"_pageExtendedBackgroundColor"];
}

void PageClientImplCocoa::pageExtendedBackgroundColorDidChange()
{
    // FIXME: Remove old `-[WKWebView _pageExtendedBackgroundColor]` SPI <rdar://77789732>
    [m_webView didChangeValueForKey:@"_pageExtendedBackgroundColor"];
}

void PageClientImplCocoa::sampledPageTopColorWillChange()
{
    [m_webView willChangeValueForKey:@"_sampledPageTopColor"];
}

void PageClientImplCocoa::sampledPageTopColorDidChange()
{
    [m_webView didChangeValueForKey:@"_sampledPageTopColor"];
}

void PageClientImplCocoa::isPlayingAudioWillChange()
{
    [m_webView willChangeValueForKey:NSStringFromSelector(@selector(_isPlayingAudio))];
}

void PageClientImplCocoa::isPlayingAudioDidChange()
{
    [m_webView didChangeValueForKey:NSStringFromSelector(@selector(_isPlayingAudio))];
}

bool PageClientImplCocoa::scrollingUpdatesDisabledForTesting()
{
    return [m_webView _scrollingUpdatesDisabledForTesting];
}

#if ENABLE(ATTACHMENT_ELEMENT)

void PageClientImplCocoa::didInsertAttachment(API::Attachment& attachment, const String& source)
{
    [m_webView _didInsertAttachment:attachment withSource:source];
}

void PageClientImplCocoa::didRemoveAttachment(API::Attachment& attachment)
{
    [m_webView _didRemoveAttachment:attachment];
}

void PageClientImplCocoa::didInvalidateDataForAttachment(API::Attachment& attachment)
{
    [m_webView _didInvalidateDataForAttachment:attachment];
}

NSFileWrapper *PageClientImplCocoa::allocFileWrapperInstance() const
{
    Class cls = [m_webView configuration]._attachmentFileWrapperClass ?: [NSFileWrapper class];
    return [cls alloc];
}

NSSet *PageClientImplCocoa::serializableFileWrapperClasses() const
{
    Class defaultFileWrapperClass = NSFileWrapper.class;
    Class configuredFileWrapperClass = [m_webView configuration]._attachmentFileWrapperClass;
    if (configuredFileWrapperClass && configuredFileWrapperClass != defaultFileWrapperClass)
        return [NSSet setWithObjects:configuredFileWrapperClass, defaultFileWrapperClass, nil];
    return [NSSet setWithObjects:defaultFileWrapperClass, nil];
}

#endif

#if ENABLE(APP_HIGHLIGHTS)
void PageClientImplCocoa::storeAppHighlight(const WebCore::AppHighlight &highlight)
{
    [m_webView _storeAppHighlight:highlight];
}
#endif // ENABLE(APP_HIGHLIGHTS)

void PageClientImplCocoa::pageClosed()
{
    m_alternativeTextUIController->clear();
}

#if ENABLE(GPU_PROCESS)
void PageClientImplCocoa::gpuProcessDidFinishLaunching()
{
    [m_webView willChangeValueForKey:@"_gpuProcessIdentifier"];
    [m_webView didChangeValueForKey:@"_gpuProcessIdentifier"];
}

void PageClientImplCocoa::gpuProcessDidExit()
{
    [m_webView willChangeValueForKey:@"_gpuProcessIdentifier"];
    [m_webView didChangeValueForKey:@"_gpuProcessIdentifier"];
}
#endif

WebCore::DictationContext PageClientImplCocoa::addDictationAlternatives(NSTextAlternatives *alternatives)
{
    return m_alternativeTextUIController->addAlternatives(alternatives);
}

void PageClientImplCocoa::replaceDictationAlternatives(NSTextAlternatives *alternatives, WebCore::DictationContext context)
{
    m_alternativeTextUIController->replaceAlternatives(alternatives, context);
}

void PageClientImplCocoa::removeDictationAlternatives(WebCore::DictationContext dictationContext)
{
    m_alternativeTextUIController->removeAlternatives(dictationContext);
}

Vector<String> PageClientImplCocoa::dictationAlternatives(WebCore::DictationContext dictationContext)
{
    return makeVector<String>(platformDictationAlternatives(dictationContext).alternativeStrings);
}

NSTextAlternatives *PageClientImplCocoa::platformDictationAlternatives(WebCore::DictationContext dictationContext)
{
    return m_alternativeTextUIController->alternativesForContext(dictationContext);
}

void PageClientImplCocoa::microphoneCaptureWillChange()
{
    [m_webView willChangeValueForKey:@"microphoneCaptureState"];
}

void PageClientImplCocoa::cameraCaptureWillChange()
{
    [m_webView willChangeValueForKey:@"cameraCaptureState"];
}

void PageClientImplCocoa::displayCaptureWillChange()
{
    [m_webView willChangeValueForKey:@"_displayCaptureState"];
}

void PageClientImplCocoa::displayCaptureSurfacesWillChange()
{
    [m_webView willChangeValueForKey:@"_displayCaptureSurfaces"];
}

void PageClientImplCocoa::systemAudioCaptureWillChange()
{
    [m_webView willChangeValueForKey:@"_systemAudioCaptureState"];
}

void PageClientImplCocoa::microphoneCaptureChanged()
{
    [m_webView didChangeValueForKey:@"microphoneCaptureState"];
}

void PageClientImplCocoa::cameraCaptureChanged()
{
    [m_webView didChangeValueForKey:@"cameraCaptureState"];
}

void PageClientImplCocoa::displayCaptureChanged()
{
    [m_webView didChangeValueForKey:@"_displayCaptureState"];
}

void PageClientImplCocoa::displayCaptureSurfacesChanged()
{
    [m_webView didChangeValueForKey:@"_displayCaptureSurfaces"];
}

void PageClientImplCocoa::systemAudioCaptureChanged()
{
    [m_webView didChangeValueForKey:@"_systemAudioCaptureState"];
}

WindowKind PageClientImplCocoa::windowKind()
{
    auto window = [m_webView window];
    if (!window)
        return WindowKind::Unparented;
    if ([window isKindOfClass:NSClassFromString(@"_SCNSnapshotWindow")])
        return WindowKind::InProcessSnapshotting;
    return WindowKind::Normal;
}

}
