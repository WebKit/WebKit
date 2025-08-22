#pragma once

#include "config.h"

#if ENABLE(PARKABLE_STRINGS)

#include <WebCore/ParkableStringProvider.h>
#include <wtf/UniqueRef.h>
#include <optional>

namespace WebKit {

class WebParkableStringStorageConnection;

class WebParkableStringProvider final : public WebCore::ParkableStringProvider {
public:
    static UniqueRef<WebParkableStringProvider> create();
    virtual ~WebParkableStringProvider();
    
    void networkProcessConnectionClosed();

private:
    WebParkableStringProvider();
    
    WebParkableStringStorageConnection& storageConnection();

    void notifyCandidatesAvailable(uint32_t candidateCount, uint64_t estimatedSize, NotificationCallback&&) override;
    void handleCandidateRequest(uint32_t maxCount, uint64_t maxTotalSize, CandidateRequestCallback&&) override;
    void notifyStorageComplete(const String& digest, bool success) override;
    std::optional<std::pair<uint64_t, uint64_t>> requestDiskLocationSync(const String& digest) override;
    std::optional<Vector<uint8_t>> readCompressedDataFromDisk(const String& digest) override;
    
    RefPtr<WebParkableStringStorageConnection> m_connection;
};

} // namespace WebKit

#endif // ENABLE(PARKABLE_STRINGS)
