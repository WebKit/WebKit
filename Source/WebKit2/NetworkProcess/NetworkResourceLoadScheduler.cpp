#include "config.h"
#include "NetworkResourceLoadScheduler.h"

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

NetworkResourceLoadScheduler::NetworkResourceLoadScheduler()
{
    platformInitializeNetworkSettings();
}

void NetworkResourceLoadScheduler::scheduleLoader(PassRefPtr<NetworkResourceLoader> loader)
{
    ASSERT(RunLoop::isMain());

    LOG(NetworkScheduling, "(NetworkProcess) NetworkResourceLoadScheduler::scheduleLoader resource '%s'", loader->originalRequest().url().string().utf8().data());

    // This request might be from WebProcess we've lost our connection to.
    // If so we should just skip it.
    if (!loader->connectionToWebProcess())
        return;

    if (loader->connectionToWebProcess()->isSerialLoadingEnabled() && !m_activeLoaders.isEmpty()) {
        m_pendingSerialLoaders.append(loader);
        return;
    }
    m_activeLoaders.add(loader.get());

    loader->start();
}

void NetworkResourceLoadScheduler::removeLoader(NetworkResourceLoader* loader)
{
    ASSERT(RunLoop::isMain());

    LOG(NetworkScheduling, "(NetworkProcess) NetworkResourceLoadScheduler::removeLoader resource '%s'", loader->originalRequest().url().string().utf8().data());

    m_activeLoaders.remove(loader);

    while (!m_pendingSerialLoaders.isEmpty() && m_activeLoaders.isEmpty())
        scheduleLoader(m_pendingSerialLoaders.takeLast());
}

uint64_t NetworkResourceLoadScheduler::loadsPendingCount() const
{
    return m_pendingSerialLoaders.size();
}

uint64_t NetworkResourceLoadScheduler::loadsActiveCount() const
{
    return m_activeLoaders.size();
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
