/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
#include "NicosiaPaintingEngine.h"

#include "NicosiaPaintingEngineBasic.h"
#include "NicosiaPaintingEngineThreaded.h"
#include <wtf/Environment.h>

namespace Nicosia {

std::unique_ptr<PaintingEngine> PaintingEngine::create()
{
#if ENABLE(DEVELOPER_MODE) && PLATFORM(WPE)
    if (auto numThreadsEnv = Environment::get("WEBKIT_NICOSIA_PAINTING_THREADS")) {
        bool ok;
        auto numThreads = numThreadsEnv->toIntStrict(&ok);
        if (ok) {
            if (numThreads < 1 || numThreads > 8) {
                WTFLogAlways("The number of Nicosia painting threads is not between 1 and 8. Using the default value 4\n");
                numThreads = 4;
            }

            return std::unique_ptr<PaintingEngine>(new PaintingEngineThreaded(numThreads));
        }
    }
#endif

    return std::unique_ptr<PaintingEngine>(new PaintingEngineBasic);
}

} // namespace Nicosia
