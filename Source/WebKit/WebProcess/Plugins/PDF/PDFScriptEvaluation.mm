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
#import "PDFScriptEvaluation.h"

#if ENABLE(PDF_PLUGIN)

#import <CoreGraphics/CGPDFDocument.h>
#import <JavaScriptCore/RegularExpression.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/OptionSet.h>
#import <wtf/text/ASCIILiteral.h>
#import <wtf/text/StringView.h>
#import <wtf/text/WTFString.h>

namespace WebKit::PDFScriptEvaluation {

static bool isPrintScript(const String& script)
{
    static constexpr ASCIILiteral searchExpression = "^.*.\\.print(.*)$"_s;
    static NeverDestroyed<const JSC::Yarr::RegularExpression> printRegex { StringView { searchExpression }, OptionSet<JSC::Yarr::Flags> { JSC::Yarr::Flags::IgnoreCase } };
    return printRegex->match(script) != -1;
}

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

static bool pdfDocumentContainsPrintScript(RetainPtr<CGPDFDocumentRef> pdfDocument)
{
    if (!pdfDocument)
        return false;

    CGPDFDictionaryRef pdfCatalog = CGPDFDocumentGetCatalog(pdfDocument.get());
    if (!pdfCatalog)
        return false;

    // Get the dictionary of all document-level name trees.
    CGPDFDictionaryRef namesDictionary = nullptr;
    if (!CGPDFDictionaryGetDictionary(pdfCatalog, "Names", &namesDictionary))
        return false;

    // Get the document-level "JavaScript" name tree.
    CGPDFDictionaryRef javaScriptNameTree = nullptr;
    if (!CGPDFDictionaryGetDictionary(namesDictionary, "JavaScript", &javaScriptNameTree))
        return false;

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

        auto scriptFromBytes = [](std::span<const uint8_t> bytes) {
            CFStringEncoding encoding = (bytes.size() > 1 && bytes[0] == 0xFE && bytes[1] == 0xFF) ? kCFStringEncodingUnicode : kCFStringEncodingUTF8;
            return adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, bytes.data(), bytes.size(), encoding, true));
        };

        auto scriptFromStream = [&] -> RetainPtr<CFStringRef> {
            CGPDFStreamRef stream = nullptr;
            if (!CGPDFDictionaryGetStream(javaScriptAction, "JS", &stream))
                return nullptr;
            CGPDFDataFormat format;
            RetainPtr<CFDataRef> data = adoptCF(CGPDFStreamCopyData(stream, &format));
            if (!data)
                return nullptr;
            return scriptFromBytes(span(data.get()));
        };

        auto scriptFromString = [&] -> RetainPtr<CFStringRef> {
            CGPDFStringRef string = nullptr;
            if (!CGPDFDictionaryGetString(javaScriptAction, "JS", &string))
                return nullptr;
            return scriptFromBytes(unsafeMakeSpan(CGPDFStringGetBytePtr(string), CGPDFStringGetLength(string)));
        };

        if (RetainPtr<CFStringRef> script = scriptFromStream() ?: scriptFromString(); script && isPrintScript({ script.get() }))
            return true;
    }

    return false;
}

void runScripts(CGPDFDocumentRef document, PrintingCallback&& callback)
{
    if (pdfDocumentContainsPrintScript(document))
        callback();
}

} // namespace WebKit::PDFScriptEvaluation

#endif // ENABLE(PDF_PLUGIN)
