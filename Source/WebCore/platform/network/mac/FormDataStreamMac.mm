/*
 * Copyright (C) 2005, 2006, 2008, 2011 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

/* originally written by Becky Willrich, additional code by Darin Adler */

#import "config.h"
#import "FormDataStreamMac.h"

#if !USE(CFNETWORK)

#import "BlobRegistryImpl.h"
#import "FileSystem.h"
#import "FormData.h"
#import "ResourceHandle.h"
#import "ResourceHandleClient.h"
#import "SchedulePair.h"
#import "WebCoreSystemInterface.h"
#import <sys/stat.h>
#import <sys/types.h>
#import <wtf/Assertions.h>
#import <wtf/HashMap.h>
#import <wtf/MainThread.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>

#if PLATFORM(IOS)
#import <MacErrors.h>
#else
#import <CoreServices/CoreServices.h>
#endif

namespace WebCore {

static Mutex& streamFieldsMapMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, staticMutex, ());
    return staticMutex;
}

static NSMapTable *streamFieldsMap()
{
    static NSMapTable *streamFieldsMap = NSCreateMapTable(NSNonRetainedObjectMapKeyCallBacks, NSNonOwnedPointerMapValueCallBacks, 1);
    return streamFieldsMap;
}

typedef HashMap<CFReadStreamRef, RefPtr<ResourceHandle> > StreamResourceHandleMap;
static StreamResourceHandleMap& getStreamResourceHandleMap()
{
    DEFINE_STATIC_LOCAL(StreamResourceHandleMap, streamResourceHandleMap, ());
    return streamResourceHandleMap;
}

void associateStreamWithResourceHandle(NSInputStream *stream, ResourceHandle* resourceHandle)
{
    ASSERT(isMainThread());

    ASSERT(resourceHandle);

    if (!stream)
        return;

    {
        MutexLocker locker(streamFieldsMapMutex());
        if (!NSMapGet(streamFieldsMap(), stream))
            return;
    }

    ASSERT(!getStreamResourceHandleMap().contains((CFReadStreamRef)stream));
    getStreamResourceHandleMap().set((CFReadStreamRef)stream, resourceHandle);
}

void disassociateStreamWithResourceHandle(NSInputStream *stream)
{
    ASSERT(isMainThread());

    if (!stream)
        return;

    getStreamResourceHandleMap().remove((CFReadStreamRef)stream);
}

struct DidSendDataCallbackData {
    DidSendDataCallbackData(CFReadStreamRef stream_, unsigned long long bytesSent_, unsigned long long streamLength_)
        : stream(stream_)
        , bytesSent(bytesSent_)
        , streamLength(streamLength_)
    {
    }

    CFReadStreamRef stream;
    unsigned long long bytesSent;
    unsigned long long streamLength;
};

static void performDidSendDataCallback(void* context)
{
    ASSERT(isMainThread());

    DidSendDataCallbackData* data = static_cast<DidSendDataCallbackData*>(context);
    ResourceHandle* resourceHandle = getStreamResourceHandleMap().get(data->stream).get();

    if (resourceHandle && resourceHandle->client())
        resourceHandle->client()->didSendData(resourceHandle, data->bytesSent, data->streamLength);

    delete data;
}

static void formEventCallback(CFReadStreamRef stream, CFStreamEventType type, void* context);

struct FormContext {
    RefPtr<FormData> formData;
    unsigned long long streamLength;
};

struct FormStreamFields {
    RefPtr<FormData> formData;
    SchedulePairHashSet scheduledRunLoopPairs;
    Vector<FormDataElement> remainingElements; // in reverse order
    CFReadStreamRef currentStream;
#if ENABLE(BLOB)
    long long currentStreamRangeLength;
#endif
    char* currentData;
    CFReadStreamRef formStream;
    unsigned long long streamLength;
    unsigned long long bytesSent;
};

static void closeCurrentStream(FormStreamFields *form)
{
    if (form->currentStream) {
        CFReadStreamClose(form->currentStream);
        CFReadStreamSetClient(form->currentStream, kCFStreamEventNone, NULL, NULL);
        CFRelease(form->currentStream);
        form->currentStream = NULL;
#if ENABLE(BLOB)
        form->currentStreamRangeLength = BlobDataItem::toEndOfFile;
#endif
    }
    if (form->currentData) {
        fastFree(form->currentData);
        form->currentData = 0;
    }
}

