/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PluginProcessManager.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "PluginProcessProxy.h"
#import <WebCore/RuntimeEnabledFeatures.h>

namespace WebKit {

void PluginProcessManager::updateProcessSuppressionDisabled(RefCounterEvent event)
{
    size_t disableCount = m_processSuppressionDisabledForPageCounter.value();

    // We only care about zero/non-zero edge changes; ignore cases where the count was previously non-zero, and still is.
    if (disableCount >= 2 || (disableCount == 1 && event == RefCounterEvent::Decrement))
        return;
    ASSERT((event == RefCounterEvent::Increment && disableCount == 1)
        || (event == RefCounterEvent::Decrement && !disableCount));

    bool enabled = !disableCount;
    for (auto& pluginProcess : m_pluginProcesses)
        pluginProcess->setProcessSuppressionEnabled(enabled);
}

void PluginProcessManager::setExperimentalPlugInSandboxProfilesEnabled(bool enabled)
{
    m_experimentalPlugInSandboxProfilesEnabled = enabled;
    WebCore::RuntimeEnabledFeatures::sharedFeatures().setExperimentalPlugInSandboxProfilesEnabled(enabled);
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
