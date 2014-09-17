/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef CGContextSPI_h
#define CGContextSPI_h

#include <CoreGraphics/CGContext.h>

#if USE(APPLE_INTERNAL_SDK)
#include <CoreGraphics/CGContextGState.h>
#include <CoreGraphics/CGContextPrivate.h>
#else
typedef enum {
    kCGCompositeCopy = 1,
    kCGCompositeSover = 2,
} CGCompositeOperation;
#endif

EXTERN_C CGAffineTransform CGContextGetBaseCTM(CGContextRef);
EXTERN_C CGColorRef CGContextGetFillColorAsColor(CGContextRef);
EXTERN_C CGCompositeOperation CGContextGetCompositeOperation(CGContextRef);
EXTERN_C CGFloat CGContextGetLineWidth(CGContextRef);
EXTERN_C void CGContextSetBaseCTM(CGContextRef, CGAffineTransform);
EXTERN_C void CGContextSetCTM(CGContextRef, CGAffineTransform);
EXTERN_C void CGContextSetCompositeOperation(CGContextRef, CGCompositeOperation);

#endif // CGContextSPI_h
