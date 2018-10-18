/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include <objc/objc.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSColor;
OBJC_CLASS NSData;
OBJC_CLASS NSDate;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSFont;
OBJC_CLASS NSNumber;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;
OBJC_CLASS UIFont;

namespace IPC {

class Encoder;
class Decoder;

// id
void encode(Encoder&, id);
bool decode(Decoder&, RetainPtr<id>&);

// NSAttributedString
void encode(Encoder&, NSAttributedString *);
bool decode(Decoder&, RetainPtr<NSAttributedString>&);

#if USE(APPKIT)
// NSColor
void encode(Encoder&, NSColor *);
bool decode(Decoder&, RetainPtr<NSColor>&);
#endif

// NSDictionary
void encode(Encoder&, NSDictionary *);
bool decode(Decoder&, RetainPtr<NSDictionary>&);

// NSArray
void encode(Encoder&, NSArray *);
bool decode(Decoder&, RetainPtr<NSArray>&);

#if USE(APPKIT)
// NSFont
void encode(Encoder&, NSFont *);
bool decode(Decoder&, RetainPtr<NSFont>&);
#endif

#if PLATFORM(IOS_FAMILY)
void encode(Encoder&, UIFont *);
bool decode(Decoder&, RetainPtr<UIFont>&);
#endif

// NSNumber
void encode(Encoder&, NSNumber *);
bool decode(Decoder&, RetainPtr<NSNumber>&);

// NSString
void encode(Encoder&, NSString *);
bool decode(Decoder&, RetainPtr<NSString>&);

// NSDate
void encode(Encoder&, NSDate *);
bool decode(Decoder&, RetainPtr<NSDate>&);

// NSData
void encode(Encoder&, NSData *);
bool decode(Decoder&, RetainPtr<NSData>&);

// NSURL
void encode(Encoder&, NSURL *);
bool decode(Decoder&, RetainPtr<NSURL>&);

} // namespace IPC
