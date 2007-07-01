/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "FormData.h"
#import "WebCoreSystemInterface.h"
#import <sys/stat.h>
#import <sys/types.h>
#import <wtf/Assertions.h>
#import <wtf/HashMap.h>

namespace WebCore {

static HashMap<CFReadStreamRef, RefPtr<FormData> >& getStreamFormDatas()
{
    static HashMap<CFReadStreamRef, RefPtr<FormData> > streamFormDatas;
    return streamFormDatas;
}

static void formEventCallback(CFReadStreamRef stream, CFStreamEventType type, void* context);

struct FormStreamFields {
    CFMutableSetRef scheduledRunLoopPairs;
    Vector<FormDataElement> remainingElements; // in reverse order
    CFReadStreamRef currentStream;
    char* currentData;
    CFReadStreamRef formStream;
};

struct SchedulePair {
    CFRunLoopRef runLoop;
    CFStringRef mode;
};

static const void* pairRetain(CFAllocatorRef alloc, const void* value)
{
    const SchedulePair* pair = static_cast<const SchedulePair*>(value);

    SchedulePair* result = new SchedulePair;
    CFRetain(pair->runLoop);
    result->runLoop = pair->runLoop;
    result->mode = CFStringCreateCopy(alloc, pair->mode);
    return result;
}

static void pairRelease(CFAllocatorRef alloc, const void* value)
{
    const SchedulePair* pair = static_cast<const SchedulePair*>(value);

    CFRelease(pair->runLoop);
    CFRelease(pair->mode);
    delete pair;
}

static Boolean pairEqual(const void* a, const void* b)
{
    const SchedulePair* pairA = static_cast<const SchedulePair*>(a);
    const SchedulePair* pairB = static_cast<const SchedulePair*>(b);

    return pairA->runLoop == pairB->runLoop && CFEqual(pairA->mode, pairB->mode);
}

static CFHashCode pairHash(const void* value)
{
    const SchedulePair* pair = static_cast<const SchedulePair*>(value);

    return (CFHashCode)pair->runLoop ^ CFHash(pair->mode);
}

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

static void scheduleWithPair(const void* value, void* context)
{
    const SchedulePair* pair = static_cast<const SchedulePair*>(value);
    CFReadStreamRef stream = (CFReadStreamRef)context;

    CFReadStreamScheduleWithRunLoop(stream, pair->runLoop, pair->mode);
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
        CFStringRef filename = nextInput.m_filename.createCFString();
        CFURLRef fileURL = CFURLCreateWithFileSystemPath(0, filename, kCFURLPOSIXPathStyle, FALSE);
        CFRelease(filename);
        form->currentStream = CFReadStreamCreateWithFile(0, fileURL);
        CFRelease(fileURL);
    }
    form->remainingElements.removeLast();

    // Set up the callback.
    CFStreamClientContext context = { 0, form, NULL, NULL, NULL };
    CFReadStreamSetClient(form->currentStream, kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered,
        formEventCallback, &context);

    // Schedule with the current set of run loops.
    CFSetApplyFunction(form->scheduledRunLoopPairs, scheduleWithPair, form->currentStream);
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
    FormData* formData = static_cast<FormData*>(context);

    CFSetCallBacks runLoopAndModeCallBacks = { 0, pairRetain, pairRelease, NULL, pairEqual, pairHash };

    FormStreamFields* newInfo = new FormStreamFields;
    newInfo->scheduledRunLoopPairs = CFSetCreateMutable(0, 0, &runLoopAndModeCallBacks);
    newInfo->currentStream = NULL;
    newInfo->currentData = 0;
    newInfo->formStream = stream; // Don't retain. That would create a reference cycle.

    // Append in reverse order since we remove elements from the end.
    size_t size = formData->elements().size();
    newInfo->remainingElements.reserveCapacity(size);
    for (size_t i = 0; i < size; ++i)
        newInfo->remainingElements.append(formData->elements()[size - i - 1]);

    getStreamFormDatas().set(stream, adoptRef(formData));

    return newInfo;
}

static void formFinalize(CFReadStreamRef stream, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    getStreamFormDatas().remove(stream);

    closeCurrentStream(form);
    CFRelease(form->scheduledRunLoopPairs);
    delete form;
}

static Boolean formOpen(CFReadStreamRef stream, CFStreamError* error, Boolean* openComplete, void* context)
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

static void formClose(CFReadStreamRef stream, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    closeCurrentStream(form);
}

static void formSchedule(CFReadStreamRef stream, CFRunLoopRef runLoop, CFStringRef runLoopMode, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    if (form->currentStream)
        CFReadStreamScheduleWithRunLoop(form->currentStream, runLoop, runLoopMode);
    SchedulePair pair = { runLoop, runLoopMode };
    CFSetAddValue(form->scheduledRunLoopPairs, &pair);
}

static void formUnschedule(CFReadStreamRef stream, CFRunLoopRef runLoop, CFStringRef runLoopMode, void* context)
{
    FormStreamFields* form = static_cast<FormStreamFields*>(context);

    if (form->currentStream)
        CFReadStreamUnscheduleFromRunLoop(form->currentStream, runLoop, runLoopMode);
    SchedulePair pair = { runLoop, runLoopMode };
    CFSetRemoveValue(form->scheduledRunLoopPairs, &pair);
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
    if (count == 1) {
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
            struct stat sb;
            int statResult = stat(element.m_filename.utf8().data(), &sb);
            if (statResult == 0 && (sb.st_mode & S_IFMT) == S_IFREG)
                length += sb.st_size;
        }
    }

    // Set the length.
    [request setValue:[NSString stringWithFormat:@"%lld", length] forHTTPHeaderField:@"Content-Length"];

    // Create and set the stream.
    CFReadStreamRef stream = wkCreateCustomCFReadStream(formCreate, formFinalize,
        formOpen, formRead, formCanRead, formClose, formSchedule, formUnschedule,
        formData.releaseRef());
    [request setHTTPBodyStream:(NSInputStream *)stream];
    CFRelease(stream);
}


FormData* httpBodyFromStream(NSInputStream* stream)
{
    return getStreamFormDatas().get((CFReadStreamRef)stream).get();
}

}
