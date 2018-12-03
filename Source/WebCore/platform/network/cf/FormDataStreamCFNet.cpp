/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
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

#include "BlobData.h"
#include "FileSystem.h"
#include "FormData.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/SchedulePair.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>

static const SInt32 fileNotFoundError = -43;

#if PLATFORM(COCOA)
extern "C" void CFURLRequestSetHTTPRequestBody(CFMutableURLRequestRef mutableHTTPRequest, CFDataRef httpBody);
extern "C" void CFURLRequestSetHTTPHeaderFieldValue(CFMutableURLRequestRef mutableHTTPRequest, CFStringRef httpHeaderField, CFStringRef httpHeaderFieldValue);
extern "C" void CFURLRequestSetHTTPRequestBodyStream(CFMutableURLRequestRef req, CFReadStreamRef bodyStream);
#elif PLATFORM(WIN)
#include <CFNetwork/CFURLRequest.h>
#endif

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

#if PLATFORM(WIN)
#define EXTERN extern "C" __declspec(dllimport)
#else
#define EXTERN extern "C"
#endif

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
    RefPtr<FormData> formData;
    unsigned long long streamLength;
};

struct FormStreamFields {
    RefPtr<FormData> formData;
    SchedulePairHashSet scheduledRunLoopPairs;
    Vector<FormDataElement> remainingElements; // in reverse order
    CFReadStreamRef currentStream;
    long long currentStreamRangeLength;
    MallocPtr<char> currentData;
    CFReadStreamRef formStream;
    unsigned long long streamLength;
    unsigned long long bytesSent;
    Lock streamIsBeingOpenedOrClosedLock;
};

