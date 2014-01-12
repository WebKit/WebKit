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

#include "config.h"
#include "UTextProviderLatin1.h"

#include "UTextProvider.h"
#include <wtf/text/StringImpl.h>

namespace WebCore {

static inline UTextProviderContext textLatin1GetCurrentContext(const UText* text)
{
    if (!text->chunkContents)
        return UTextProviderContext::NoContext;
    return text->chunkContents == text->pExtra ? UTextProviderContext::PrimaryContext : UTextProviderContext::PriorContext;
}

static void textLatin1MoveInPrimaryContext(UText* text, int64_t nativeIndex, int64_t nativeLength, UBool forward)
{
    ASSERT(text->chunkContents == text->pExtra);
    if (forward) {
        ASSERT(nativeIndex >= text->b && nativeIndex < nativeLength);
        text->chunkNativeStart = nativeIndex;
        text->chunkNativeLimit = nativeIndex + text->extraSize / sizeof(UChar);
        if (text->chunkNativeLimit > nativeLength)
            text->chunkNativeLimit = nativeLength;
    } else {
        ASSERT(nativeIndex > text->b && nativeIndex <= nativeLength);
        text->chunkNativeLimit = nativeIndex;
        text->chunkNativeStart = nativeIndex - text->extraSize / sizeof(UChar);
        if (text->chunkNativeStart < text->b)
            text->chunkNativeStart = text->b;
    }
    int64_t length = text->chunkNativeLimit - text->chunkNativeStart;
    // Ensure chunk length is well defined if computed length exceeds int32_t range.
    ASSERT(length < std::numeric_limits<int32_t>::max());
    text->chunkLength = length < std::numeric_limits<int32_t>::max() ? static_cast<int32_t>(length) : 0;
    text->nativeIndexingLimit = text->chunkLength;
    text->chunkOffset = forward ? 0 : text->chunkLength;
    StringImpl::copyChars(const_cast<UChar*>(text->chunkContents), static_cast<const LChar*>(text->p) + (text->chunkNativeStart - text->b), static_cast<unsigned>(text->chunkLength));
}

static void textLatin1SwitchToPrimaryContext(UText* text, int64_t nativeIndex, int64_t nativeLength, UBool forward)
{
    ASSERT(!text->chunkContents || text->chunkContents == text->q);
    text->chunkContents = static_cast<const UChar*>(text->pExtra);
    textLatin1MoveInPrimaryContext(text, nativeIndex, nativeLength, forward);
}

static void textLatin1MoveInPriorContext(UText* text, int64_t nativeIndex, int64_t nativeLength, UBool forward)
{
    ASSERT(text->chunkContents == text->q);
    ASSERT(forward ? nativeIndex < text->b : nativeIndex <= text->b);
    ASSERT_UNUSED(nativeLength, forward ? nativeIndex < nativeLength : nativeIndex <= nativeLength);
    ASSERT_UNUSED(forward, forward ? nativeIndex < nativeLength : nativeIndex <= nativeLength);
    text->chunkNativeStart = 0;
    text->chunkNativeLimit = text->b;
    text->chunkLength = text->b;
    text->nativeIndexingLimit = text->chunkLength;
    int64_t offset = nativeIndex - text->chunkNativeStart;
    // Ensure chunk offset is well defined if computed offset exceeds int32_t range or chunk length.
    ASSERT(offset < std::numeric_limits<int32_t>::max());
    text->chunkOffset = std::min(offset < std::numeric_limits<int32_t>::max() ? static_cast<int32_t>(offset) : 0, text->chunkLength);
}

static void textLatin1SwitchToPriorContext(UText* text, int64_t nativeIndex, int64_t nativeLength, UBool forward)
{
    ASSERT(!text->chunkContents || text->chunkContents == text->pExtra);
    text->chunkContents = static_cast<const UChar*>(text->q);
    textLatin1MoveInPriorContext(text, nativeIndex, nativeLength, forward);
}

// -- Begin Latin-1 provider functions --

static UText* uTextLatin1Clone(UText*, const UText*, UBool, UErrorCode*);
static int64_t uTextLatin1NativeLength(UText*); 
static UBool uTextLatin1Access(UText*, int64_t, UBool); 
static int32_t uTextLatin1Extract(UText*, int64_t, int64_t, UChar*, int32_t, UErrorCode*); 
static void uTextLatin1Close(UText*);

static UText* uTextLatin1Clone(UText* destination, const UText* source, UBool deep, UErrorCode* status)
{
    return uTextCloneImpl(destination, source, deep, status);
}

static int64_t uTextLatin1NativeLength(UText* text)
{
    return text->a + text->b;
}

static UBool uTextLatin1Access(UText* text, int64_t nativeIndex, UBool forward)
{
    if (!text->context)
        return FALSE;
    int64_t nativeLength = uTextLatin1NativeLength(text);
    UBool isAccessible;
    if (uTextAccessInChunkOrOutOfRange(text, nativeIndex, nativeLength, forward, isAccessible))
        return isAccessible;
    nativeIndex = uTextAccessPinIndex(nativeIndex, nativeLength);
    UTextProviderContext currentContext = textLatin1GetCurrentContext(text);
    UTextProviderContext newContext = uTextProviderContext(text, nativeIndex, forward);
    ASSERT(newContext != UTextProviderContext::NoContext);
    if (newContext == currentContext) {
        if (currentContext == UTextProviderContext::PrimaryContext)
            textLatin1MoveInPrimaryContext(text, nativeIndex, nativeLength, forward);
        else
            textLatin1MoveInPriorContext(text, nativeIndex, nativeLength, forward);
    } else if (newContext == UTextProviderContext::PrimaryContext)
        textLatin1SwitchToPrimaryContext(text, nativeIndex, nativeLength, forward);
    else {
        ASSERT(newContext == UTextProviderContext::PriorContext);
        textLatin1SwitchToPriorContext(text, nativeIndex, nativeLength, forward);
    }
    return TRUE;
}

static int32_t uTextLatin1Extract(UText*, int64_t, int64_t, UChar*, int32_t, UErrorCode* errorCode)
{
    // In the present context, this text provider is used only with ICU functions
    // that do not perform an extract operation.
    ASSERT_NOT_REACHED();
    *errorCode = U_UNSUPPORTED_ERROR;
    return 0;
}

static void uTextLatin1Close(UText* text)
{
    text->context = 0;
}

// -- End Latin-1 provider functions --

static const struct UTextFuncs textLatin1Funcs = {
    sizeof(UTextFuncs),
    0, 0, 0,
    uTextLatin1Clone,
    uTextLatin1NativeLength,
    uTextLatin1Access,
    uTextLatin1Extract,
    0, 0, 0, 0,
    uTextLatin1Close,
    0, 0, 0,
};

UText* uTextOpenLatin1(UTextWithBuffer* utWithBuffer, const LChar* string, unsigned length, const UChar* priorContext, int priorContextLength, UErrorCode* status)
{
    if (U_FAILURE(*status))
        return 0;
    if (!string || length > static_cast<unsigned>(std::numeric_limits<int32_t>::max())) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UText* text = utext_setup(&utWithBuffer->text, sizeof(utWithBuffer->buffer), status);
    if (U_FAILURE(*status)) {
        ASSERT(!text);
        return 0;
    }
    uTextInitialize(text, &textLatin1Funcs, string, length, priorContext, priorContextLength);
    return text;
}

} // namespace WebCore
