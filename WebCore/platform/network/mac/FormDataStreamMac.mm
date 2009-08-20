/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc.  All rights reserved.
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

#import "CString.h"
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

namespace WebCore {

typedef HashMap<CFReadStreamRef, RefPtr<FormData> > StreamFormDataMap;
static StreamFormDataMap& getStreamFormDataMap()
{
    DEFINE_STATIC_LOCAL(StreamFormDataMap, streamFormDataMap, ());
    return streamFormDataMap;
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

    if (!getStreamFormDataMap().contains((CFReadStreamRef)stream))
        return;

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
    FormData* formData;
    unsigned long long streamLength;
};

struct FormStreamFields {
    SchedulePairHashSet scheduledRunLoopPairs;
    Vector<FormDataElement> remainingElements; // in reverse order
    CFReadStreamRef currentStream;
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
    }
    if (form->currentData) {
        fastFree(form->currentData);
        form->currentData = 0;
    }
}

static void advanceCurrentStream(FormStreamFields *form)
{
    closeCurrentStream(form);

    if (form->remainingElements.isEmpty())
        return;

    // Create the new stream.
    FormDataElement& nextInput = form->remainingElements.last();
    if (nextInput.m_type == FormDataElement::data) {
        size_t size = nextInput.m_data.size();
        char* data = nextInput.m_data.releaseBuffer();
        form->currentStream = CFReadStreamCreateWithBytesNoCopy(0, reinterpret_cast<const UInt8*>(data), size, kCFAllocatorNull);
        form->currentData = data;
    } else {
        const String& path = nextInput.m_shouldGenerateFile ? nextInput.m_generatedFilename : nextInput.m_filename;
        RetainPtr<CFStringRef> filename(AdoptCF, path.createCFString());
        RetainPtr<CFURLRef> fileURL(AdoptCF, CFURLCreateWithFileSystemPath(0, filename.get(), kCFURLPOSIXPathStyle, FALSE));
        form->currentStream = CFReadStreamCreateWithFile(0, fileURL.get());
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
}

static void openNextStream(FormStreamFields* form)
{
    // Skip over any streams we can't open.
    // For some purposes we might want to return an error, but the current NSURLConnection
    // can't really do anything useful with an error at this point, so this is better.
    advanceCurrentStream(form);
    while (form->currentStream && !CFReadStreamOpen(form->currentStream))
        advanceCurrentStream(form);
}

static void* formCreate(CFReadStreamRef stream, void* context)
{
    FormContext* formContext = static_cast<FormContext*>(context);

    FormStreamFields* newInfo = new FormStreamFields;
    newInfo->currentStream = NULL;
    newInfo->currentData = 0;
    newInfo->formStream = stream; // Don't retain. That would create a reference cycle.
    newInfo->streamLength = formContext->streamLength;
    newInfo->bytesSent = 0;

    FormData* formData = formContext->formData;

    // Append in reverse order since we remove elements from the end.
    size_t size = formData->elements().size();
    newInfo->remainingElements.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        newInfo->remainingElements.append(formData->elements()[size - i - 1]);

    getStreamFormDataMap().set(stream, adoptRef(formData));

    return newInfo;
}

static void formFinalize(CFReadStreamRef stream, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    getStreamFormDataMap().remove(stream);

    closeCurrentStream(form);
    delete form;
}

static Boolean formOpen(CFReadStreamRef, CFStreamError* error, Boolean* openComplete, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    openNextStream(form);

    *openComplete = TRUE;
    error->error = 0;
    return TRUE;
}

static CFIndex formRead(CFReadStreamRef stream, UInt8* buffer, CFIndex bufferLength, CFStreamError* error, Boolean* atEOF, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    while (form->currentStream) {
        CFIndex bytesRead = CFReadStreamRead(form->currentStream, buffer, bufferLength);
        if (bytesRead < 0) {
            *error = CFReadStreamGetError(form->currentStream);
            return -1;
        }
        if (bytesRead > 0) {
            error->error = 0;
            *atEOF = FALSE;
            form->bytesSent += bytesRead;

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

void setHTTPBody(NSMutableURLRequest *request, PassRefPtr<FormData> formData)
{
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

    // Precompute the content length so NSURLConnection doesn't use chunked mode.
    long long length = 0;
    for (size_t i = 0; i < count; ++i) {
        const FormDataElement& element = formData->elements()[i];
        if (element.m_type == FormDataElement::data)
            length += element.m_data.size();
        else {
            long long fileSize;
            if (getFileSize(element.m_shouldGenerateFile ? element.m_generatedFilename : element.m_filename, fileSize))
                length += fileSize;
        }
    }

    // Set the length.
    [request setValue:[NSString stringWithFormat:@"%lld", length] forHTTPHeaderField:@"Content-Length"];

    // Create and set the stream.

    // Pass the length along with the formData so it does not have to be recomputed.
    FormContext formContext = { formData.releaseRef(), length };

    RetainPtr<CFReadStreamRef> stream(AdoptCF, wkCreateCustomCFReadStream(formCreate, formFinalize,
        formOpen, formRead, formCanRead, formClose, formSchedule, formUnschedule,
        &formContext));
    [request setHTTPBodyStream:(NSInputStream *)stream.get()];
}

FormData* httpBodyFromStream(NSInputStream* stream)
{
    return getStreamFormDataMap().get((CFReadStreamRef)stream).get();
}

} // namespace WebCore