static void closeCurrentStream(FormStreamFields* form)
{
    ASSERT(form->streamIsBeingOpenedOrClosedLock.isHeld());

    if (form->currentStream) {
        CFReadStreamClose(form->currentStream);
        CFReadStreamSetClient(form->currentStream, kCFStreamEventNone, 0, 0);
        CFRelease(form->currentStream);
        form->currentStream = 0;
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

    bool success = switchOn(nextInput.data,
        [form] (Vector<char>& bytes) {
            size_t size = bytes.size();
            MallocPtr<char> data = bytes.releaseBuffer();
            form->currentStream = CFReadStreamCreateWithBytesNoCopy(0, reinterpret_cast<const UInt8*>(data.get()), size, kCFAllocatorNull);
            form->currentData = WTFMove(data);
            return true;
        }, [form] (const FormDataElement::EncodedFileData& fileData) {
            // Check if the file has been changed or not if required.
            if (fileData.expectedFileModificationTime) {
                auto fileModificationTime = FileSystem::getFileModificationTime(fileData.filename);
                if (!fileModificationTime)
                    return false;
                if (fileModificationTime->secondsSinceEpoch().secondsAs<time_t>() != fileData.expectedFileModificationTime->secondsSinceEpoch().secondsAs<time_t>())
                    return false;
            }
            const String& path = fileData.shouldGenerateFile ? fileData.generatedFilename : fileData.filename;
            form->currentStream = CFReadStreamCreateWithFile(0, FileSystem::pathAsURL(path).get());
            if (!form->currentStream) {
                // The file must have been removed or become unreadable.
                return false;
            }
            if (fileData.fileStart > 0) {
                RetainPtr<CFNumberRef> position = adoptCF(CFNumberCreate(0, kCFNumberLongLongType, &fileData.fileStart));
                CFReadStreamSetProperty(form->currentStream, kCFStreamPropertyFileCurrentOffset, position.get());
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

    form->remainingElements.removeLast();

    // Set up the callback.
    CFStreamClientContext context = { 0, form, 0, 0, 0 };
    CFReadStreamSetClient(form->currentStream, kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered,
        formEventCallback, &context);

    // Schedule with the current set of run loops.
    SchedulePairHashSet::iterator end = form->scheduledRunLoopPairs.end();
    for (SchedulePairHashSet::iterator it = form->scheduledRunLoopPairs.begin(); it != end; ++it)
        CFReadStreamScheduleWithRunLoop(form->currentStream, (*it)->runLoop(), (*it)->mode());

    return true;
}

static bool openNextStream(FormStreamFields* form)
{
    // CFReadStreamOpen() can cause this function to be re-entered from another thread before it returns.
    // One example when this can occur is when the stream being opened has no data. See <rdar://problem/23550269>.
    LockHolder locker(form->streamIsBeingOpenedOrClosedLock);

    // Skip over any streams we can't open.
    if (!advanceCurrentStream(form))
        return false;
    while (form->currentStream && !CFReadStreamOpen(form->currentStream)) {
        if (!advanceCurrentStream(form))
            return false;
    }
    return true;
}

static void* formCreate(CFReadStreamRef stream, void* context)
{
    FormCreationContext* formContext = static_cast<FormCreationContext*>(context);

    FormStreamFields* newInfo = new FormStreamFields;
    newInfo->formData = WTFMove(formContext->formData);
    newInfo->currentStream = 0;
    newInfo->currentStreamRangeLength = BlobDataItem::toEndOfFile;
    newInfo->formStream = stream; // Don't retain. That would create a reference cycle.
    newInfo->streamLength = formContext->streamLength;
    newInfo->bytesSent = 0;

    // Append in reverse order since we remove elements from the end.
    size_t size = newInfo->formData->elements().size();
    newInfo->remainingElements.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        newInfo->remainingElements.uncheckedAppend(newInfo->formData->elements()[size - i - 1]);

    return newInfo;
}

static void formFinalize(CFReadStreamRef stream, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);
    ASSERT_UNUSED(stream, form->formStream == stream);

    callOnMainThread([form] {
        {
            LockHolder locker(form->streamIsBeingOpenedOrClosedLock);
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
    error->error = opened ? 0 :
#if PLATFORM(WIN)
        ENOENT;
#else
        fileNotFoundError;
#endif
    return opened;
}

static CFIndex formRead(CFReadStreamRef, UInt8* buffer, CFIndex bufferLength, CFStreamError* error, Boolean* atEOF, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    while (form->currentStream) {
        CFIndex bytesToRead = bufferLength;
        if (form->currentStreamRangeLength != BlobDataItem::toEndOfFile && form->currentStreamRangeLength < bytesToRead)
            bytesToRead = static_cast<CFIndex>(form->currentStreamRangeLength);
        CFIndex bytesRead = CFReadStreamRead(form->currentStream, buffer, bytesToRead);
        if (bytesRead < 0) {
            *error = CFReadStreamGetError(form->currentStream);
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

    while (form->currentStream && CFReadStreamGetStatus(form->currentStream) == kCFStreamStatusAtEnd)
        openNextStream(form);

    if (!form->currentStream) {
        CFReadStreamSignalEvent(stream, kCFStreamEventEndEncountered, 0);
        return FALSE;
    }
    return CFReadStreamHasBytesAvailable(form->currentStream);
}

static void formClose(CFReadStreamRef, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);
    LockHolder locker(form->streamIsBeingOpenedOrClosedLock);

    closeCurrentStream(form);
}

static CFTypeRef formCopyProperty(CFReadStreamRef, CFStringRef propertyName, void *context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    if (kCFCompareEqualTo == CFStringCompare(propertyName, formDataPointerPropertyName, 0)) {
        long long formDataAsNumber = static_cast<long long>(reinterpret_cast<intptr_t>(form->formData.get()));
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
        CFReadStreamScheduleWithRunLoop(form->currentStream, runLoop, runLoopMode);
    form->scheduledRunLoopPairs.add(SchedulePair::create(runLoop, runLoopMode));
}

static void formUnschedule(CFReadStreamRef, CFRunLoopRef runLoop, CFStringRef runLoopMode, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    if (form->currentStream)
        CFReadStreamUnscheduleFromRunLoop(form->currentStream, runLoop, runLoopMode);
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
    auto resolvedFormData = formData.resolveBlobReferences();

    // Precompute the content length so CFNetwork doesn't use chunked mode.
    unsigned long long length = 0;
    for (auto& element : resolvedFormData->elements())
        length += element.lengthInBytes();

    FormCreationContext formContext = { WTFMove(resolvedFormData), length };
    CFReadStreamCallBacksV1 callBacks = { 1, formCreate, formFinalize, nullptr, formOpen, nullptr, formRead, nullptr, formCanRead, formClose, formCopyProperty, nullptr, nullptr, formSchedule, formUnschedule };
    return adoptCF(CFReadStreamCreate(nullptr, static_cast<const void*>(&callBacks), &formContext));
}

void setHTTPBody(CFMutableURLRequestRef request, FormData* formData)
{
    if (!formData)
        return;

    // Handle the common special case of one piece of form data, with no files.
    auto& elements = formData->elements();
    if (elements.size() == 1 && !formData->alwaysStream()) {
        if (auto* vector = WTF::get_if<Vector<char>>(elements[0].data)) {
            auto data = adoptCF(CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(vector->data()), vector->size()));
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
    // so a side HashMap wouldn't work.
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
