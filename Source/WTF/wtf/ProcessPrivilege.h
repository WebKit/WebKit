/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/OptionSet.h>

namespace WTF {

enum class ProcessPrivilege {
    CanAccessRawCookies            = 1 << 0,
    CanAccessCredentials           = 1 << 1,
    CanCommunicateWithWindowServer = 1 << 2,
};

WTF_EXPORT_PRIVATE void setProcessPrivileges(OptionSet<ProcessPrivilege>);
WTF_EXPORT_PRIVATE void addProcessPrivilege(ProcessPrivilege);
WTF_EXPORT_PRIVATE void removeProcessPrivilege(ProcessPrivilege);
WTF_EXPORT_PRIVATE bool hasProcessPrivilege(ProcessPrivilege);
WTF_EXPORT_PRIVATE OptionSet<ProcessPrivilege> allPrivileges();

} // namespace WTF

using WTF::ProcessPrivilege;
using WTF::allPrivileges;
using WTF::hasProcessPrivilege;
using WTF::setProcessPrivileges;

