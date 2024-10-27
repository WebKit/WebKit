/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(PDF_PLUGIN) && HAVE(INCREMENTAL_PDF_APIS)

#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

OBJC_CLASS PDFDocument;

namespace WebCore {
class NetscapePlugInStreamLoader;
}

namespace WebKit {

class ByteRangeRequest;
class PDFPluginBase;
class PDFPluginStreamLoaderClient;

enum class ByteRangeRequestIdentifierType { };
using ByteRangeRequestIdentifier = ObjectIdentifier<ByteRangeRequestIdentifierType>;
using DataRequestCompletionHandler = Function<void(std::span<const uint8_t>)>;

enum class CheckValidRanges : bool;

class PDFIncrementalLoader : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PDFIncrementalLoader> {
    WTF_MAKE_TZONE_ALLOCATED(PDFIncrementalLoader);
    WTF_MAKE_NONCOPYABLE(PDFIncrementalLoader);
    friend class ByteRangeRequest;
    friend class PDFPluginStreamLoaderClient;
public:
    ~PDFIncrementalLoader();

    static Ref<PDFIncrementalLoader> create(PDFPluginBase&);

    void clear();

    void incrementalPDFStreamDidReceiveData(const WebCore::SharedBuffer&);
    void incrementalPDFStreamDidFinishLoading();
    void incrementalPDFStreamDidFail();

    void streamLoaderDidStart(ByteRangeRequestIdentifier, RefPtr<WebCore::NetscapePlugInStreamLoader>&&);

    void receivedNonLinearizedPDFSentinel();

#if !LOG_DISABLED
    void logState(WTF::TextStream&);
#endif

    // Only public for the callbacks
    size_t dataProviderGetBytesAtPosition(std::span<uint8_t> buffer, off_t position);
    void dataProviderGetByteRanges(CFMutableArrayRef buffers, std::span<const CFRange> ranges);

private:
    PDFIncrementalLoader(PDFPluginBase&);

    void threadEntry(Ref<PDFIncrementalLoader>&&);
    void transitionToMainThreadDocument();

    bool documentFinishedLoading() const;

    void appendAccumulatedDataToDataBuffer(ByteRangeRequest&);

    std::span<const uint8_t> dataSpanForRange(uint64_t position, size_t count, CheckValidRanges) const;
    uint64_t availableDataSize() const;

    void getResourceBytesAtPosition(size_t count, off_t position, DataRequestCompletionHandler&&);
    size_t getResourceBytesAtPositionAfterLoadingComplete(std::span<uint8_t> buffer, off_t position);

    void unconditionalCompleteOutstandingRangeRequests();

    ByteRangeRequest* byteRangeRequestForStreamLoader(WebCore::NetscapePlugInStreamLoader&);
    void forgetStreamLoader(WebCore::NetscapePlugInStreamLoader&);
    void cancelAndForgetStreamLoader(WebCore::NetscapePlugInStreamLoader&);

    std::optional<ByteRangeRequestIdentifier> identifierForLoader(WebCore::NetscapePlugInStreamLoader*);
    void removeOutstandingByteRangeRequest(ByteRangeRequestIdentifier);


    bool requestCompleteIfPossible(ByteRangeRequest&);
    void requestDidCompleteWithBytes(ByteRangeRequest&, size_t byteCount);
    void requestDidCompleteWithAccumulatedData(ByteRangeRequest&, size_t completionSize);

#if !LOG_DISABLED
    size_t incrementThreadsWaitingOnCallback() { return ++m_threadsWaitingOnCallback; }
    size_t decrementThreadsWaitingOnCallback() { return --m_threadsWaitingOnCallback; }

    void incrementalLoaderLog(const String&);
    void logStreamLoader(WTF::TextStream&, WebCore::NetscapePlugInStreamLoader&);
#endif

    class SemaphoreWrapper : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<SemaphoreWrapper> {
    public:
        static Ref<SemaphoreWrapper> create() { return adoptRef(*new SemaphoreWrapper); }

        void wait() { m_semaphore.wait(); }
        void signal()
        {
            m_wasSignaled = true;
            m_semaphore.signal();
        }
        bool wasSignaled() const { return m_wasSignaled; }

    private:
        SemaphoreWrapper() = default;

        BinarySemaphore m_semaphore;
        std::atomic<bool> m_wasSignaled { false };
    };

    RefPtr<SemaphoreWrapper> createDataSemaphore();

    ThreadSafeWeakPtr<PDFPluginBase> m_plugin;

    RetainPtr<PDFDocument> m_backgroundThreadDocument;
    RefPtr<Thread> m_pdfThread;

    Ref<PDFPluginStreamLoaderClient> m_streamLoaderClient;

    struct RequestData;
    std::unique_ptr<RequestData> m_requestData;

    ThreadSafeWeakHashSet<SemaphoreWrapper> m_dataSemaphores WTF_GUARDED_BY_LOCK(m_wasPDFThreadTerminationRequestedLock);

    Lock m_wasPDFThreadTerminationRequestedLock;
    bool m_wasPDFThreadTerminationRequested WTF_GUARDED_BY_LOCK(m_wasPDFThreadTerminationRequestedLock) { false };

#if !LOG_DISABLED
    std::atomic<size_t> m_threadsWaitingOnCallback { 0 };
    std::atomic<size_t> m_completedRangeRequests { 0 };
    std::atomic<size_t> m_completedNetworkRangeRequests { 0 };
#endif


};

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN) && HAVE(INCREMENTAL_PDF_APIS)
