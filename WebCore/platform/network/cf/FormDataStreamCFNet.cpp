/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "FormDataStreamCFNet.h"

#include "FileSystem.h"
#include "FormData.h"
#include <CFNetwork/CFURLRequestPriv.h>
#include <CoreFoundation/CFStreamAbstract.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <sys/types.h>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/CString.h>

#define USE_V1_CFSTREAM_CALLBACKS
#ifdef USE_V1_CFSTREAM_CALLBACKS
typedef CFReadStreamCallBacksV1 WCReadStreamCallBacks;
#else
typedef CFReadStreamCallBacks WCReadStreamCallBacks;
#endif

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
#if PLATFORM(WIN)
        CFURLRef fileURL = CFURLCreateWithFileSystemPath(0, filename, kCFURLWindowsPathStyle, FALSE);
#else
        CFURLRef fileURL = CFURLCreateWithFileSystemPath(0, filename, kCFURLPOSIXPathStyle, FALSE);
#endif
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
    // For some purposes we might want to return an error, but the current CFURLConnection
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
        CFReadStreamSignalEvent(stream, kCFStreamEventEndEncountered, 0);
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

void setHTTPBody(CFMutableURLRequestRef request, PassRefPtr<FormData> formData)
{
    if (!formData) {
        wkCFURLRequestSetHTTPRequestBodyParts(request, 0);
        return;
    }

    size_t count = formData->elements().size();

    if (count == 0)
        return;

    // Handle the common special case of one piece of form data, with no files.
    if (count == 1) {
        const FormDataElement& element = formData->elements()[0];
        if (element.m_type == FormDataElement::data) {
            CFDataRef data = CFDataCreate(0, reinterpret_cast<const UInt8 *>(element.m_data.data()), element.m_data.size());
            CFURLRequestSetHTTPRequestBody(request, data);
            CFRelease(data);
            return;
        }
    }

    RetainPtr<CFMutableArrayRef> array(AdoptCF, CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    for (size_t i = 0; i < count; ++i) {
        const FormDataElement& element = formData->elements()[i];
        if (element.m_type == FormDataElement::data) {
            RetainPtr<CFDataRef> data(AdoptCF, CFDataCreate(0, reinterpret_cast<const UInt8*>(element.m_data.data()), element.m_data.size()));
            CFArrayAppendValue(array.get(), data.get());
        } else {
            RetainPtr<CFStringRef> filename(AdoptCF, element.m_filename.createCFString());
            CFArrayAppendValue(array.get(), filename.get());
        }
    }

    wkCFURLRequestSetHTTPRequestBodyParts(request, array.get());
}

PassRefPtr<FormData> httpBodyFromRequest(CFURLRequestRef request)
{
    if (RetainPtr<CFDataRef> bodyData = CFURLRequestCopyHTTPRequestBody(request))
        return FormData::create(CFDataGetBytePtr(bodyData.get()), CFDataGetLength(bodyData.get()));

    if (RetainPtr<CFArrayRef> bodyParts = wkCFURLRequestCopyHTTPRequestBodyParts(request)) {
        RefPtr<FormData> formData = FormData::create();

        CFIndex count = CFArrayGetCount(bodyParts.get());
        for (CFIndex i = 0; i < count; i++) {
            CFTypeRef bodyPart = CFArrayGetValueAtIndex(bodyParts.get(), i);
            CFTypeID typeID = CFGetTypeID(bodyPart);
            if (typeID == CFStringGetTypeID()) {
                String filename = (CFStringRef)bodyPart;
                formData->appendFile(filename);
            } else if (typeID == CFDataGetTypeID()) {
                CFDataRef data = (CFDataRef)bodyPart;
                formData->appendData(CFDataGetBytePtr(data), CFDataGetLength(data));
            } else
                ASSERT_NOT_REACHED();
        }
        return formData.release();
    }

    // FIXME: what to do about arbitrary body streams?
    return 0;
}

}
