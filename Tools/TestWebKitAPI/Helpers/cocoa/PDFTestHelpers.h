/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#ifdef __cplusplus
#import <wtf/RetainPtr.h>
#endif

@class NSData;
@class WKWebViewConfiguration;
@class _WKFrameHandle;

@protocol WKUIDelegate;

@interface PDFPrintUIDelegate : NSObject <WKUIDelegate>

#if PLATFORM(MAC)
- (NSSize)waitForPageSize;
#else
- (CGSize)waitForPageSize;
#endif
- (_WKFrameHandle *)lastPrintedFrame;

@end

#ifdef __cplusplus

namespace TestWebKitAPI {

#if ENABLE(UNIFIED_PDF_BY_DEFAULT)
static constexpr bool unifiedPDFForTestingEnabled = true;
#define UNIFIED_PDF_TEST(name) TEST(UnifiedPDF, name)
#else
static constexpr bool unifiedPDFForTestingEnabled = false;
#define UNIFIED_PDF_TEST(name) TEST(UnifiedPDF, DISABLED_##name)
#endif

RetainPtr<WKWebViewConfiguration> configurationForWebViewTestingUnifiedPDF(bool hudEnabled = false);

RetainPtr<NSData> testPDFData();

}

#endif // __cplusplus
