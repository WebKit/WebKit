/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(WRITING_TOOLS_UI)

#import <pal/spi/cocoa/WritingToolsUISPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, WritingToolsUI)

SOFT_LINK_CLASS_FOR_HEADER(PAL, WTWritingTools)
SOFT_LINK_CLASS_FOR_HEADER(PAL, WTWritingToolsViewController)

SOFT_LINK_CLASS_FOR_HEADER(PAL, _WTTextEffect)
SOFT_LINK_CLASS_FOR_HEADER(PAL, _WTTextEffectView)
SOFT_LINK_CLASS_FOR_HEADER(PAL, _WTSweepTextEffect)
SOFT_LINK_CLASS_FOR_HEADER(PAL, _WTReplaceTextEffect)
SOFT_LINK_CLASS_FOR_HEADER(PAL, _WTReplaceSourceTextEffect)
SOFT_LINK_CLASS_FOR_HEADER(PAL, _WTReplaceDestinationTextEffect)
SOFT_LINK_CLASS_FOR_HEADER(PAL, _WTTextChunk)
SOFT_LINK_CLASS_FOR_HEADER(PAL, _WTTextPreview)

#endif // ENABLE(WRITING_TOOLS_UI)

