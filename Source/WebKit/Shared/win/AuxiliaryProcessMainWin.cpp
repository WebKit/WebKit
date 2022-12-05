/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "AuxiliaryProcessMain.h"

#include <JavaScriptCore/ExecutableAllocator.h>
#include <cstring>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

AuxiliaryProcessMainCommon::AuxiliaryProcessMainCommon() { }

bool AuxiliaryProcessMainCommon::parseCommandLine(int argc, char** argv)
{
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-clientIdentifier") && i + 1 < argc)
            m_parameters.connectionIdentifier = IPC::Connection::Identifier { reinterpret_cast<HANDLE>(parseIntegerAllowingTrailingJunk<uint64_t>(StringView::fromLatin1(argv[++i])).value_or(0)) };
        else if (!strcmp(argv[i], "-processIdentifier") && i + 1 < argc)
            m_parameters.processIdentifier = makeObjectIdentifier<WebCore::ProcessIdentifierType>(parseIntegerAllowingTrailingJunk<uint64_t>(StringView::fromLatin1(argv[++i])).value_or(0));
        else if (!strcmp(argv[i], "-configure-jsc-for-testing"))
            JSC::Config::configureForTesting();
        else if (!strcmp(argv[i], "-disable-jit"))
            JSC::ExecutableAllocator::disableJIT();
    }
    return true;
}

} // namespace WebKit
