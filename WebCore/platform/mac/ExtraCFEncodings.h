/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef ExtraCFEncodings_H
#define ExtraCFEncodings_H

// Until there's a CFString constant for these encodings, this works.
// Since they are macros, they won't cause a compile failure even if the CFString constant is added.
#define kCFStringEncodingBig5_DOSVariant (kTextEncodingBig5 | (kBig5_DOSVariant << 16))
#define kCFStringEncodingEUC_CN_DOSVariant (kTextEncodingEUC_CN | (kEUC_CN_DOSVariant << 16))
#define kCFStringEncodingEUC_KR_DOSVariant (kTextEncodingEUC_KR | (kEUC_KR_DOSVariant << 16))
#define kCFStringEncodingISOLatin10 kTextEncodingISOLatin10
#define kCFStringEncodingKOI8_U kTextEncodingKOI8_U
#define kCFStringEncodingShiftJIS_DOSVariant (kTextEncodingShiftJIS | (kShiftJIS_DOSVariant << 16))

#endif // ExtraCFEncodings_H
