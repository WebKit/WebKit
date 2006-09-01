/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import <WebCore/DOMHTMLElement.h>

@interface DOMHTMLAppletElement : DOMHTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)alt;
- (void)setAlt:(NSString *)alt;
- (NSString *)archive;
- (void)setArchive:(NSString *)archive;
- (NSString *)code;
- (void)setCode:(NSString *)code;
- (NSString *)codeBase;
- (void)setCodeBase:(NSString *)codeBase;
- (NSString *)height;
- (void)setHeight:(NSString *)height;
- (int)hspace;
- (void)setHspace:(int)hspace;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)object;
- (void)setObject:(NSString *)object;
- (int)vspace;
- (void)setVspace:(int)vspace;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end
