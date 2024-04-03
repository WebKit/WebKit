/*
 * Copyright (C) 2009-2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PDFScriptEvaluator.h"

#if ENABLE(PDF_PLUGIN)

#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/JSObjectRef.h>
#import <JavaScriptCore/OpaqueJSString.h>

namespace WebKit {

static void appendValuesInPDFNameSubtreeToVector(RetainPtr<CGPDFDictionaryRef> subtree, Vector<CGPDFObjectRef>& values)
{
    CGPDFArrayRef names = nullptr;
    if (CGPDFDictionaryGetArray(subtree.get(), "Names", &names)) {
        size_t nameCount = CGPDFArrayGetCount(names) / 2;
        for (size_t i = 0; i < nameCount; ++i) {
            CGPDFObjectRef object = nullptr;
            CGPDFArrayGetObject(names, 2 * i + 1, &object);
            values.append(object);
        }
        return;
    }

    CGPDFArrayRef kids = nullptr;
    if (!CGPDFDictionaryGetArray(subtree.get(), "Kids", &kids))
        return;

    size_t kidCount = CGPDFArrayGetCount(kids);
    for (size_t i = 0; i < kidCount; ++i) {
        CGPDFDictionaryRef kid = nullptr;
        if (!CGPDFArrayGetDictionary(kids, i, &kid))
            continue;
        appendValuesInPDFNameSubtreeToVector(kid, values);
    }
}

static void getAllScriptsInPDFDocument(RetainPtr<CGPDFDocumentRef> pdfDocument, Vector<RetainPtr<CFStringRef>>& scripts)
{
    if (!pdfDocument)
        return;

    CGPDFDictionaryRef pdfCatalog = CGPDFDocumentGetCatalog(pdfDocument.get());
    if (!pdfCatalog)
        return;

    // Get the dictionary of all document-level name trees.
    CGPDFDictionaryRef namesDictionary = nullptr;
    if (!CGPDFDictionaryGetDictionary(pdfCatalog, "Names", &namesDictionary))
        return;

    // Get the document-level "JavaScript" name tree.
    CGPDFDictionaryRef javaScriptNameTree = nullptr;
    if (!CGPDFDictionaryGetDictionary(namesDictionary, "JavaScript", &javaScriptNameTree))
        return;

    // The names are arbitrary. We are only interested in the values.
    Vector<CGPDFObjectRef> objects;
    appendValuesInPDFNameSubtreeToVector(javaScriptNameTree, objects);

    for (auto object : objects) {
        CGPDFDictionaryRef javaScriptAction = nullptr;
        if (!CGPDFObjectGetValue(object, kCGPDFObjectTypeDictionary, &javaScriptAction))
            continue;

        // A JavaScript action must have an action type of "JavaScript".
        const char* actionType = nullptr;
        if (!CGPDFDictionaryGetName(javaScriptAction, "S", &actionType) || strcmp(actionType, "JavaScript"))
            continue;

        auto scriptFromBytes = [](const UInt8* bytes, CFIndex length) {
            CFStringEncoding encoding = (length > 1 && bytes[0] == 0xFE && bytes[1] == 0xFF) ? kCFStringEncodingUnicode : kCFStringEncodingUTF8;
            return adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, bytes, length, encoding, true));
        };

        auto scriptFromStream = [&]() -> RetainPtr<CFStringRef> {
            CGPDFStreamRef stream = nullptr;
            if (!CGPDFDictionaryGetStream(javaScriptAction, "JS", &stream))
                return nullptr;
            CGPDFDataFormat format;
            RetainPtr<CFDataRef> data = adoptCF(CGPDFStreamCopyData(stream, &format));
            if (!data)
                return nullptr;
            return scriptFromBytes(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
        };

        auto scriptFromString = [&]() -> RetainPtr<CFStringRef> {
            CGPDFStringRef string = nullptr;
            if (!CGPDFDictionaryGetString(javaScriptAction, "JS", &string))
                return nullptr;
            return scriptFromBytes(CGPDFStringGetBytePtr(string), CGPDFStringGetLength(string));
        };

        RetainPtr<CFStringRef> script = scriptFromStream() ?: scriptFromString();
        if (!script)
            continue;

        scripts.append(script);
    }
}

static void jsPDFDocInitialize(JSContextRef ctx, JSObjectRef object)
{
    PDFScriptEvaluator* evaluator = static_cast<PDFScriptEvaluator*>(JSObjectGetPrivate(object));
    evaluator->ref();
}

static void jsPDFDocFinalize(JSObjectRef object)
{
    PDFScriptEvaluator* evaluator = static_cast<PDFScriptEvaluator*>(JSObjectGetPrivate(object));
    evaluator->deref();
}

JSClassRef PDFScriptEvaluator::jsPDFDocClass()
{
    static const JSStaticFunction jsPDFDocStaticFunctions[] = {
        { "print", jsPDFDocPrint, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 },
    };

    static const JSClassDefinition jsPDFDocClassDefinition = {
        0,
        kJSClassAttributeNone,
        "Doc",
        0,
        0,
        jsPDFDocStaticFunctions,
        jsPDFDocInitialize, jsPDFDocFinalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static JSClassRef jsPDFDocClass = JSClassCreate(&jsPDFDocClassDefinition);
    return jsPDFDocClass;
}

JSValueRef PDFScriptEvaluator::jsPDFDocPrint(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (!JSValueIsObjectOfClass(ctx, thisObject, jsPDFDocClass()))
        return JSValueMakeUndefined(ctx);

    RefPtr evaluator = static_cast<PDFScriptEvaluator*>(JSObjectGetPrivate(thisObject));
    evaluator->print();

    return JSValueMakeUndefined(ctx);
}

void PDFScriptEvaluator::runScripts(CGPDFDocumentRef document, Client& client)
{
    ASSERT(isMainRunLoop());

    Ref evaluator = PDFScriptEvaluator::create(document, client);

    auto completionHandler = [evaluator = WTFMove(evaluator)] (Vector<RetainPtr<CFStringRef>>&& scripts) mutable {
        if (scripts.isEmpty())
            return;

        JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
        JSObjectRef jsPDFDoc = JSObjectMake(ctx, jsPDFDocClass(), evaluator.ptr());
        for (auto& script : scripts)
            JSEvaluateScript(ctx, OpaqueJSString::tryCreate(script.get()).get(), jsPDFDoc, nullptr, 1, nullptr);
        JSGlobalContextRelease(ctx);
    };

#if HAVE(INCREMENTAL_PDF_APIS)
    auto scriptUtilityQueue = WorkQueue::create("PDF script utility");
    auto& rawQueue = scriptUtilityQueue.get();
    rawQueue.dispatch([scriptUtilityQueue = WTFMove(scriptUtilityQueue), completionHandler = WTFMove(completionHandler), document = WTFMove(document)] () mutable {
        ASSERT(!isMainRunLoop());

        Vector<RetainPtr<CFStringRef>> scripts;
        getAllScriptsInPDFDocument(document, scripts);

        callOnMainRunLoop([completionHandler = WTFMove(completionHandler), scripts = WTFMove(scripts)] () mutable {
            completionHandler(WTFMove(scripts));
        });
    });
#else
    Vector<RetainPtr<CFStringRef>> scripts;
    getAllScriptsInPDFDocument(document, scripts);
    completionHandler(WTFMove(scripts));
#endif
}

void PDFScriptEvaluator::print()
{
    if (!m_client)
        return;
    m_client->print();
}

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)
