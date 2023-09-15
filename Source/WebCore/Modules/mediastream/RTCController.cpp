/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCController.h"

#if ENABLE(WEB_RTC)

#include "Document.h"
#include "RTCNetworkManager.h"
#include "RTCPeerConnection.h"
#include "WebRTCProvider.h"

namespace WebCore {

RTCController::~RTCController()
{
    for (RTCPeerConnection& connection : m_peerConnections)
        connection.clearController();
}

void RTCController::reset(bool shouldFilterICECandidates)
{
    m_shouldFilterICECandidates = shouldFilterICECandidates;
    for (RTCPeerConnection& connection : m_peerConnections)
        connection.clearController();
    m_peerConnections.clear();
    m_filteringDisabledOrigins.clear();
}

void RTCController::remove(RTCPeerConnection& connection)
{
    m_peerConnections.removeFirstMatching([&connection](auto item) {
        return &connection == &item.get();
    });
}

static inline bool matchDocumentOrigin(Document& document, SecurityOrigin& topOrigin, SecurityOrigin& clientOrigin)
{
    if (topOrigin.isSameOriginAs(document.securityOrigin()))
        return true;
    return topOrigin.isSameOriginAs(document.topOrigin()) && clientOrigin.isSameOriginAs(document.securityOrigin());
}

bool RTCController::shouldDisableICECandidateFiltering(Document& document)
{
    if (!m_shouldFilterICECandidates)
        return true;
    return notFound != m_filteringDisabledOrigins.findIf([&] (const auto& origin) {
        return matchDocumentOrigin(document, origin.topOrigin, origin.clientOrigin);
    });
}

void RTCController::add(RTCPeerConnection& connection)
{
    Ref document = downcast<Document>(*connection.scriptExecutionContext());
    if (RefPtr networkManager = document->rtcNetworkManager())
        networkManager->setICECandidateFiltering(!shouldDisableICECandidateFiltering(document));

    m_peerConnections.append(connection);
    if (shouldDisableICECandidateFiltering(downcast<Document>(*connection.scriptExecutionContext())))
        connection.disableICECandidateFiltering();
}

void RTCController::disableICECandidateFilteringForAllOrigins()
{
    if (!WebRTCProvider::webRTCAvailable())
        return;

    m_shouldFilterICECandidates = false;
    for (RTCPeerConnection& connection : m_peerConnections) {
        if (RefPtr document = downcast<Document>(connection.scriptExecutionContext())) {
            if (RefPtr networkManager = document->rtcNetworkManager())
                networkManager->setICECandidateFiltering(false);
        }
        connection.disableICECandidateFiltering();
    }
}

void RTCController::disableICECandidateFilteringForDocument(Document& document)
{
    if (!WebRTCProvider::webRTCAvailable())
        return;

    if (RefPtr networkManager = document.rtcNetworkManager())
        networkManager->setICECandidateFiltering(false);

    m_filteringDisabledOrigins.append(PeerConnectionOrigin { document.topOrigin(), document.securityOrigin() });
    for (RTCPeerConnection& connection : m_peerConnections) {
        if (RefPtr peerConnectionDocument = downcast<Document>(connection.scriptExecutionContext())) {
            if (matchDocumentOrigin(*peerConnectionDocument, document.topOrigin(), document.securityOrigin())) {
                if (RefPtr networkManager = peerConnectionDocument->rtcNetworkManager())
                    networkManager->setICECandidateFiltering(false);
                connection.disableICECandidateFiltering();
            }
        }
    }
}

void RTCController::enableICECandidateFiltering()
{
    if (!WebRTCProvider::webRTCAvailable())
        return;

    m_filteringDisabledOrigins.clear();
    m_shouldFilterICECandidates = true;
    for (RTCPeerConnection& connection : m_peerConnections) {
        connection.enableICECandidateFiltering();
        if (RefPtr document = downcast<Document>(connection.scriptExecutionContext())) {
            if (RefPtr networkManager = document->rtcNetworkManager())
                networkManager->setICECandidateFiltering(true);
        }
    }
}

} // namespace WebCore

#endif
