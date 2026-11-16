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

// WebKit replacement for VK-GL-CTS framework/common/tcuMustpassGen.cpp. The
// upstream file pulls in jsoncpp solely to implement the RUNMODE_GEN_MUSTPASS
// mustpass-generation tool, which the WebKit dEQP test runners never use (they
// run RUNMODE_EXECUTE). Stubbing genMustpassFromSpec lets tcuApp.cpp link
// without vendoring jsoncpp. The Apple Xcode target compiles this in place of
// the upstream tcuMustpassGen.cpp.

#include "tcuMustpassGen.hpp"
#include "tcuDefs.hpp"

namespace tcu
{

void genMustpassFromSpec(TestPackageRoot &, TestContext &, const CommandLine &)
{
    throw tcu::InternalError("Mustpass generation (RUNMODE_GEN_MUSTPASS) is not "
                             "supported in the WebKit dEQP build",
                             nullptr, __FILE__, __LINE__);
}

}  // namespace tcu
