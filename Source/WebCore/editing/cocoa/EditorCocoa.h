/*
 * Copyright (C) 2006, 2007, 2008, 2013, 2015 Apple Inc. All rights reserved.
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

#ifndef EditorCocoa_h
#define EditorCocoa_h

#include "Editor.h"

#if PLATFORM(IOS)

typedef NS_ENUM(NSInteger, NSUnderlineStyle) {
    NSUnderlineStyleNone                                = 0x00,
    NSUnderlineStyleSingle                              = 0x01,
    NSUnderlineStyleThick NS_ENUM_AVAILABLE_IOS(7_0)    = 0x02,
    NSUnderlineStyleDouble NS_ENUM_AVAILABLE_IOS(7_0)   = 0x09,
    
    NSUnderlinePatternSolid NS_ENUM_AVAILABLE_IOS(7_0)      = 0x0000,
    NSUnderlinePatternDot NS_ENUM_AVAILABLE_IOS(7_0)        = 0x0100,
    NSUnderlinePatternDash NS_ENUM_AVAILABLE_IOS(7_0)       = 0x0200,
    NSUnderlinePatternDashDot NS_ENUM_AVAILABLE_IOS(7_0)    = 0x0300,
    NSUnderlinePatternDashDotDot NS_ENUM_AVAILABLE_IOS(7_0) = 0x0400,
    
    NSUnderlineByWord NS_ENUM_AVAILABLE_IOS(7_0) = 0x8000
};

#endif

#endif
