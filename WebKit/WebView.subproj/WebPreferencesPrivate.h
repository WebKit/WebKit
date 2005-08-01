/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import <WebKit/WebPreferences.h>

#import <PDFKit/PDFView.h>
// NOTE: #importing Quartz/Quartz.h causes an ambiguous signature conflict anywhere the method "count" is used.
// To avoid this, we #import just this specific header. For this to work, the project's Framework Search Paths
// has been modified to include /System/Library/Frameworks/Quartz.framework/Frameworks. Clients that include 
// this file may have to do this same trick.

@interface WebPreferences (WebPrivate)

// Preferences that might be public in a future release
- (BOOL)respectStandardStyleKeyEquivalents;
- (void)setRespectStandardStyleKeyEquivalents:(BOOL)flag;

- (BOOL)showsURLsInToolTips;
- (void)setShowsURLsInToolTips:(BOOL)flag;

- (BOOL)textAreasAreResizable;
- (void)setTextAreasAreResizable:(BOOL)flag;

- (PDFDisplayMode)PDFDisplayMode;
- (void)setPDFDisplayMode:(PDFDisplayMode)mode;

// zero means do AutoScale
- (float)PDFScaleFactor;
- (void)setPDFScaleFactor:(float)scale;

// Other private methods
- (int)_pageCacheSize;
- (int)_objectCacheSize;
- (void)_postPreferencesChangesNotification;
+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)identifier;
+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)identifier;
+ (void)_removeReferenceForIdentifier:(NSString *)identifier;
- (NSTimeInterval)_backForwardCacheExpirationInterval;
+ (CFStringEncoding)_systemCFStringEncoding;
+ (void)_setInitialDefaultTextEncodingToSystemEncoding;
+ (void)_setIBCreatorID:(NSString *)string;

@end
