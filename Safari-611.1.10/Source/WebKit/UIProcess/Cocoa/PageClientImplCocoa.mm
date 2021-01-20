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

#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivateForTesting.h"
#import <WebCore/AlternativeTextUIController.h>
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

void PageClientImplCocoa::pageClosed()
{
    m_alternativeTextUIController->clear();
}

WebCore::DictationContext PageClientImplCocoa::addDictationAlternatives(NSTextAlternatives *alternatives)
{
    return m_alternativeTextUIController->addAlternatives(alternatives);
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
    
}
