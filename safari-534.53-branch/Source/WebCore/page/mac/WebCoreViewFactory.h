/*
 * Copyright (C) 2003, 2005, 2009, 2010 Apple Inc. All rights reserved.
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

@class WebCoreTextMarker;
@class WebCoreTextMarkerRange;

@protocol WebCoreViewFactory

- (BOOL)objectIsTextMarker:(id)object;
- (BOOL)objectIsTextMarkerRange:(id)object;

- (WebCoreTextMarker *)textMarkerWithBytes:(const void *)bytes length:(size_t)length;
- (BOOL)getBytes:(void *)bytes fromTextMarker:(WebCoreTextMarker *)textMarker length:(size_t)length;

- (WebCoreTextMarkerRange *)textMarkerRangeWithStart:(WebCoreTextMarker *)start end:(WebCoreTextMarker *)end;
- (WebCoreTextMarker *)startOfTextMarkerRange:(WebCoreTextMarkerRange *)range;
- (WebCoreTextMarker *)endOfTextMarkerRange:(WebCoreTextMarkerRange *)range;

- (void)accessibilityHandleFocusChanged;
- (CGRect)accessibilityConvertScreenRect:(CGRect)bounds;

- (AXUIElementRef)AXUIElementForElement:(id)element;
- (void)unregisterUniqueIdForUIElement:(id)element;

@end

@interface WebCoreViewFactory : NSObject
+ (WebCoreViewFactory *)sharedFactory;
@end

@interface WebCoreViewFactory (SubclassResponsibility) <WebCoreViewFactory>
@end
