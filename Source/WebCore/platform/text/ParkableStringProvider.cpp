#include "config.h"
#include "ParkableStringProvider.h"

#if ENABLE(PARKABLE_STRINGS)

#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Default no-op provider for when WebKit layer is not available
class DefaultParkableStringProvider final : public ParkableStringProvider {
public:
    void notifyCandidatesAvailable(uint32_t, uint64_t, NotificationCallback&& callback) override
    {
        callback();
    }
    
    void handleCandidateRequest(uint32_t, uint64_t, CandidateRequestCallback&& callback) override
    {
        callback({ }, { }, { });
    }
    
    void notifyStorageComplete(const String&, bool) override
    {
    }
    
    std::optional<std::pair<uint64_t, uint64_t>> requestDiskLocationSync(const String&) override
    {
        return std::nullopt;
    }
    
    std::optional<Vector<uint8_t>> readCompressedDataFromDisk(const String&) override
    {
        return std::nullopt;
    }
};

ParkableStringProvider* ParkableStringProvider::s_sharedProvider = nullptr;

ParkableStringProvider::ParkableStringProvider()
{
}

ParkableStringProvider::~ParkableStringProvider()
{
}

ParkableStringProvider& ParkableStringProvider::singleton()
{
    ASSERT(isMainThread());
    
    if (s_sharedProvider)
        return *s_sharedProvider;
    
    // Fallback to default no-op provider when WebKit layer is not available
    static NeverDestroyed<DefaultParkableStringProvider> defaultProvider;
    return defaultProvider.get();
}

void ParkableStringProvider::setSharedProvider(ParkableStringProvider& newProvider)
{
    s_sharedProvider = &newProvider;
}

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
