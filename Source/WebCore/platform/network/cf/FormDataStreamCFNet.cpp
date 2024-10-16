/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FormDataStreamCFNet.h"

#include "BlobRegistryImpl.h"
#include "FormData.h"
#include "PlatformStrategies.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <wtf/Assertions.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/SchedulePair.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>

static const SInt32 fileNotFoundError = -43;

extern "C" void CFURLRequestSetHTTPRequestBody(CFMutableURLRequestRef mutableHTTPRequest, CFDataRef httpBody);
extern "C" void CFURLRequestSetHTTPHeaderFieldValue(CFMutableURLRequestRef mutableHTTPRequest, CFStringRef httpHeaderField, CFStringRef httpHeaderFieldValue);
extern "C" void CFURLRequestSetHTTPRequestBodyStream(CFMutableURLRequestRef req, CFReadStreamRef bodyStream);

typedef struct {
    CFIndex version; /* == 1 */
    void *(*create)(CFReadStreamRef stream, void *info);
    void (*finalize)(CFReadStreamRef stream, void *info);
    CFStringRef (*copyDescription)(CFReadStreamRef stream, void *info);
    Boolean (*open)(CFReadStreamRef stream, CFStreamError *error, Boolean *openComplete, void *info);
    Boolean (*openCompleted)(CFReadStreamRef stream, CFStreamError *error, void *info);
    CFIndex (*read)(CFReadStreamRef stream, UInt8 *buffer, CFIndex bufferLength, CFStreamError *error, Boolean *atEOF, void *info);
    const UInt8 *(*getBuffer)(CFReadStreamRef stream, CFIndex maxBytesToRead, CFIndex *numBytesRead, CFStreamError *error, Boolean *atEOF, void *info);
    Boolean (*canRead)(CFReadStreamRef stream, void *info);
    void (*close)(CFReadStreamRef stream, void *info);
    CFTypeRef (*copyProperty)(CFReadStreamRef stream, CFStringRef propertyName, void *info);
    Boolean (*setProperty)(CFReadStreamRef stream, CFStringRef propertyName, CFTypeRef propertyValue, void *info);
    void (*requestEvents)(CFReadStreamRef stream, CFOptionFlags streamEvents, void *info);
    void (*schedule)(CFReadStreamRef stream, CFRunLoopRef runLoop, CFStringRef runLoopMode, void *info);
    void (*unschedule)(CFReadStreamRef stream, CFRunLoopRef runLoop, CFStringRef runLoopMode, void *info);
} CFReadStreamCallBacksV1;

#define EXTERN extern "C"

EXTERN void CFReadStreamSignalEvent(CFReadStreamRef stream, CFStreamEventType event, const void *error);
EXTERN CFReadStreamRef CFReadStreamCreate(CFAllocatorRef alloc, const void *callbacks, void *info);

namespace WebCore {

static void formEventCallback(CFReadStreamRef stream, CFStreamEventType type, void* context);

static CFStringRef formDataPointerPropertyName = CFSTR("WebKitFormDataPointer");

CFStringRef formDataStreamLengthPropertyName()
{
    return CFSTR("WebKitFormDataStreamLength");
}

struct FormCreationContext {
    FormDataForUpload data;
    unsigned long long streamLength;
};

struct FormStreamFields {
    FormStreamFields(FormDataForUpload&& data)
        : data(WTFMove(data)) { }

