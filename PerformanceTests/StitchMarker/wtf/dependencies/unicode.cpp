/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

// Stub out all unicode things. Our benchmarks shouldn't be using them.

#include <wtf/Assertions.h>

extern "C" {
void u_charDirection_55() { CRASH(); }
void u_errorName_55() { CRASH(); }
void u_foldCase_55() { CRASH(); }
void u_memcasecmp_55() { CRASH(); }
void u_strFoldCase_55() { CRASH(); }
void u_strToLower_55() { CRASH(); }
void u_strToUpper_55() { CRASH(); }
void u_tolower_55() { CRASH(); }
void u_toupper_55() { CRASH(); }
void ubrk_close_55() { CRASH(); }
void ubrk_current_55() { CRASH(); }
void ubrk_following_55() { CRASH(); }
void ubrk_getRuleStatus_55() { CRASH(); }
void ubrk_next_55() { CRASH(); }
void ubrk_open_55() { CRASH(); }
void ubrk_setText_55() { CRASH(); }
void ubrk_setUText_55() { CRASH(); }
void uloc_setKeywordValue_55() { CRASH(); }
void utext_close_55() { CRASH(); }
void utext_setup_55() { CRASH(); }
}
