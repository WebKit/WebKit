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

#if USE(CFNETWORK)

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

} // namespace WebCore

#endif // USE(CFNETWORK)
