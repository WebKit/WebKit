/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2013 Company 100 Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS''
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
#include "NetworkProcessMain.h"

#include "AuxiliaryProcessMain.h"
#include "NetworkProcess.h"
#include <WebCore/NetworkStorageSession.h>

namespace WebKit {

static RefPtr<NetworkProcess> globalNetworkProcess;

class NetworkProcessMainSoup final: public AuxiliaryProcessMainBase {
public:
    void platformFinalize() override
    {
        // FIXME: Is this still needed? We should probably destroy all existing sessions at this point instead.
        // Needed to destroy the SoupSession and SoupCookieJar, e.g. to avoid
        // leaking SQLite temporary journaling files.
        globalNetworkProcess->destroySession(PAL::SessionID::defaultSessionID());
    }
};

template<>
void initializeAuxiliaryProcess<NetworkProcess>(AuxiliaryProcessInitializationParameters&& parameters)
{
    static NeverDestroyed<NetworkProcess> networkProcess(WTFMove(parameters));
    globalNetworkProcess = &networkProcess.get();
}
    
int NetworkProcessMain(int argc, char** argv)
{
    return AuxiliaryProcessMain<NetworkProcess, NetworkProcessMainSoup>(argc, argv);
}

} // namespace WebKit
