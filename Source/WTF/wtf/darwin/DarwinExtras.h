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

#include <wtf/Platform.h>

#if OS(DARWIN)

#include <objc/runtime.h>
#include <os/log.h>
#include <wtf/Compiler.h>
#include <wtf/spi/darwin/DataVaultSPI.h>
#include <wtf/spi/darwin/SandboxSPI.h>
#include <wtf/text/UTF8CStringView.h>

// Wrappers for Darwin functions that take UTF-8 strings, so that callers can pass typed strings
// instead of unwrapping them with legacyCStringPointer(). Like the functions they wrap, they return
// raw pointers, which the caller adopts as before.

namespace WTF {

inline int sandboxCheck(pid_t pid, const char* operation, enum sandbox_filter_type type, UTF8CStringView argument)
{
    return sandbox_check(pid, operation, type, argument.utf8());
}

inline int sandboxCheckByAuditToken(audit_token_t token, const char* operation, enum sandbox_filter_type type, UTF8CStringView argument)
{
    return sandbox_check_by_audit_token(token, operation, type, argument.utf8());
}

inline int64_t sandboxExtensionConsume(UTF8CStringView token)
{
    return sandbox_extension_consume(token.utf8());
}

inline char* sandboxExtensionIssueFile(const char* extensionClass, UTF8CStringView path, uint32_t flags)
{
    return sandbox_extension_issue_file(extensionClass, path.utf8(), flags);
}

inline char* sandboxExtensionIssueGeneric(UTF8CStringView extensionClass, uint32_t flags)
{
    return sandbox_extension_issue_generic(extensionClass.utf8(), flags);
}

inline char* sandboxExtensionIssueFileToProcess(const char* extensionClass, UTF8CStringView path, uint32_t flags, audit_token_t token)
{
    return sandbox_extension_issue_file_to_process(extensionClass, path.utf8(), flags, token);
}

inline char* sandboxExtensionIssueMach(const char* extensionClass, UTF8CStringView name, uint32_t flags)
{
    return sandbox_extension_issue_mach(extensionClass, name.utf8(), flags);
}

inline char* sandboxExtensionIssueMachToProcess(const char* extensionClass, UTF8CStringView name, uint32_t flags, audit_token_t token)
{
    return sandbox_extension_issue_mach_to_process(extensionClass, name.utf8(), flags, token);
}

inline char* sandboxExtensionIssueIOKitRegistryEntryClass(const char* extensionClass, UTF8CStringView registryEntryClass, uint32_t flags)
{
    return sandbox_extension_issue_iokit_registry_entry_class(extensionClass, registryEntryClass.utf8(), flags);
}

inline char* sandboxExtensionIssueIOKitRegistryEntryClassToProcess(const char* extensionClass, UTF8CStringView registryEntryClass, uint32_t flags, audit_token_t token)
{
    return sandbox_extension_issue_iokit_registry_entry_class_to_process(extensionClass, registryEntryClass.utf8(), flags, token);
}

inline int sandboxSetParam(sandbox_params_t parameters, const char* key, UTF8CStringView value)
{
    return sandbox_set_param(parameters, key, value.utf8());
}

inline sandbox_profile_t sandboxCompileFile(UTF8CStringView path, sandbox_params_t parameters, char** error)
{
    return sandbox_compile_file(path.utf8(), parameters, error);
}

inline sandbox_profile_t sandboxCompileString(UTF8CStringView data, sandbox_params_t parameters, char** error)
{
    return sandbox_compile_string(data.utf8(), parameters, error);
}

inline int sandboxInitWithParameters(UTF8CStringView profile, uint64_t flags, const char* const parameters[], char** errorBuffer)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return sandbox_init_with_parameters(profile.utf8(), flags, parameters, errorBuffer);
ALLOW_DEPRECATED_DECLARATIONS_END
}

inline int rootlessCheckDatavaultFlag(UTF8CStringView path, const char* storageClass)
{
    return rootless_check_datavault_flag(path.utf8(), storageClass);
}

#if USE(APPLE_INTERNAL_SDK)
inline int rootlessMkdirDatavault(UTF8CStringView path, mode_t mode, const char* storageClass)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return rootless_mkdir_datavault(path.utf8(), mode, storageClass);
ALLOW_DEPRECATED_DECLARATIONS_END
}
#endif

OS_OBJECT_RETURNS_RETAINED inline os_log_t osLogCreate(UTF8CStringView subsystem, UTF8CStringView category)
{
    return os_log_create(subsystem.utf8(), category.utf8());
}

inline Class objcLookUpClass(UTF8CStringView name)
{
    return objc_lookUpClass(name.utf8());
}

inline Class objcGetClass(UTF8CStringView name)
{
    return objc_getClass(name.utf8());
}

} // namespace WTF

using WTF::objcGetClass;
using WTF::objcLookUpClass;
using WTF::osLogCreate;
using WTF::rootlessCheckDatavaultFlag;
#if USE(APPLE_INTERNAL_SDK)
using WTF::rootlessMkdirDatavault;
#endif
using WTF::sandboxCheck;
using WTF::sandboxCheckByAuditToken;
using WTF::sandboxCompileFile;
using WTF::sandboxCompileString;
using WTF::sandboxExtensionConsume;
using WTF::sandboxExtensionIssueFile;
using WTF::sandboxExtensionIssueFileToProcess;
using WTF::sandboxExtensionIssueGeneric;
using WTF::sandboxExtensionIssueIOKitRegistryEntryClass;
using WTF::sandboxExtensionIssueIOKitRegistryEntryClassToProcess;
using WTF::sandboxExtensionIssueMach;
using WTF::sandboxExtensionIssueMachToProcess;
using WTF::sandboxInitWithParameters;
using WTF::sandboxSetParam;

#endif // OS(DARWIN)