    FormDataForUpload data;
    SchedulePairHashSet scheduledRunLoopPairs;
    Vector<FormDataElement> remainingElements; // in reverse order
    RetainPtr<CFReadStreamRef> currentStream;
    long long currentStreamRangeLength { BlobDataItem::toEndOfFile };
    MallocPtr<uint8_t, WTF::VectorBufferMalloc> currentData;
    CFReadStreamRef formStream { nullptr };
    unsigned long long streamLength { 0 };
    unsigned long long bytesSent { 0 };
    Lock streamIsBeingOpenedOrClosedLock;
};

static void closeCurrentStream(FormStreamFields* form)
{
    ASSERT(form->streamIsBeingOpenedOrClosedLock.isHeld());

    if (form->currentStream) {
        CFReadStreamClose(form->currentStream.get());
        CFReadStreamSetClient(form->currentStream.get(), kCFStreamEventNone, 0, 0);
        form->currentStream = nullptr;
        form->currentStreamRangeLength = BlobDataItem::toEndOfFile;
    }
    form->currentData = nullptr;
}

// Return false if we cannot advance the stream. Currently the only possible failure is that the underlying file has been removed or changed since File.slice.
static bool advanceCurrentStream(FormStreamFields* form)
{
    ASSERT(form->streamIsBeingOpenedOrClosedLock.isHeld());

    closeCurrentStream(form);

    if (form->remainingElements.isEmpty())
        return true;

    // Create the new stream.
    FormDataElement& nextInput = form->remainingElements.last();

    bool success = WTF::switchOn(nextInput.data,
        [form] (Vector<uint8_t>& bytes) {
            size_t size = bytes.size();
            MallocPtr<uint8_t, WTF::VectorBufferMalloc> data = bytes.releaseBuffer();
            form->currentStream = adoptCF(CFReadStreamCreateWithBytesNoCopy(0, data.get(), size, kCFAllocatorNull));
            form->currentData = WTFMove(data);
            return true;
        }, [form] (const FormDataElement::EncodedFileData& fileData) {
            // Check if the file has been changed.
            if (!fileData.fileModificationTimeMatchesExpectation())
                return false;

            const String& path = fileData.filename;
            form->currentStream = adoptCF(CFReadStreamCreateWithFile(0, FileSystem::pathAsURL(path).get()));
            if (!form->currentStream) {
                // The file must have been removed or become unreadable.
                return false;
            }
            if (fileData.fileStart > 0) {
                RetainPtr<CFNumberRef> position = adoptCF(CFNumberCreate(0, kCFNumberLongLongType, &fileData.fileStart));
                CFReadStreamSetProperty(form->currentStream.get(), kCFStreamPropertyFileCurrentOffset, position.get());
            }
            form->currentStreamRangeLength = fileData.fileLength;
            return true;
        }, [] (const FormDataElement::EncodedBlobData&) {
            ASSERT_NOT_REACHED();
            return false;
        }
    );

    if (!success)
        return false;

    callOnMainThread([lastElement = form->remainingElements.takeLast()] {
        // Ensure FormDataElement destructor happens on main thread.
    });

    // Set up the callback.
    CFStreamClientContext context = { 0, form, 0, 0, 0 };
    CFReadStreamSetClient(form->currentStream.get(), kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered,
        formEventCallback, &context);

    // Schedule with the current set of run loops.
    for (auto& pair : form->scheduledRunLoopPairs)
        CFReadStreamScheduleWithRunLoop(form->currentStream.get(), pair->runLoop(), pair->mode());

    return true;
}

static bool openNextStream(FormStreamFields* form)
{
    // CFReadStreamOpen() can cause this function to be re-entered from another thread before it returns.
    // One example when this can occur is when the stream being opened has no data. See <rdar://problem/23550269>.
    Locker locker { form->streamIsBeingOpenedOrClosedLock };

    // Skip over any streams we can't open.
    if (!advanceCurrentStream(form))
        return false;
    while (form->currentStream && !CFReadStreamOpen(form->currentStream.get())) {
        if (!advanceCurrentStream(form))
            return false;
    }
    return true;
}

static void* formCreate(CFReadStreamRef stream, void* context)
{
    FormCreationContext* formContext = static_cast<FormCreationContext*>(context);
    
    FormStreamFields* newInfo = new FormStreamFields(WTFMove(formContext->data));
    newInfo->formStream = stream; // Don't retain. That would create a reference cycle.
    newInfo->streamLength = formContext->streamLength;
    
    callOnMainThread([formContext] {
        delete formContext;
    });

    // Append in reverse order since we remove elements from the end.
    auto& elements = newInfo->data.data().elements();
    size_t size = elements.size();
    newInfo->remainingElements = Vector<FormDataElement>(size, [&](size_t i) {
        return elements[size - i - 1];
    });

    return newInfo;
}

static void formFinalize(CFReadStreamRef stream, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);
    ASSERT_UNUSED(stream, form->formStream == stream);

