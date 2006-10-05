#include <windows.h>
#include "BrowserExtensionWin.h"
#include "FrameWin.h"

namespace WebCore {

#define notImplemented() do { \
    char buf[256] = {0}; \
    _snprintf(buf, sizeof(buf), "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); \
    OutputDebugStringA(buf); \
} while (0)


BrowserExtensionWin::BrowserExtensionWin(WebCore::FrameWin* frame) : m_frame(frame)
{

}

void BrowserExtensionWin::setTypedIconURL(KURL const&, const String&)
{
}

void BrowserExtensionWin::setIconURL(KURL const&)
{

}

int BrowserExtensionWin::getHistoryLength()
{
    return 0;
}

bool BrowserExtensionWin::canRunModal()
{
    notImplemented();
    return 0;
}

void BrowserExtensionWin::createNewWindow(struct WebCore::ResourceRequest const& request)
{
    m_frame->createNewWindow(request);
}

void BrowserExtensionWin::createNewWindow(struct WebCore::ResourceRequest const& request,
                                          struct WebCore::WindowArgs const& args,
                                          Frame*& frame)
{
    m_frame->createNewWindow(request, args, frame);
}

bool BrowserExtensionWin::canRunModalNow()
{
    notImplemented();
    return 0;
}

void BrowserExtensionWin::runModal()
{
    notImplemented();
}

void BrowserExtensionWin::goBackOrForward(int)
{
    notImplemented();
}

KURL BrowserExtensionWin::historyURL(int distance)
{
    notImplemented();
    return KURL();
}

} // namespace WebCore