// Return false if we cannot advance the stream. Currently the only possible failure is that the underlying file has been removed or changed since File.slice.
static bool advanceCurrentStream(FormStreamFields* form)
{
    closeCurrentStream(form);

    if (form->remainingElements.isEmpty())
        return true;

    // Create the new stream.
    FormDataElement& nextInput = form->remainingElements.last();

    if (nextInput.m_type == FormDataElement::data) {
        size_t size = nextInput.m_data.size();
        char* data = nextInput.m_data.releaseBuffer();
        form->currentStream = CFReadStreamCreateWithBytesNoCopy(0, reinterpret_cast<const UInt8*>(data), size, kCFAllocatorNull);
        form->currentData = data;
    } else {
#if ENABLE(BLOB)
        // Check if the file has been changed or not if required.
        if (nextInput.m_expectedFileModificationTime != BlobDataItem::doNotCheckFileChange) {
            time_t fileModificationTime;
            if (!getFileModificationTime(nextInput.m_filename, fileModificationTime) || fileModificationTime != static_cast<time_t>(nextInput.m_expectedFileModificationTime))
                return false;
        }
#endif
        const String& path = nextInput.m_shouldGenerateFile ? nextInput.m_generatedFilename : nextInput.m_filename;
        form->currentStream = CFReadStreamCreateWithFile(0, pathAsURL(path).get());
        if (!form->currentStream) {
            // The file must have been removed or become unreadable.
            return false;
        }
#if ENABLE(BLOB)
        if (nextInput.m_fileStart > 0) {
            RetainPtr<CFNumberRef> position(AdoptCF, CFNumberCreate(0, kCFNumberLongLongType, &nextInput.m_fileStart));
            CFReadStreamSetProperty(form->currentStream, kCFStreamPropertyFileCurrentOffset, position.get());
        }
        form->currentStreamRangeLength = nextInput.m_fileLength;
#endif
    }
    form->remainingElements.removeLast();

    // Set up the callback.
    CFStreamClientContext context = { 0, form, NULL, NULL, NULL };
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
    FormContext* formContext = static_cast<FormContext*>(context);

    FormStreamFields* newInfo = new FormStreamFields;
    newInfo->formData = formContext->formData.release();
    newInfo->currentStream = NULL;
#if ENABLE(BLOB)
    newInfo->currentStreamRangeLength = BlobDataItem::toEndOfFile;
#endif
    newInfo->currentData = 0;
    newInfo->formStream = stream; // Don't retain. That would create a reference cycle.
    newInfo->streamLength = formContext->streamLength;
    newInfo->bytesSent = 0;

    // Append in reverse order since we remove elements from the end.
    size_t size = newInfo->formData->elements().size();
    newInfo->remainingElements.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        newInfo->remainingElements.append(newInfo->formData->elements()[size - i - 1]);

    MutexLocker locker(streamFieldsMapMutex());
    ASSERT(!NSMapGet(streamFieldsMap(), stream));
    NSMapInsertKnownAbsent(streamFieldsMap(), stream, newInfo);

    return newInfo;
}

static void formFinishFinalizationOnMainThread(void* context)
{
    OwnPtr<FormStreamFields> form = adoptPtr(static_cast<FormStreamFields*>(context));

    closeCurrentStream(form.get());
}

static void formFinalize(CFReadStreamRef stream, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    MutexLocker locker(streamFieldsMapMutex());

    ASSERT(form->formStream == stream);
    ASSERT(NSMapGet(streamFieldsMap(), stream) == context);

    // Do this right away because the CFReadStreamRef is being deallocated.
    // We can't wait to remove this from the map until we finish finalizing
    // on the main thread because in theory the freed memory could be reused
    // for a new CFReadStream before that runs.
    NSMapRemove(streamFieldsMap(), stream);

    callOnMainThread(formFinishFinalizationOnMainThread, form);
}

static Boolean formOpen(CFReadStreamRef, CFStreamError* error, Boolean* openComplete, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    bool opened = openNextStream(form);

    *openComplete = opened;
    error->error = opened ? 0 : fnfErr;
    return opened;
}

