#include "config.h"
#include "WebParkableStringProvider.h"

#if ENABLE(PARKABLE_STRINGS)

#include "NetworkProcessConnection.h"
#include "SandboxExtension.h"
#include "WebParkableStringStorageConnection.h"
#include "WebProcess.h"
#include <wtf/Vector.h>

namespace WebKit {
using namespace WebCore;

UniqueRef<WebParkableStringProvider> WebParkableStringProvider::create()
{
    return makeUniqueRef<WebParkableStringProvider>();
}

WebParkableStringProvider::WebParkableStringProvider()
{
}

WebParkableStringProvider::~WebParkableStringProvider()
{
}

WebParkableStringStorageConnection& WebParkableStringProvider::storageConnection()
{
    if (!m_connection)
        m_connection = &WebProcess::singleton().ensureNetworkProcessConnection().webParkableStringStorageConnection();
    return *m_connection;
}

void WebParkableStringProvider::networkProcessConnectionClosed()
{
    m_connection = nullptr;
}

void WebParkableStringProvider::notifyCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, NotificationCallback&& callback)
{
    storageConnection().notifyCandidatesAvailable(candidateCount, estimatedSize, WTFMove(callback));
}

void WebParkableStringProvider::handleCandidateRequest(uint32_t maxCount, uint64_t maxTotalSize, CandidateRequestCallback&& callback)
{
    storageConnection().handleCandidateRequest(maxCount, maxTotalSize, WTFMove(callback));
}

void WebParkableStringProvider::notifyStorageComplete(const String& digest, bool success)
{
    storageConnection().notifyStorageComplete(digest, success);
}

std::optional<std::pair<uint64_t, uint64_t>> WebParkableStringProvider::requestDiskLocationSync(const String& digest)
{
    return storageConnection().requestDiskLocationSync(digest);
}

std::optional<Vector<uint8_t>> WebParkableStringProvider::readCompressedDataFromDisk(const String& digest)
{
    return storageConnection().readCompressedDataFromDisk(digest);
}

} // namespace WebKit

#endif // ENABLE(PARKABLE_STRINGS)