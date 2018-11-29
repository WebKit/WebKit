/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WebPageProxy.h"

#import "APIAttachment.h"
#import "APIUIClient.h"
#import "DataDetectionResult.h"
#import "LoadParameters.h"
#import "PageClient.h"
#import "SafeBrowsingSPI.h"
#import "SafeBrowsingWarning.h"
#import "WebPageMessages.h"
#import "WebProcessProxy.h"
#import <WebCore/DragItem.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <WebCore/ValidationBubble.h>
#import <wtf/BlockPtr.h>
#import <wtf/cf/TypeCastsCF.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(DATA_DETECTION)
void WebPageProxy::setDataDetectionResult(const DataDetectionResult& dataDetectionResult)
{
    m_dataDetectionResults = dataDetectionResult.results;
}
#endif

void WebPageProxy::saveRecentSearches(const String& name, const Vector<WebCore::RecentSearch>& searchItems)
{
    if (!name) {
        // FIXME: This should be a message check.
        return;
    }

    WebCore::saveRecentSearches(name, searchItems);
}

void WebPageProxy::loadRecentSearches(const String& name, Vector<WebCore::RecentSearch>& searchItems)
{
    if (!name) {
        // FIXME: This should be a message check.
        return;
    }

    searchItems = WebCore::loadRecentSearches(name);
}

void WebPageProxy::beginSafeBrowsingCheck(const URL& url, WebFramePolicyListenerProxy& listener)
{
#if HAVE(SAFE_BROWSING)
    SSBLookupContext *context = [SSBLookupContext sharedLookupContext];
    if (!context)
        return listener.didReceiveSafeBrowsingResults({ });
    [context lookUpURL:url completionHandler:BlockPtr<void(SSBLookupResult *, NSError *)>::fromCallable([listener = makeRef(listener), url = url] (SSBLookupResult *result, NSError *error) mutable {
        RunLoop::main().dispatch([listener = WTFMove(listener), result = retainPtr(result), error = retainPtr(error), url = WTFMove(url)] {
            if (error) {
                listener->didReceiveSafeBrowsingResults({ });
                return;
            }

            for (SSBServiceLookupResult *lookupResult in [result serviceLookupResults]) {
                if (lookupResult.isPhishing || lookupResult.isMalware || lookupResult.isUnwantedSoftware) {
                    listener->didReceiveSafeBrowsingResults(SafeBrowsingWarning::create(url, lookupResult));
                    return;
                }
            }
            listener->didReceiveSafeBrowsingResults({ });
        });
    }).get()];
#else
    listener.didReceiveSafeBrowsingResults({ });
#endif
}

#if ENABLE(CONTENT_FILTERING)
void WebPageProxy::contentFilterDidBlockLoadForFrame(const WebCore::ContentFilterUnblockHandler& unblockHandler, uint64_t frameID)
{
    if (WebFrameProxy* frame = m_process->webFrame(frameID))
        frame->contentFilterDidBlockLoad(unblockHandler);
}
#endif

void WebPageProxy::addPlatformLoadParameters(LoadParameters& loadParameters)
{
    loadParameters.dataDetectionContext = m_uiClient->dataDetectionContext();
}

void WebPageProxy::createSandboxExtensionsIfNeeded(const Vector<String>& files, SandboxExtension::Handle& fileReadHandle, SandboxExtension::HandleArray& fileUploadHandles)
{
    if (!files.size())
        return;

    if (files.size() == 1) {
        BOOL isDirectory;
        if ([[NSFileManager defaultManager] fileExistsAtPath:files[0] isDirectory:&isDirectory] && !isDirectory) {
            SandboxExtension::createHandle("/", SandboxExtension::Type::ReadOnly, fileReadHandle);
            process().willAcquireUniversalFileReadSandboxExtension();
        }
    }

    fileUploadHandles.allocate(files.size());
    for (size_t i = 0; i< files.size(); i++) {
        NSString *file = files[i];
        if (![[NSFileManager defaultManager] fileExistsAtPath:file])
            continue;
        SandboxExtension::createHandle(file, SandboxExtension::Type::ReadOnly, fileUploadHandles[i]);
    }
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::startDrag(const DragItem& dragItem, const ShareableBitmap::Handle& dragImageHandle)
{
    pageClient().startDrag(dragItem, dragImageHandle);
}

#if PLATFORM(IOS_FAMILY)

void WebPageProxy::setPromisedDataForImage(const String&, const SharedMemory::Handle&, uint64_t, const String&, const String&, const String&, const String&, const String&, const SharedMemory::Handle&, uint64_t)
{
    notImplemented();
}

void WebPageProxy::setDragCaretRect(const IntRect& dragCaretRect)
{
    if (m_currentDragCaretRect == dragCaretRect)
        return;

    auto previousRect = m_currentDragCaretRect;
    m_currentDragCaretRect = dragCaretRect;
    pageClient().didChangeDataInteractionCaretRect(previousRect, dragCaretRect);
}

#endif // PLATFORM(IOS_FAMILY)

#endif // ENABLE(DRAG_SUPPORT)

#if ENABLE(ATTACHMENT_ELEMENT)

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&& attachment, const String& preferredFileName, const IPC::DataReference& dataReference)
{
    if (dataReference.isEmpty())
        return;

    auto buffer = SharedBuffer::create(dataReference.data(), dataReference.size());
    auto fileWrapper = adoptNS([pageClient().allocFileWrapperInstance() initRegularFileWithContents:buffer->createNSData().autorelease()]);
    [fileWrapper setPreferredFilename:preferredFileName];
    attachment->setFileWrapper(fileWrapper.get());
}

void WebPageProxy::platformRegisterAttachment(Ref<API::Attachment>&& attachment, const String& filePath)
{
    if (!filePath)
        return;

    auto fileWrapper = adoptNS([pageClient().allocFileWrapperInstance() initWithURL:[NSURL fileURLWithPath:filePath] options:0 error:nil]);
    attachment->setFileWrapper(fileWrapper.get());
}

void WebPageProxy::platformCloneAttachment(Ref<API::Attachment>&& fromAttachment, Ref<API::Attachment>&& toAttachment)
{
    toAttachment->setFileWrapper(fromAttachment->fileWrapper());
}

#endif // ENABLE(ATTACHMENT_ELEMENT)
    
void WebPageProxy::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    if (!isValid())
        return;
    
    process().send(Messages::WebPage::PerformDictionaryLookupAtLocation(point), m_pageID);
}

void WebPageProxy::performDictionaryLookupOfCurrentSelection()
{
    if (!isValid())
        return;
    
    process().send(Messages::WebPage::PerformDictionaryLookupOfCurrentSelection(), m_pageID);
}

} // namespace WebKit
