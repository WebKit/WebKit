/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef AutoCorrectionCallback_h
#define AutoCorrectionCallback_h

#include "APIError.h"
#include "GenericCallback.h"
#include "WKAPICast.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class AutocorrectionDataCallback : public CallbackBase {
public:
    typedef void (*CallbackFunction)(const Vector<WebCore::FloatRect>&, const String&, double, uint64_t, WKErrorRef, void*);

    static PassRefPtr<AutocorrectionDataCallback> create(void* context, CallbackFunction callback)
    {
        return adoptRef(new AutocorrectionDataCallback(context, callback));
    }

    virtual ~AutocorrectionDataCallback()
    {
        ASSERT(!m_callback);
    }

    void performCallbackWithReturnValue(const Vector<WebCore::FloatRect>& returnValue1, const String& returnValue2, double returnValue3, uint64_t returnValue4)
    {
        ASSERT(m_callback);

        m_callback(returnValue1, returnValue2, returnValue3, returnValue4, 0, context());

        m_callback = 0;
    }

    void invalidate()
    {
        ASSERT(m_callback);

        RefPtr<API::Error> error = API::Error::create();
        m_callback(Vector<WebCore::FloatRect>(), String(), 0, 0, toAPI(error.get()), context());

        m_callback = 0;
    }

private:
    AutocorrectionDataCallback(void* context, CallbackFunction callback)
        : CallbackBase(context)
        , m_callback(callback)
    {
        ASSERT(m_callback);
    }
    
    CallbackFunction m_callback;
};

class AutocorrectionContextCallback : public CallbackBase {
public:
    typedef void (*CallbackFunction)(const String&, const String&, const String&, const String&, uint64_t, uint64_t, WKErrorRef, void*);

    static PassRefPtr<AutocorrectionContextCallback> create(void* context, CallbackFunction callback)
    {
        return adoptRef(new AutocorrectionContextCallback(context, callback));
    }

    virtual ~AutocorrectionContextCallback()
    {
        ASSERT(!m_callback);
    }

    void performCallbackWithReturnValue(const String& returnValue1, const String& returnValue2, const String& returnValue3, const String& returnValue4, uint64_t returnValue5, uint64_t returnValue6)
    {
        ASSERT(m_callback);

        m_callback(returnValue1, returnValue2, returnValue3, returnValue4, returnValue5, returnValue6, 0, context());

        m_callback = 0;
    }

    void invalidate()
    {
        ASSERT(m_callback);

        RefPtr<API::Error> error = API::Error::create();
        m_callback(String(), String(), String(), String(), 0, 0, toAPI(error.get()), context());

        m_callback = 0;
    }

private:
    AutocorrectionContextCallback(void* context, CallbackFunction callback)
        : CallbackBase(context)
        , m_callback(callback)
    {
        ASSERT(m_callback);
    }
    
    CallbackFunction m_callback;
};

} // namespace WebKit

#endif // AutoCorrectionCallback_h
