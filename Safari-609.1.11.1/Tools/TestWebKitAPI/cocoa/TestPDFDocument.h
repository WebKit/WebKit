/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if HAVE(PDFKIT)

#import <PDFKit/PDFKit.h>
#import <wtf/RefCounted.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

namespace WebCore {
class Color;
}

namespace TestWebKitAPI {

// Annotations aren't ref counted. They remain valid as long as their containing TestPDFPage remains valid
class TestPDFAnnotation {
public:
    TestPDFAnnotation(PDFAnnotation *);

    bool isLink() const;
    CGRect bounds() const;
    NSURL *linkURL() const;
    
private:
    RetainPtr<PDFAnnotation> m_annotation;
};

class TestPDFPage : public RefCounted<TestPDFPage> {
public:
    static Ref<TestPDFPage> create(PDFPage *);

    const Vector<TestPDFAnnotation>& annotations();
    size_t characterCount() const;
    String text() const;
    CGRect rectForCharacterAtIndex(size_t) const;
    size_t characterIndexAtPoint(CGPoint) const;
    CGRect bounds() const;

    WebCore::Color colorAtPoint(int x, int y) const;
    
private:
    TestPDFPage(PDFPage *);
    RetainPtr<PDFPage> m_page;
    Optional<Vector<TestPDFAnnotation>> m_annotations;
    mutable Optional<String> m_textWithoutSurroundingWhitespace;
};

class TestPDFDocument : public RefCounted<TestPDFDocument> {
public:
    static Ref<TestPDFDocument> createFromData(NSData *);

    void dump();

    size_t pageCount() const;
    TestPDFPage* page(size_t index);

private:
    TestPDFDocument(NSData *);

    RetainPtr<PDFDocument> m_document;
    Vector<RefPtr<TestPDFPage>> m_pages;
};

} // namespace TestWebKitAPI
#endif // HAVE(PDFKIT)
