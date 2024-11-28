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
#include "CairoPaintingEngine.h"

#if USE(CAIRO)
#include "CairoPaintingEngineBasic.h"
#include "CairoPaintingEngineThreaded.h"
#include <wtf/NumberOfCores.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {
namespace Cairo {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PaintingEngine);

std::unique_ptr<PaintingEngine> PaintingEngine::create()
{
#if PLATFORM(WPE) || USE(GTK4)
    unsigned numThreads = std::max(1, std::min(8, WTF::numberOfProcessorCores() / 2));
#else
    unsigned numThreads = 0;
#endif
    const char* numThreadsEnv = getenv("WEBKIT_CAIRO_PAINTING_THREADS");
    if (!numThreadsEnv)
        numThreadsEnv = getenv("WEBKIT_NICOSIA_PAINTING_THREADS");
    if (numThreadsEnv) {
        auto newValue = parseInteger<unsigned>(StringView::fromLatin1(numThreadsEnv));
        if (newValue && *newValue <= 8)
            numThreads = *newValue;
        else
            WTFLogAlways("The number of Cairo painting threads is not between 0 and 8. Using the default value %u\n", numThreads);
    }

    if (numThreads)
        return std::unique_ptr<PaintingEngine>(new PaintingEngineThreaded(numThreads));

    return std::unique_ptr<PaintingEngine>(new PaintingEngineBasic);
}

} // namespace Cairo
} // namespace WebCore

#endif // USE(CAIRO)
