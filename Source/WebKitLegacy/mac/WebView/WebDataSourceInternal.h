/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "WebDataSourcePrivate.h"
#import <wtf/Forward.h>
#import <wtf/NakedPtr.h>

namespace WebCore {
class DocumentLoader;
class LegacyPreviewLoaderClient;
}

class WebDocumentLoaderMac;

@class DOMDocumentFragment;
@class DOMElement;
@class WebArchive;
@class WebResource;
@class WebView;

@interface WebDataSource (WebInternal)
- (void)_makeRepresentation;
- (BOOL)_isDocumentHTML;
- (WebView *)_webView;
- (NSURL *)_URL;
- (DOMElement *)_imageElementWithImageResource:(WebResource *)resource;
- (DOMDocumentFragment *)_documentFragmentWithImageResource:(WebResource *)resource;
- (DOMDocumentFragment *)_documentFragmentWithArchive:(WebArchive *)archive;
+ (NSMutableDictionary *)_repTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission;
- (void)_replaceSelectionWithArchive:(WebArchive *)archive selectReplacement:(BOOL)selectReplacement;
- (id)_initWithDocumentLoader:(Ref<WebDocumentLoaderMac>&&)loader;
- (void)_finishedLoading;
- (void)_receivedData:(NSData *)data;
- (void)_revertToProvisionalState;
- (void)_setMainDocumentError:(NSError *)error;
- (NakedPtr<WebCore::DocumentLoader>)_documentLoader;
#if USE(QUICK_LOOK)
@property (nonatomic, copy, setter=_setQuickLookContent:) NSDictionary *_quickLookContent;
@property (nonatomic, setter=_setQuickLookPreviewLoaderClient:) WebCore::LegacyPreviewLoaderClient* _quickLookPreviewLoaderClient;
#endif
@end
