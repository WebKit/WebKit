/*
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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

#import <WebKitLegacy/WebKitLegacy_Private.h>

// Include below only header files for macOS, and that can be included from
// both C and C++ source files. Non-macOS header files should be enumerated in
// WebKitLegacy_iOS_Private.h. Files that should only be included from C++
// source files should be enumerated in the special "cpp" section in
// WebKitLegacy_macOS.private.modulemap.

#import <WebKitLegacy/WebDashboardRegion.h>
#import <WebKitLegacy/WebDynamicScrollBarsView.h>
#import <WebKitLegacy/WebIconDatabase.h>
#import <WebKitLegacy/WebJavaScriptTextInputPanel.h>
#import <WebKitLegacy/WebNSEventExtras.h>
#import <WebKitLegacy/WebNSPasteboardExtras.h>
#import <WebKitLegacy/WebNSWindowExtras.h>
#import <WebKitLegacy/WebPanelAuthenticationHandler.h>
#import <WebKitLegacy/WebStringTruncator.h>
#import <WebKitLegacy/npapi.h>
#import <WebKitLegacy/npfunctions.h>
#import <WebKitLegacy/npruntime.h>
#import <WebKitLegacy/nptypes.h>
