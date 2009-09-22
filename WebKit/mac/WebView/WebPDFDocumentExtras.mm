/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebPDFDocumentExtras.h"

#import "WebTypesInternal.h"
#import <JavaScriptCore/RetainPtr.h>
#import <PDFKit/PDFDocument.h>
#import <objc/objc-runtime.h>

#if defined(BUILDING_ON_TIGER) || defined(BUILDING_ON_LEOPARD)
@interface PDFDocument (Internal)
- (CGPDFDocumentRef)documentRef;
@end
#endif

static void appendValuesInPDFNameSubtreeToArray(CGPDFDictionaryRef subtree, NSMutableArray *values)
{
    CGPDFArrayRef names;
    if (CGPDFDictionaryGetArray(subtree, "Names", &names)) {
        size_t nameCount = CGPDFArrayGetCount(names) / 2;
        for (size_t i = 0; i < nameCount; ++i) {
            CGPDFObjectRef object;
            CGPDFArrayGetObject(names, 2 * i + 1, &object);
            [values addObject:(id)object];
        }
        return;
    }

    CGPDFArrayRef kids;
    if (!CGPDFDictionaryGetArray(subtree, "Kids", &kids))
        return;

    size_t kidCount = CGPDFArrayGetCount(kids);
    for (size_t i = 0; i < kidCount; ++i) {
        CGPDFDictionaryRef kid;
        if (!CGPDFArrayGetDictionary(kids, i, &kid))
            continue;
        appendValuesInPDFNameSubtreeToArray(kid, values);
    }
}

static NSArray *allValuesInPDFNameTree(CGPDFDictionaryRef tree)
{
    NSMutableArray *allValues = [[NSMutableArray alloc] init];
    appendValuesInPDFNameSubtreeToArray(tree, allValues);
    return [allValues autorelease];
}

static NSArray *web_PDFDocumentAllScripts(id self, SEL _cmd)
{
    NSMutableArray *scripts = [NSMutableArray array];
    CGPDFDocumentRef pdfDocument = [self documentRef];
    if (!pdfDocument)
        return scripts;

    CGPDFDictionaryRef pdfCatalog = CGPDFDocumentGetCatalog(pdfDocument);
    if (!pdfCatalog)
        return scripts;

    // Get the dictionary of all document-level name trees.
    CGPDFDictionaryRef namesDictionary;
    if (!CGPDFDictionaryGetDictionary(pdfCatalog, "Names", &namesDictionary))
        return scripts;

    // Get the document-level "JavaScript" name tree.
    CGPDFDictionaryRef javaScriptNameTree;
    if (!CGPDFDictionaryGetDictionary(namesDictionary, "JavaScript", &javaScriptNameTree))
        return scripts;

    // The names are aribtrary. We are only interested in the values.
    NSArray *objects = allValuesInPDFNameTree(javaScriptNameTree);
    NSUInteger objectCount = [objects count];

    for (NSUInteger i = 0; i < objectCount; ++i) {
        CGPDFDictionaryRef javaScriptAction;
        if (!CGPDFObjectGetValue(reinterpret_cast<CGPDFObjectRef>([objects objectAtIndex:i]), kCGPDFObjectTypeDictionary, &javaScriptAction))
            continue;

        // A JavaScript action must have an action type of "JavaScript".
        const char* actionType;
        if (!CGPDFDictionaryGetName(javaScriptAction, "S", &actionType) || strcmp(actionType, "JavaScript"))
            continue;

        const UInt8* bytes = 0;
        CFIndex length;
        CGPDFStreamRef stream;
        CGPDFStringRef string;
        RetainPtr<CFDataRef> data;
        if (CGPDFDictionaryGetStream(javaScriptAction, "JS", &stream)) {
            CGPDFDataFormat format;
            data.adoptCF(CGPDFStreamCopyData(stream, &format));
            bytes = CFDataGetBytePtr(data.get());
            length = CFDataGetLength(data.get());
        } else if (CGPDFDictionaryGetString(javaScriptAction, "JS", &string)) {
            bytes = CGPDFStringGetBytePtr(string);
            length = CGPDFStringGetLength(string);
        }
        if (!bytes)
            continue;

        NSStringEncoding encoding = (length > 1 && bytes[0] == 0xFE && bytes[1] == 0xFF) ? NSUnicodeStringEncoding : NSUTF8StringEncoding;
        NSString *script = [[NSString alloc] initWithBytes:bytes length:length encoding:encoding];
        [scripts addObject:script];
        [script release];
    }

    return scripts;
}

void addWebPDFDocumentExtras(Class pdfDocumentClass)
{
#ifndef BUILDING_ON_TIGER
    class_addMethod(pdfDocumentClass, @selector(_web_allScripts), (IMP)web_PDFDocumentAllScripts, "@@:");
#else
    static struct objc_method_list methodList = { 0, 1, { @selector(_web_allScripts), (char*)"@@:", (IMP)web_PDFDocumentAllScripts } };
    class_addMethods(pdfDocumentClass, &methodList);
#endif
}
