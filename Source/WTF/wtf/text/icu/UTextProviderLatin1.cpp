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
#include <wtf/text/icu/UTextProviderLatin1.h>

#include <wtf/StdLibExtras.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/icu/UTextProvider.h>

namespace WTF {

// Latin1 provider

static UText* uTextLatin1Clone(UText*, const UText*, UBool, UErrorCode*);
static int64_t uTextLatin1NativeLength(UText*);
static UBool uTextLatin1Access(UText*, int64_t, UBool);
static int32_t uTextLatin1Extract(UText*, int64_t, int64_t, UChar*, int32_t, UErrorCode*);
static int64_t uTextLatin1MapOffsetToNative(const UText*);
static int32_t uTextLatin1MapNativeIndexToUTF16(const UText*, int64_t);
static void uTextLatin1Close(UText*);

static const struct UTextFuncs uTextLatin1Funcs = {
    sizeof(UTextFuncs),
    0,
    0,
    0,
    uTextLatin1Clone,
    uTextLatin1NativeLength,
    uTextLatin1Access,
    uTextLatin1Extract,
    nullptr,
    nullptr,
    uTextLatin1MapOffsetToNative,
    uTextLatin1MapNativeIndexToUTF16,
    uTextLatin1Close,
    nullptr,
    nullptr,
    nullptr
};

static UText* uTextLatin1Clone(UText* destination, const UText* source, UBool deep, UErrorCode* status)
{
    ASSERT_UNUSED(deep, !deep);

    if (U_FAILURE(*status))
        return nullptr;

    UText* result = utext_setup(destination, sizeof(UChar) * UTextWithBufferInlineCapacity, status);
    if (U_FAILURE(*status))
        return destination;
    
    result->providerProperties = source->providerProperties;
    
    // Point at the same position, but with an empty buffer.
    result->chunkNativeStart = source->chunkNativeStart;
    result->chunkNativeLimit = source->chunkNativeStart;
    result->nativeIndexingLimit = static_cast<int32_t>(source->chunkNativeStart);
    result->chunkOffset = 0;
    result->context = source->context;
    result->a = source->a;
    result->pFuncs = &uTextLatin1Funcs;
    result->chunkContents = static_cast<UChar*>(result->pExtra);
    memset(const_cast<UChar*>(result->chunkContents), 0, sizeof(UChar) * UTextWithBufferInlineCapacity);

    return result;
}

static int64_t uTextLatin1NativeLength(UText* uText)
{
    return uText->a;
}

static UBool uTextLatin1Access(UText* uText, int64_t index, UBool forward)
{
    int64_t length = uText->a;

    if (forward) {
        if (index < uText->chunkNativeLimit && index >= uText->chunkNativeStart) {
            // Already inside the buffer. Set the new offset.
            uText->chunkOffset = static_cast<int32_t>(index - uText->chunkNativeStart);
            return true;
        }
        if (index >= length && uText->chunkNativeLimit == length) {
            // Off the end of the buffer, but we can't get it.
            uText->chunkOffset = static_cast<int32_t>(index - uText->chunkNativeStart);
            return false;
        }
    } else {
        if (index <= uText->chunkNativeLimit && index > uText->chunkNativeStart) {
            // Already inside the buffer. Set the new offset.
            uText->chunkOffset = static_cast<int32_t>(index - uText->chunkNativeStart);
            return true;
        }
        if (!index && !uText->chunkNativeStart) {
            // Already at the beginning; can't go any farther.
            uText->chunkOffset = 0;
            return false;
        }
    }
    
    if (forward) {
        uText->chunkNativeStart = index;
        uText->chunkNativeLimit = uText->chunkNativeStart + UTextWithBufferInlineCapacity;
        if (uText->chunkNativeLimit > length)
            uText->chunkNativeLimit = length;

        uText->chunkOffset = 0;
    } else {
        uText->chunkNativeLimit = index;
        if (uText->chunkNativeLimit > length)
            uText->chunkNativeLimit = length;

        uText->chunkNativeStart = uText->chunkNativeLimit - UTextWithBufferInlineCapacity;
        if (uText->chunkNativeStart < 0)
            uText->chunkNativeStart = 0;

        uText->chunkOffset = static_cast<int32_t>(index - uText->chunkNativeStart);
    }
    uText->chunkLength = static_cast<int32_t>(uText->chunkNativeLimit - uText->chunkNativeStart);

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    StringImpl::copyCharacters(const_cast<UChar*>(uText->chunkContents), unsafeMakeSpan(static_cast<const LChar*>(uText->context) + uText->chunkNativeStart, uText->chunkLength));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    uText->nativeIndexingLimit = uText->chunkLength;

    return true;
}

static int32_t uTextLatin1Extract(UText* uText, int64_t start, int64_t limit, UChar* rawDest, int32_t rawDestCapacity, UErrorCode* status)
{
    int64_t rawLength = uText->a;
    if (U_FAILURE(*status))
        return 0;

    if (rawDestCapacity < 0 || (!rawDest && rawDestCapacity > 0)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    std::span dest = unsafeMakeSpan(rawDest, rawDestCapacity);

    if (start < 0 || start > limit || (limit - start) > INT32_MAX) {
        *status = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    if (start > rawLength)
        start = rawLength;
    if (limit > rawLength)
        limit = rawLength;

    size_t length = limit - start;
    
    if (!length)
        return 0;

    if (dest.data()) {
        size_t trimmedLength = length;
        if (trimmedLength > dest.size())
            trimmedLength = dest.size();
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        StringImpl::copyCharacters(dest.data(), unsafeMakeSpan(static_cast<const LChar*>(uText->context) + start, trimmedLength));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    if (length < dest.size()) {
        if (dest.data())
            dest[length] = 0;
        if (*status == U_STRING_NOT_TERMINATED_WARNING)
            *status = U_ZERO_ERROR;
    } else if (length == dest.size())
        *status = U_STRING_NOT_TERMINATED_WARNING;
    else
        *status = U_BUFFER_OVERFLOW_ERROR;

    return static_cast<int32_t>(length);
}

static int64_t uTextLatin1MapOffsetToNative(const UText* uText)
{
    return uText->chunkNativeStart + uText->chunkOffset;
}

static int32_t uTextLatin1MapNativeIndexToUTF16(const UText* uText, int64_t nativeIndex)
{
    ASSERT_UNUSED(uText, uText->chunkNativeStart >= nativeIndex);
    ASSERT_UNUSED(uText, nativeIndex < uText->chunkNativeLimit);
    return static_cast<int32_t>(nativeIndex);
}

static void uTextLatin1Close(UText* uText)
{
    uText->context = nullptr;
}

UText* openLatin1UTextProvider(UTextWithBuffer* utWithBuffer, std::span<const LChar> string, UErrorCode* status)
{
    if (U_FAILURE(*status))
        return nullptr;
    if (!string.data() || string.size() > static_cast<unsigned>(std::numeric_limits<int32_t>::max())) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    UText* text = utext_setup(&utWithBuffer->text, sizeof(utWithBuffer->buffer), status);
    if (U_FAILURE(*status)) {
        ASSERT(!text);
        return nullptr;
    }

    text->context = string.data();
    text->a = string.size();
    text->pFuncs = &uTextLatin1Funcs;
    text->chunkContents = static_cast<UChar*>(text->pExtra);
    memset(const_cast<UChar*>(text->chunkContents), 0, sizeof(UChar) * UTextWithBufferInlineCapacity);

    return text;
}


// Latin1ContextAware provider

static UText* uTextLatin1ContextAwareClone(UText*, const UText*, UBool, UErrorCode*);
static int64_t uTextLatin1ContextAwareNativeLength(UText*);
static UBool uTextLatin1ContextAwareAccess(UText*, int64_t, UBool);
static int32_t uTextLatin1ContextAwareExtract(UText*, int64_t, int64_t, UChar*, int32_t, UErrorCode*);
static void uTextLatin1ContextAwareClose(UText*);

static const struct UTextFuncs textLatin1ContextAwareFuncs = {
    sizeof(UTextFuncs),
    0,
    0,
    0,
    uTextLatin1ContextAwareClone,
    uTextLatin1ContextAwareNativeLength,
    uTextLatin1ContextAwareAccess,
    uTextLatin1ContextAwareExtract,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    uTextLatin1ContextAwareClose,
    nullptr,
    nullptr,
    nullptr
};

static inline UTextProviderContext textLatin1ContextAwareGetCurrentContext(const UText* text)
{
    if (!text->chunkContents)
        return UTextProviderContext::NoContext;
    return text->chunkContents == text->pExtra ? UTextProviderContext::PrimaryContext : UTextProviderContext::PriorContext;
}

static void textLatin1ContextAwareMoveInPrimaryContext(UText* text, int64_t nativeIndex, int64_t nativeLength, UBool forward)
{
    ASSERT(text->chunkContents == text->pExtra);
    ASSERT(forward ? nativeIndex >= text->b : nativeIndex > text->b);
    ASSERT(nativeIndex <= nativeLength);
    if (forward) {
        text->chunkNativeStart = nativeIndex;
        text->chunkNativeLimit = nativeIndex + text->extraSize / sizeof(UChar);
        if (text->chunkNativeLimit > nativeLength)
            text->chunkNativeLimit = nativeLength;
    } else {
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
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    StringImpl::copyCharacters(const_cast<UChar*>(text->chunkContents), unsafeMakeSpan(static_cast<const LChar*>(text->p) + (text->chunkNativeStart - text->b), text->chunkLength));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

static void textLatin1ContextAwareSwitchToPrimaryContext(UText* text, int64_t nativeIndex, int64_t nativeLength, UBool forward)
{
    ASSERT(!text->chunkContents || text->chunkContents == text->q);
    text->chunkContents = static_cast<const UChar*>(text->pExtra);
    textLatin1ContextAwareMoveInPrimaryContext(text, nativeIndex, nativeLength, forward);
}

static void textLatin1ContextAwareMoveInPriorContext(UText* text, int64_t nativeIndex, int64_t nativeLength, UBool forward)
{
    ASSERT(text->chunkContents == text->q);
    ASSERT_UNUSED(forward, forward ? nativeIndex < text->b : nativeIndex <= text->b);
    ASSERT_UNUSED(nativeLength, nativeIndex <= nativeLength);
    text->chunkNativeStart = 0;
    text->chunkNativeLimit = text->b;
    text->chunkLength = text->b;
    text->nativeIndexingLimit = text->chunkLength;
    int64_t offset = nativeIndex - text->chunkNativeStart;
    // Ensure chunk offset is well defined if computed offset exceeds int32_t range or chunk length.
    ASSERT(offset < std::numeric_limits<int32_t>::max());
    text->chunkOffset = std::min(offset < std::numeric_limits<int32_t>::max() ? static_cast<int32_t>(offset) : 0, text->chunkLength);
}

static void textLatin1ContextAwareSwitchToPriorContext(UText* text, int64_t nativeIndex, int64_t nativeLength, UBool forward)
{
    ASSERT(!text->chunkContents || text->chunkContents == text->pExtra);
    text->chunkContents = static_cast<const UChar*>(text->q);
    textLatin1ContextAwareMoveInPriorContext(text, nativeIndex, nativeLength, forward);
}

static UText* uTextLatin1ContextAwareClone(UText* destination, const UText* source, UBool deep, UErrorCode* status)
{
    return uTextCloneImpl(destination, source, deep, status);
}

static int64_t uTextLatin1ContextAwareNativeLength(UText* text)
{
    return text->a + text->b;
}

static UBool uTextLatin1ContextAwareAccess(UText* text, int64_t nativeIndex, UBool forward)
{
    if (!text->context)
        return false;
    int64_t nativeLength = uTextLatin1ContextAwareNativeLength(text);
    UBool isAccessible;
    if (uTextAccessInChunkOrOutOfRange(text, nativeIndex, nativeLength, forward, isAccessible))
        return isAccessible;
    nativeIndex = uTextAccessPinIndex(nativeIndex, nativeLength);
    UTextProviderContext currentContext = textLatin1ContextAwareGetCurrentContext(text);
    UTextProviderContext newContext = uTextProviderContext(text, nativeIndex, forward);
    ASSERT(newContext != UTextProviderContext::NoContext);
    if (newContext == currentContext) {
        if (currentContext == UTextProviderContext::PrimaryContext)
            textLatin1ContextAwareMoveInPrimaryContext(text, nativeIndex, nativeLength, forward);
        else
            textLatin1ContextAwareMoveInPriorContext(text, nativeIndex, nativeLength, forward);
    } else if (newContext == UTextProviderContext::PrimaryContext)
        textLatin1ContextAwareSwitchToPrimaryContext(text, nativeIndex, nativeLength, forward);
    else {
        ASSERT(newContext == UTextProviderContext::PriorContext);
        textLatin1ContextAwareSwitchToPriorContext(text, nativeIndex, nativeLength, forward);
    }
    return true;
}

static int32_t uTextLatin1ContextAwareExtract(UText*, int64_t, int64_t, UChar*, int32_t, UErrorCode* errorCode)
{
    // In the present context, this text provider is used only with ICU functions
    // that do not perform an extract operation.
    ASSERT_NOT_REACHED();
    *errorCode = U_UNSUPPORTED_ERROR;
    return 0;
}

static void uTextLatin1ContextAwareClose(UText* text)
{
    text->context = nullptr;
}

UText* openLatin1ContextAwareUTextProvider(UTextWithBuffer* utWithBuffer, std::span<const LChar> string, std::span<const UChar> priorContext, UErrorCode* status)
{
    if (U_FAILURE(*status))
        return nullptr;
    if (!string.data() || string.size() > static_cast<unsigned>(std::numeric_limits<int32_t>::max())) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    UText* text = utext_setup(&utWithBuffer->text, sizeof(utWithBuffer->buffer), status);
    if (U_FAILURE(*status)) {
        ASSERT(!text);
        return nullptr;
    }

    initializeContextAwareUTextProvider(text, &textLatin1ContextAwareFuncs, string, priorContext);
    return text;
}

} // namespace WTF
