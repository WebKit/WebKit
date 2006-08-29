/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source ec must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DOMHTML.h"

#import "DOMHTMLCollection.h"
#import "DOMHTMLElement.h"
#import "DOMHTMLFormElement.h"
#import "DOMHTMLOptionsCollection.h"

namespace WebCore {
    class HTMLCollection;
    class HTMLDocument;
    class HTMLElement;
    class HTMLFormElement;
    class HTMLImageElement;
    class HTMLInputElement;
    class HTMLObjectElement;
    class HTMLOptionsCollection;
    class HTMLTableCaptionElement;
    class HTMLTableCellElement;
    class HTMLTableElement;
    class HTMLTableSectionElement;
}

@interface DOMHTMLOptionsCollection (WebCoreInternal)
+ (DOMHTMLOptionsCollection *)_optionsCollectionWith:(WebCore::HTMLOptionsCollection *)impl;
@end

@interface DOMHTMLDocument (WebCoreInternal)
+ (DOMHTMLDocument *)_HTMLDocumentWith:(WebCore::HTMLDocument *)impl;
@end

@interface DOMHTMLCollection (WebCoreInternal)
+ (DOMHTMLCollection *)_collectionWith:(WebCore::HTMLCollection *)impl;
@end

@interface DOMHTMLElement (WebCoreInternal)
+ (DOMHTMLElement *)_elementWith:(WebCore::HTMLElement *)impl;
- (WebCore::HTMLElement *)_HTMLElement;
@end

@interface DOMHTMLFormElement (WebCoreInternal)
+ (DOMHTMLFormElement *)_formElementWith:(WebCore::HTMLFormElement *)impl;
@end

@interface DOMHTMLTableCaptionElement (WebCoreInternal)
+ (DOMHTMLTableCaptionElement *)_tableCaptionElementWith:(WebCore::HTMLTableCaptionElement *)impl;
- (WebCore::HTMLTableCaptionElement *)_tableCaptionElement;
@end

@interface DOMHTMLTableSectionElement (WebCoreInternal)
+ (DOMHTMLTableSectionElement *)_tableSectionElementWith:(WebCore::HTMLTableSectionElement *)impl;
- (WebCore::HTMLTableSectionElement *)_tableSectionElement;
@end

@interface DOMHTMLTableElement (WebCoreInternal)
+ (DOMHTMLTableElement *)_tableElementWith:(WebCore::HTMLTableElement *)impl;
- (WebCore::HTMLTableElement *)_tableElement;
@end

@interface DOMHTMLTableCellElement (WebCoreInternal)
+ (DOMHTMLTableCellElement *)_tableCellElementWith:(WebCore::HTMLTableCellElement *)impl;
- (WebCore::HTMLTableCellElement *)_tableCellElement;
@end

@interface DOMHTMLImageElement (WebCoreInternal)
- (WebCore::HTMLImageElement *)_HTMLImageElement;
@end

@interface DOMHTMLObjectElement (WebCoreInternal)
- (WebCore::HTMLObjectElement *)_objectElement;
@end

@interface DOMHTMLInputElement (WebCoreInternal)
- (WebCore::HTMLInputElement *)_HTMLInputElement;
@end
