/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef APIClient_h
#define APIClient_h

#include "APIClientTraits.h"
#include <array>

#if !ASSERT_DISABLED
#include <algorithm> // std::is_sorted
#endif

// FIXME: Transition all clients from WebKit::APIClient to API::Client.
namespace API {

template<typename ClientInterface> struct ClientTraits;

template<typename ClientInterface> class Client {
    typedef typename ClientTraits<ClientInterface>::Versions ClientVersions;
    static const int latestClientVersion = std::tuple_size<ClientVersions>::value - 1;
    typedef typename std::tuple_element<latestClientVersion, ClientVersions>::type LatestClientInterface;

    // Helper class that can return an std::array of element sizes in a tuple.
    template<typename> struct InterfaceSizes;
    template<typename... Interfaces> struct InterfaceSizes<std::tuple<Interfaces...>> {
        static std::array<size_t, sizeof...(Interfaces)> sizes()
        {
#if COMPILER(CLANG)
// Workaround for http://llvm.org/bugs/show_bug.cgi?id=18117
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
            return { { sizeof(Interfaces)... } };
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif
        }
    };

public:
    Client()
    {
#if !ASSERT_DISABLED
        auto interfaceSizes = InterfaceSizes<ClientVersions>::sizes();
        ASSERT(std::is_sorted(interfaceSizes.begin(), interfaceSizes.end()));
#endif

        initialize(nullptr);
    }

    void initialize(const ClientInterface* client)
    {
        if (client && client->version == latestClientVersion) {
            m_client = *reinterpret_cast<const LatestClientInterface*>(client);
            return;
        }

        memset(&m_client, 0, sizeof(m_client));

        if (client && client->version < latestClientVersion) {
            auto interfaceSizes = InterfaceSizes<ClientVersions>::sizes();

            memcpy(&m_client, client, interfaceSizes[client->version]);
        }
    }

    const LatestClientInterface& client() const { return m_client; }

protected:
    LatestClientInterface m_client;
};

} // namespace API

namespace WebKit {

template<typename ClientInterface, int currentVersion> class APIClient {
public:
    APIClient()
    {
        initialize(0);
    }
    
    void initialize(const ClientInterface* client)
    {
        COMPILE_ASSERT(sizeof(APIClientTraits<ClientInterface>::interfaceSizesByVersion) / sizeof(size_t) == currentVersion + 1, size_of_some_interfaces_are_unknown);

        if (client && client->version == currentVersion) {
            m_client = *client;
            return;
        }

        memset(&m_client, 0, sizeof(m_client));

        if (client && client->version < currentVersion)
            memcpy(&m_client, client, APIClientTraits<ClientInterface>::interfaceSizesByVersion[client->version]);
    }

    const ClientInterface& client() const { return m_client; }

protected:
    ClientInterface m_client;
};

} // namespace WebKit

#endif // APIClient_h
