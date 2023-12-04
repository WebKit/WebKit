/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaBufferDamage.h"

#include <cstdlib>
#include <cstring>
#include <mutex>

namespace Nicosia {

static std::once_flag bufDamageUnitedRegionOnce;
static bool bufDamageUnifiedRegionVar = false;
static void checkDamageBufUnifiedRegion()
{
    auto bufDamageUnifiedRegionsEnvVar = std::getenv("WEBKIT_BUF_DAMAGE_UNIFIED_REGION");
    if (!bufDamageUnifiedRegionsEnvVar)
        return;
    bufDamageUnifiedRegionVar = !strcmp(bufDamageUnifiedRegionsEnvVar, "1");
}

bool bufDamageUnifiedRegion()
{
    std::call_once(bufDamageUnitedRegionOnce, checkDamageBufUnifiedRegion);
    return bufDamageUnifiedRegionVar;
}

} // namespace Nicosia

