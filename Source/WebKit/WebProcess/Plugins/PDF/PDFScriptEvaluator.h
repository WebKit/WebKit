/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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

#if ENABLE(PDF_PLUGIN)

#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class PDFScriptEvaluatorClient;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::PDFScriptEvaluatorClient> : std::true_type { };
}

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;
typedef const struct OpaqueJSValue* JSValueRef;
typedef struct OpaqueJSClass* JSClassRef;

namespace WebKit {

class PDFScriptEvaluatorClient : public CanMakeWeakPtr<PDFScriptEvaluatorClient> {
public:
    virtual ~PDFScriptEvaluatorClient() = default;

    virtual void print() = 0;
};

class PDFScriptEvaluator : public RefCounted<PDFScriptEvaluator> {
    WTF_MAKE_TZONE_ALLOCATED(PDFScriptEvaluator);
    WTF_MAKE_NONCOPYABLE(PDFScriptEvaluator);
public:
    static void runScripts(CGPDFDocumentRef, PDFScriptEvaluatorClient&);

private:
    PDFScriptEvaluator() = delete;
    PDFScriptEvaluator(CGPDFDocumentRef pdfDocument, PDFScriptEvaluatorClient& client)
        : m_pdfDocument(pdfDocument)
        , m_client(client)
    {
    }

    static Ref<PDFScriptEvaluator> create(CGPDFDocumentRef pdfDocument, PDFScriptEvaluatorClient& client)
    {
        return adoptRef(*new PDFScriptEvaluator(pdfDocument, client));
    }

    static JSClassRef jsPDFDocClass();
    static JSValueRef jsPDFDocPrint(JSContextRef, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    void print();

    RetainPtr<CGPDFDocumentRef> m_pdfDocument;
    WeakPtr<PDFScriptEvaluatorClient> m_client;
};

}

#endif // ENABLE(PDF_PLUGIN)
