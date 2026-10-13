/*
 * WKC Inspector backend bridge.
 */
#include "config.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "InspectorBackendClientWKC.h"

#include "ChromeClientWKC.h"
#include <WebCore/Page.h>
#include <WebCore/PageInspectorController.h>

namespace WKC {

InspectorBackendClientWKC::InspectorBackendClientWKC(ChromeClientWKC& chromeClient)
    : m_chromeClient(chromeClient)
    , m_frontendChannel(*this)
{
}

InspectorBackendClientWKC::~InspectorBackendClientWKC()
{
    disconnectFrontend();
}

void InspectorBackendClientWKC::setPage(WebCore::Page& page)
{
    m_page = &page;
}

bool InspectorBackendClientWKC::connectFrontend()
{
    if (!m_page)
        return false;

    if (m_connected)
        return true;

    m_page->inspectorController().connectFrontend(m_frontendChannel);
    m_connected = true;
    return true;
}

void InspectorBackendClientWKC::disconnectFrontend()
{
    if (m_page && m_connected)
        m_page->inspectorController().disconnectFrontend(m_frontendChannel);

    m_connected = false;
}

void InspectorBackendClientWKC::dispatchMessageFromFrontend(const WTF::String& message)
{
    if (!m_page || !m_connected || message.isEmpty())
        return;

    m_page->inspectorController().dispatchMessageFromFrontend(message);
}

void InspectorBackendClientWKC::inspectedPageDestroyed()
{
    // Page calls this while its inspector controller is being torn down. Do not
    // re-enter the controller; only invalidate this non-owning pointer.
    m_page = nullptr;
    m_connected = false;
}

void InspectorBackendClientWKC::frontendCountChanged(unsigned count)
{
    m_chromeClient.inspectorFrontendCountChanged(count);
}

Inspector::FrontendChannel* InspectorBackendClientWKC::openLocalFrontend(WebCore::PageInspectorController*)
{
    // WKC has no bundled inspector UI. The browser embedder connects its own
    // frontend explicitly through WKCWebView::connectInspectorFrontend().
    return nullptr;
}

void InspectorBackendClientWKC::bringFrontendToFront()
{
    m_chromeClient.bringInspectorFrontendToFront();
}

void InspectorBackendClientWKC::highlight()
{
    // Native highlight presentation is owned by the embedder; the generic
    // WebCore inspector overlay continues to be managed by PageInspectorController.
}

void InspectorBackendClientWKC::hideHighlight()
{
    // See highlight(). No native overlay is assumed by this portable bridge.
}

void InspectorBackendClientWKC::sendMessageToFrontend(const WTF::String& message)
{
    m_chromeClient.sendInspectorMessageToFrontend(message);
}

} // namespace WKC

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