    callOnMainThread([form] {
        {
            Locker locker { form->streamIsBeingOpenedOrClosedLock };
            closeCurrentStream(form);
        }
        delete form;
    });
}

static Boolean formOpen(CFReadStreamRef, CFStreamError* error, Boolean* openComplete, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    bool opened = openNextStream(form);

    *openComplete = opened;
    error->error = opened ? 0 : fileNotFoundError;
    return opened;
}

static CFIndex formRead(CFReadStreamRef, UInt8* buffer, CFIndex bufferLength, CFStreamError* error, Boolean* atEOF, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    while (form->currentStream) {
        CFIndex bytesToRead = bufferLength;
        if (form->currentStreamRangeLength != BlobDataItem::toEndOfFile && form->currentStreamRangeLength < bytesToRead)
            bytesToRead = static_cast<CFIndex>(form->currentStreamRangeLength);
        CFIndex bytesRead = CFReadStreamRead(form->currentStream.get(), buffer, bytesToRead);
        if (bytesRead < 0) {
            *error = CFReadStreamGetError(form->currentStream.get());
            return -1;
        }
        if (bytesRead > 0) {
            error->error = 0;
            *atEOF = FALSE;
            form->bytesSent += bytesRead;
            if (form->currentStreamRangeLength != BlobDataItem::toEndOfFile)
                form->currentStreamRangeLength -= bytesRead;

            return bytesRead;
        }
        openNextStream(form);
    }

    error->error = 0;
    *atEOF = TRUE;
    return 0;
}

static Boolean formCanRead(CFReadStreamRef stream, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    while (form->currentStream && CFReadStreamGetStatus(form->currentStream.get()) == kCFStreamStatusAtEnd)
        openNextStream(form);

    if (!form->currentStream) {
        CFReadStreamSignalEvent(stream, kCFStreamEventEndEncountered, 0);
        return FALSE;
    }
    return CFReadStreamHasBytesAvailable(form->currentStream.get());
}

static void formClose(CFReadStreamRef, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);
    Locker locker { form->streamIsBeingOpenedOrClosedLock };

    closeCurrentStream(form);
}

static CFTypeRef formCopyProperty(CFReadStreamRef, CFStringRef propertyName, void *context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    if (kCFCompareEqualTo == CFStringCompare(propertyName, formDataPointerPropertyName, 0)) {
        long long formDataAsNumber = static_cast<long long>(reinterpret_cast<intptr_t>(&form->data.data()));
        return CFNumberCreate(0, kCFNumberLongLongType, &formDataAsNumber);
    }

    if (kCFCompareEqualTo == CFStringCompare(propertyName, formDataStreamLengthPropertyName(), 0))
        return CFStringCreateWithFormat(0, 0, CFSTR("%llu"), form->streamLength);

    return 0;
}

static void formSchedule(CFReadStreamRef, CFRunLoopRef runLoop, CFStringRef runLoopMode, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    if (form->currentStream)
        CFReadStreamScheduleWithRunLoop(form->currentStream.get(), runLoop, runLoopMode);
    form->scheduledRunLoopPairs.add(SchedulePair::create(runLoop, runLoopMode));
}

static void formUnschedule(CFReadStreamRef, CFRunLoopRef runLoop, CFStringRef runLoopMode, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    if (form->currentStream)
        CFReadStreamUnscheduleFromRunLoop(form->currentStream.get(), runLoop, runLoopMode);
    form->scheduledRunLoopPairs.remove(SchedulePair::create(runLoop, runLoopMode));
}

