/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "CorpseTargetType.h"

#if ENABLE(MYA) && HAVE(LLDB)

#include "CorpseLimits.h"
#include <lldb/API/LLDB.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {
namespace Corpse {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TargetType);

static String imagePath(lldb::SBModule module)
{
    char path[maxImagePathLength];
    module.GetFileSpec().GetPath(path, sizeof(path));
    return String::fromUTF8(path);
}

TargetType::TargetType(const lldb::SBType& type, const lldb::SBModule& module)
    : m_type(makeUniqueWithoutFastMallocCheck<lldb::SBType>(type))
    , m_module(makeUniqueWithoutFastMallocCheck<lldb::SBModule>(module))
    , m_name(String::fromUTF8(m_type->GetName()))
    , m_image(imagePath(module))
{
}

TargetType::~TargetType() = default;

size_t TargetType::byteSize() const
{
    return m_type->GetByteSize();
}

bool TargetType::isPolymorphic() const
{
    return m_type->IsPolymorphicClass();
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA) && HAVE(LLDB)
