/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/DOMCore.h>

@class DOMMediaList;
@class DOMStyleSheet;

extern NSString * const DOMEventException;

@interface DOMStyleSheet : DOMObject
- (NSString *)type;
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (DOMNode *)ownerNode;
- (DOMStyleSheet *)parentStyleSheet;
- (NSString *)href;
- (NSString *)title;
- (DOMMediaList *)media;
@end

@interface DOMStyleSheetList : DOMObject
- (unsigned long)length;
- (DOMStyleSheet *)item:(unsigned long)index;
@end

@interface DOMMediaList : DOMObject
- (NSString *)mediaText;
- (void)setMediaText:(NSString *)mediaText;
- (unsigned long)length;
- (NSString *)item:(unsigned long)index;
- (void)deleteMedium:(NSString *)oldMedium;
- (void)appendMedium:(NSString *)newMedium;
@end

@interface DOMObject (DOMLinkStyle)
- (DOMStyleSheet *)sheet;
@end

@interface DOMDocument (DOMDocumentStyle)
- (DOMStyleSheetList *)styleSheets;
@end
