/*
 * WKC Inspector backend bridge.
 *
 * The browser embedder owns the actual frontend transport. This class carries
 * the Chrome DevTools Protocol JSON messages between WebCore and that transport;
 * it deliberately does not open a network port itself.
 */
#pragma once

#include "config.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "InspectorBackendClient.h"
#include "InspectorFrontendChannel.h"

namespace WebCore {
class Page;
class PageInspectorController;
}

namespace WKC {

class ChromeClientWKC;

class InspectorBackendClientWKC final : public WebCore::InspectorBackendClient {
public:
    explicit InspectorBackendClientWKC(ChromeClientWKC&);
    ~InspectorBackendClientWKC() override;

    void setPage(WebCore::Page&);
    bool connectFrontend();
    void disconnectFrontend();
    void dispatchMessageFromFrontend(const WTF::String&);
    bool isConnected() const { return m_connected; }

    void inspectedPageDestroyed() final;
    void frontendCountChanged(unsigned) final;
    Inspector::FrontendChannel* openLocalFrontend(WebCore::PageInspectorController*) final;
    void bringFrontendToFront() final;
    void highlight() final;
    void hideHighlight() final;

    void sendMessageToFrontend(const WTF::String&);

private:
    class FrontendChannelWKC final : public Inspector::FrontendChannel {
    public:
        explicit FrontendChannelWKC(InspectorBackendClientWKC& owner)
            : m_owner(owner)
        {
        }

        Inspector::FrontendChannel::ConnectionType connectionType() const final { return Inspector::FrontendChannel::ConnectionType::Remote; }
        void sendMessageToFrontend(const WTF::String& message) final { m_owner.sendMessageToFrontend(message); }

    private:
        InspectorBackendClientWKC& m_owner;
    };

    ChromeClientWKC& m_chromeClient;
    WebCore::Page* m_page { nullptr };
    FrontendChannelWKC m_frontendChannel;
    bool m_connected { false };
};

} // namespace WKC

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