static CFIndex formRead(CFReadStreamRef stream, UInt8* buffer, CFIndex bufferLength, CFStreamError* error, Boolean* atEOF, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    while (form->currentStream) {
        CFIndex bytesToRead = bufferLength;
#if ENABLE(BLOB)
        if (form->currentStreamRangeLength != BlobDataItem::toEndOfFile && form->currentStreamRangeLength < bytesToRead)
            bytesToRead = static_cast<CFIndex>(form->currentStreamRangeLength);
#endif
        CFIndex bytesRead = CFReadStreamRead(form->currentStream, buffer, bytesToRead);
        if (bytesRead < 0) {
            *error = CFReadStreamGetError(form->currentStream);
            return -1;
        }
        if (bytesRead > 0) {
            error->error = 0;
            *atEOF = FALSE;
            form->bytesSent += bytesRead;
#if ENABLE(BLOB)
            if (form->currentStreamRangeLength != BlobDataItem::toEndOfFile)
                form->currentStreamRangeLength -= bytesRead;
#endif

            if (!ResourceHandle::didSendBodyDataDelegateExists()) {
                // FIXME: Figure out how to only do this when a ResourceHandleClient is available.
                DidSendDataCallbackData* data = new DidSendDataCallbackData(stream, form->bytesSent, form->streamLength);
                callOnMainThread(performDidSendDataCallback, data);
            }

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

    while (form->currentStream && CFReadStreamGetStatus(form->currentStream) == kCFStreamStatusAtEnd) {
        openNextStream(form);
    }
    if (!form->currentStream) {
        wkSignalCFReadStreamEnd(stream);
        return FALSE;
    }
    return CFReadStreamHasBytesAvailable(form->currentStream);
}

static void formClose(CFReadStreamRef, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    closeCurrentStream(form);
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
        wkSignalCFReadStreamHasBytes(form->formStream);
        break;
    case kCFStreamEventErrorOccurred: {
        CFStreamError readStreamError = CFReadStreamGetError(stream);
        wkSignalCFReadStreamError(form->formStream, &readStreamError);
        break;
    }
    case kCFStreamEventEndEncountered:
        openNextStream(form);
        if (!form->currentStream) {
            wkSignalCFReadStreamEnd(form->formStream);
        }
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

void setHTTPBody(NSMutableURLRequest *request, PassRefPtr<FormData> prpFormData)
{
    RefPtr<FormData> formData = prpFormData;

    if (!formData)
        return;
        
    size_t count = formData->elements().size();

    // Handle the common special case of one piece of form data, with no files.
    if (count == 1 && !formData->alwaysStream()) {
        const FormDataElement& element = formData->elements()[0];
        if (element.m_type == FormDataElement::data) {
            NSData *data = [[NSData alloc] initWithBytes:element.m_data.data() length:element.m_data.size()];
            [request setHTTPBody:data];
            [data release];
            return;
        }
    }

#if ENABLE(BLOB)
    // Check if there is a blob in the form data.
    bool hasBlob = false;
    for (size_t i = 0; i < count; ++i) {
        const FormDataElement& element = formData->elements()[i];
        if (element.m_type == FormDataElement::encodedBlob) {
            hasBlob = true;
            break;
        }
    }

    // If yes, we have to resolve all the blob references and regenerate the form data with only data and file types.
    if (hasBlob) {
        RefPtr<FormData> newFormData = FormData::create();
        newFormData->setAlwaysStream(formData->alwaysStream());
        newFormData->setIdentifier(formData->identifier());
        for (size_t i = 0; i < count; ++i) {
            const FormDataElement& element = formData->elements()[i];
            if (element.m_type == FormDataElement::data)
                newFormData->appendData(element.m_data.data(), element.m_data.size());
            else if (element.m_type == FormDataElement::encodedFile)
                newFormData->appendFile(element.m_filename, element.m_shouldGenerateFile);
            else {
                ASSERT(element.m_type == FormDataElement::encodedBlob);
                RefPtr<BlobStorageData> blobData = static_cast<BlobRegistryImpl&>(blobRegistry()).getBlobDataFromURL(KURL(ParsedURLString, element.m_blobURL));
                if (blobData) {
                    for (size_t j = 0; j < blobData->items().size(); ++j) {
                        const BlobDataItem& blobItem = blobData->items()[j];
                        if (blobItem.type == BlobDataItem::Data) {
                            newFormData->appendData(blobItem.data->data() + static_cast<int>(blobItem.offset), static_cast<int>(blobItem.length));
                        } else {
                            ASSERT(blobItem.type == BlobDataItem::File);
                            newFormData->appendFileRange(blobItem.path, blobItem.offset, blobItem.length, blobItem.expectedModificationTime);
                        }
                    }
                }
            }
        }
        formData = newFormData.release();
        count = formData->elements().size();
    }
#endif

    // Precompute the content length so NSURLConnection doesn't use chunked mode.
    long long length = 0;
    for (size_t i = 0; i < count; ++i) {
        const FormDataElement& element = formData->elements()[i];
        if (element.m_type == FormDataElement::data)
            length += element.m_data.size();
        else {
#if ENABLE(BLOB)
            // If we're sending the file range, use the existing range length for now. We will detect if the file has been changed right before we read the file and abort the operation if necessary.
            if (element.m_fileLength != BlobDataItem::toEndOfFile) {
                length += element.m_fileLength;
                continue;
            }
#endif
            long long fileSize;
            if (getFileSize(element.m_shouldGenerateFile ? element.m_generatedFilename : element.m_filename, fileSize))
                length += fileSize;
        }
    }

    // Set the length.
    [request setValue:[NSString stringWithFormat:@"%lld", length] forHTTPHeaderField:@"Content-Length"];

    // Create and set the stream.

    // Pass the length along with the formData so it does not have to be recomputed.
    FormContext formContext = { formData.release(), length };

    RetainPtr<CFReadStreamRef> stream(AdoptCF, wkCreateCustomCFReadStream(formCreate, formFinalize,
        formOpen, formRead, formCanRead, formClose, formSchedule, formUnschedule,
        &formContext));
    [request setHTTPBodyStream:(NSInputStream *)stream.get()];
}

FormData* httpBodyFromStream(NSInputStream* stream)
{
    MutexLocker locker(streamFieldsMapMutex());
    FormStreamFields* formStream = static_cast<FormStreamFields*>(NSMapGet(streamFieldsMap(), stream));
    if (!formStream)
        return 0;
    return formStream->formData.get();
}

} // namespace WebCore

#endif // !USE(CFNETWORK)