static void formEventCallback(CFReadStreamRef stream, CFStreamEventType type, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    switch (type) {
    case kCFStreamEventHasBytesAvailable:
        CFReadStreamSignalEvent(form->formStream, kCFStreamEventHasBytesAvailable, 0);
        break;
    case kCFStreamEventErrorOccurred: {
        CFStreamError readStreamError = CFReadStreamGetError(stream);
        CFReadStreamSignalEvent(form->formStream, kCFStreamEventErrorOccurred, &readStreamError);
        break;
    }
    case kCFStreamEventEndEncountered:
        openNextStream(form);
        if (!form->currentStream)
            CFReadStreamSignalEvent(form->formStream, kCFStreamEventEndEncountered, 0);
        break;
    case kCFStreamEventNone:
        LOG_ERROR("unexpected kCFStreamEventNone");
        break;
    case kCFStreamEventOpenCompleted:
        LOG_ERROR("unexpected kCFStreamEventOpenCompleted");
        break;
    case kCFStreamEventCanAcceptBytes:
        LOG_ERROR("unexpected kCFStreamEventCanAcceptBytes");
        break;
    }
}

RetainPtr<CFReadStreamRef> createHTTPBodyCFReadStream(FormData& formData)
{
    if (!hasPlatformStrategies() && formData.containsBlobElement())
        return nullptr;

    auto resolvedFormData = formData.resolveBlobReferences();
    auto dataForUpload = resolvedFormData->prepareForUpload();

    // Precompute the content length so CFNetwork doesn't use chunked mode.
    unsigned long long length = 0;
    for (auto& element : dataForUpload.data().elements()) {
        length += element.lengthInBytes([](auto& url) {
            return blobRegistry().blobRegistryImpl()->blobSize(url);
        });
    }
    ASSERT(isMainThread());
    FormCreationContext* formContext = new FormCreationContext { WTFMove(dataForUpload), length };
    CFReadStreamCallBacksV1 callBacks = { 1, formCreate, formFinalize, nullptr, formOpen, nullptr, formRead, nullptr, formCanRead, formClose, formCopyProperty, nullptr, nullptr, formSchedule, formUnschedule };
    return adoptCF(CFReadStreamCreate(nullptr, static_cast<const void*>(&callBacks), formContext));
}

void setHTTPBody(CFMutableURLRequestRef request, const RefPtr<FormData>& formData)
{
    if (!formData)
        return;

    // Handle the common special case of one piece of form data, with no files.
    auto& elements = formData->elements();
    if (elements.size() == 1 && !formData->alwaysStream()) {
        if (auto* vector = std::get_if<Vector<uint8_t>>(&elements[0].data)) {
            auto data = adoptCF(CFDataCreate(nullptr, vector->data(), vector->size()));
            CFURLRequestSetHTTPRequestBody(request, data.get());
            return;
        }
    }

    CFURLRequestSetHTTPRequestBodyStream(request, createHTTPBodyCFReadStream(*formData).get());
}

FormData* httpBodyFromStream(CFReadStreamRef stream)
{
    if (!stream)
        return nullptr;

    // Passing the pointer as property appears to be the only way to associate a stream with FormData.
    // A new stream is always created in CFURLRequestCopyHTTPRequestBodyStream (or -[NSURLRequest HTTPBodyStream]),
    // so a side UncheckedKeyHashMap wouldn't work.
    // Even the stream's context pointer is different from the one we returned from formCreate().

    RetainPtr<CFNumberRef> formDataPointerAsCFNumber = adoptCF(static_cast<CFNumberRef>(CFReadStreamCopyProperty(stream, formDataPointerPropertyName)));
    if (!formDataPointerAsCFNumber)
        return nullptr;

    long formDataPointerAsNumber;
    if (!CFNumberGetValue(formDataPointerAsCFNumber.get(), kCFNumberLongType, &formDataPointerAsNumber))
        return nullptr;

    return reinterpret_cast<FormData*>(static_cast<intptr_t>(formDataPointerAsNumber));
}

} // namespace WebCore
