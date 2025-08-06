/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(GSTREAMER_WEBRTC) && USE(LIBNICE)

#include "GUniquePtrNice.h"

#include <WebCore/ExceptionData.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/GStreamerIceBuffer.h>
#include <WebCore/GStreamerIceCandidateStatsPair.h>
#include <WebCore/RTCIceComponent.h>
#include <wtf/Condition.h>
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class GStreamerIceBackendNice {
public:
    GStreamerIceBackendNice();
    ~GStreamerIceBackendNice();

    void setForceRelay(bool);
    void setStunServer(const String&);
    void addTurnServer(const String&, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&&);
    void setTurnServer(const String&);
    void setTos(unsigned, unsigned);
    void addStream(unsigned, CompletionHandler<void(std::optional<unsigned>)>&&);
    void gatherCandidatesForStream(unsigned, CompletionHandler<void(bool)>&&);
    void setIsController(bool);
    void addCandidate(unsigned, const String&, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&&);
    void setLocalCredentials(unsigned, const String&, const String&, CompletionHandler<void(bool)>&&);
    void setRemoteCredentials(unsigned, const String&, const String&, CompletionHandler<void(bool)>&&);
    void sendData(unsigned, WebCore::RTCIceComponent, Vector<WebCore::GStreamerIceBuffer>&&);
    void finalizeStream(unsigned);
    void getSelectedPairStats(unsigned, CompletionHandler<void(std::optional<WebCore::GStreamerIceCandidateStatsPair>&&)>&&);

private:
    virtual IPC::Connection* connection() const = 0;
    virtual uint64_t destination() const = 0;

    void notifyNewCandidate(const NiceCandidate&);
    void notifyGatheringDone(unsigned);
    void notifyComponentStateChanged(unsigned, NiceComponentType, NiceComponentState);
    void notifyNewSelectedPair(unsigned, NiceComponentType);

    void handleIncomingData(unsigned, NiceComponentType, std::span<const uint8_t>&&);

    void fillLocalCandidateCredentials(const NiceCandidate&, GUniqueOutPtr<NiceCandidate>&);
    void fillRemoteCandidateCredentials(unsigned, const NiceCandidate&, GUniqueOutPtr<NiceCandidate>&);

    void populateCandidateStats(WebCore::GStreamerIceCandidateStats&, const NiceCandidate&, bool);

    struct CandidateAddress {
        String prefix;
        String address;
        String postfix;
    };
    Expected<CandidateAddress, WebCore::ExceptionData> getCandidateAddress(StringView candidate);
    static void addIceCandidateToAgent(NiceAgent*, unsigned, NiceCandidate&);
    void resolveAddress(String&&, CompletionHandler<void(Expected<String, WebCore::ExceptionData>&&)>&&);

    enum class ValidationErrorCode {
        ParseError,
        UnknownScheme,
        UnknownTransport,
        UnknownParameter,
        MissingUsername,
        MissingPassword
    };
    struct URLValidationError {
        ValidationErrorCode code;
        String data;
    };
    Expected<URL, URLValidationError> validateTurnServerURL(const String&);
    void addTurnServerForStream(unsigned, const URL&);

    GRefPtr<NiceAgent> m_agent;

    RefPtr<Thread> m_thread;
    GRefPtr<GMainContext> m_mainContext;
    GRefPtr<GMainLoop> m_loop;
    Lock m_lock;
    Condition m_condition;

    String m_stunServer;
    String m_turnServer;

    struct StreamItem {
        unsigned sessionId;
        unsigned streamId;
    };
    Vector<StreamItem> m_streams;
    HashSet<URL> m_turnServers;

    struct StreamCredentials {
        String ufrag;
        String pwd;
    };
    HashMap<unsigned, StreamCredentials> m_streamCredentials;
};

} // namespace WebKit

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBNICE)
