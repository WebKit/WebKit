/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SourceCodeDumpUtils.h"

#if ENABLE(ASSEMBLER)

#include "CodeBlock.h"
#include "InlineCallFrame.h"
#include "Options.h"
#include <wtf/StringPrintStream.h>

namespace JSC {

static const char* profileName(LinkBuffer::Profile profile)
{
#define RETURN_LINKBUFFER_PROFILE_NAME(name) case LinkBuffer::Profile::name: return #name;
    switch (profile) {
        FOR_EACH_LINKBUFFER_PROFILE(RETURN_LINKBUFFER_PROFILE_NAME)
    }
    RELEASE_ASSERT_NOT_REACHED();
#undef RETURN_LINKBUFFER_PROFILE_NAME
    return "";
}

CString functionNameForJITDump(LinkBuffer::Profile profile, CodeBlock* codeBlock)
{
    StringPrintStream out;
    out.print("JSC-", profileName(profile), ": ");
    codeBlock->dumpSimpleName(out);
    return out.toCString();
}

CString sourceCodeDumpFunctionName(LinkBuffer::Profile profile, CodeBlock* codeBlock)
{
    if (Options::useJITTiersInSourceCodeDump())
        return functionNameForJITDump(profile, codeBlock);
    StringPrintStream out;
    codeBlock->dumpSimpleName(out);
    return out.toCString();
}

RefPtr<SourceCodeDumpDebugInfo::FrameInfo> resolveSourceCodeDumpFrameInfo(CodeOrigin codeOrigin, CodeBlock* rootCodeBlock, LinkBuffer::Profile profile, UncheckedKeyHashMap<CodeOrigin, Ref<SourceCodeDumpDebugInfo::FrameInfo>>& frameInfoCache, UncheckedKeyHashMap<CodeBlock*, CString>& functionNames)
{
    Vector<CodeOrigin, 4> inlineStack;
    codeOrigin.walkUpInlineStack([&](CodeOrigin origin) {
        inlineStack.append(origin);
    });

    auto ensureFunctionName = [&](CodeBlock* codeBlock) {
        auto result = functionNames.add(codeBlock, CString());
        if (result.isNewEntry)
            result.iterator->value = sourceCodeDumpFunctionName(profile, codeBlock);
    };

    RefPtr<SourceCodeDumpDebugInfo::FrameInfo> currentFrame;
    for (auto stackOrigin : inlineStack | std::views::reverse) {
        auto it = frameInfoCache.find(stackOrigin);
        if (it != frameInfoCache.end()) {
            currentFrame = it->value.ptr();
            continue;
        }

        InlineCallFrame* inlineCallFrame = stackOrigin.inlineCallFrame();
        CodeBlock* codeBlock = inlineCallFrame ? inlineCallFrame->baselineCodeBlock.get() : rootCodeBlock;
        if (!codeBlock)
            return nullptr;

        BytecodeIndex bytecodeIndex = stackOrigin.bytecodeIndex();
        if (bytecodeIndex.offset() >= codeBlock->instructionsSize())
            return nullptr;

        LineColumn lineColumn = codeBlock->lineColumnForBytecodeIndex(bytecodeIndex);
        LineColumn functionStartLineColumn = codeBlock->functionStartLineColumn();
        RefPtr provider = codeBlock->ownerExecutable()->source().provider();
        if (!provider)
            return nullptr;

        ensureFunctionName(codeBlock);

        auto frame = SourceCodeDumpDebugInfo::FrameInfo::create(codeBlock, provider.releaseNonNull(), lineColumn, functionStartLineColumn, RefPtr { currentFrame });
        frameInfoCache.add(stackOrigin, frame.copyRef());
        currentFrame = WTF::move(frame);
    }

    return currentFrame;
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
