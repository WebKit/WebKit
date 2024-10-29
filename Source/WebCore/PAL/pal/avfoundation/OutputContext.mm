/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "OutputContext.h"

#if USE(AVFOUNDATION)

#include "OutputDevice.h"
#include <mutex>
#include <pal/spi/cocoa/AVFoundationSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#include <pal/cocoa/AVFoundationSoftLink.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace PAL {

OutputContext::OutputContext(RetainPtr<AVOutputContext>&& context)
    : m_context(WTFMove(context))
{
}

std::optional<OutputContext>& OutputContext::sharedAudioPresentationOutputContext()
{
    static NeverDestroyed<std::optional<OutputContext>> sharedAudioPresentationOutputContext = [] () -> std::optional<OutputContext> {
        if (![PAL::getAVOutputContextClass() respondsToSelector:@selector(sharedAudioPresentationOutputContext)])
            return std::nullopt;

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        AVOutputContext* context = [getAVOutputContextClass() sharedSystemAudioContext];
#else
        auto context = [getAVOutputContextClass() sharedAudioPresentationOutputContext];
#endif
        if (!context)
            return std::nullopt;

        return OutputContext(retainPtr(context));
    }();
    return sharedAudioPresentationOutputContext;
}

bool OutputContext::supportsMultipleOutputDevices()
{
    return [m_context respondsToSelector:@selector(supportsMultipleOutputDevices)]
        && [m_context respondsToSelector:@selector(outputDevices)]
        && [m_context supportsMultipleOutputDevices];
}

String OutputContext::deviceName()
{
    if (!supportsMultipleOutputDevices())
        return [m_context deviceName];

    StringBuilder builder;
    auto devices = outputDevices();
    auto iterator = devices.begin();

    while (iterator != devices.end()) {
        builder.append(iterator->name());

        if (++iterator != devices.end())
            builder.append(" + "_s);
    }

    return builder.toString();
}

Vector<OutputDevice> OutputContext::outputDevices() const
{
    if (![m_context respondsToSelector:@selector(outputDevices)]) {
        if (auto *outputDevice = [m_context outputDevice])
            return { retainPtr(outputDevice) };
        return { };
    }

    auto *avOutputDevices = [m_context outputDevices];
    return Vector<OutputDevice>(avOutputDevices.count, [&](size_t i) {
        return OutputDevice { retainPtr((AVOutputDevice *)avOutputDevices[i]) };
    });
}


}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif
