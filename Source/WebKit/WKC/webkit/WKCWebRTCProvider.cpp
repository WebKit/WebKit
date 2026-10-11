/*
 * WKCWebRTCProvider.cpp
 *
 * Wii U-specific WebRTC provider selection for WKC builds.
 * Upstream keeps the concrete provider factory in per-platform files; this
 * factory intentionally lives in WKC so upstream syncs do not overwrite it.
 */

#include "config.h"

#if USE(LIBWEBRTC) && PLATFORM(WKC)

#include "WebRTCProvider.h"
#include "LibWebRTCProvider.h"

#include <webrtc/api/video_codecs/builtin_video_decoder_factory.h>
#include <webrtc/api/video_codecs/builtin_video_encoder_factory.h>

namespace WebCore {

class LibWebRTCProviderWKC final : public LibWebRTCProvider {
public:
    static UniqueRef<LibWebRTCProviderWKC> create()
    {
        return makeUniqueRef<LibWebRTCProviderWKC>();
    }

protected:
    std::unique_ptr<webrtc::VideoDecoderFactory> createDecoderFactory() final
    {
        return webrtc::CreateBuiltinVideoDecoderFactory();
    }

    std::unique_ptr<webrtc::VideoEncoderFactory> createEncoderFactory() final
    {
        return webrtc::CreateBuiltinVideoEncoderFactory();
    }
};

UniqueRef<LibWebRTCProvider> LibWebRTCProvider::create()
{
    return makeUniqueRef<LibWebRTCProvider>();
}

UniqueRef<WebRTCProvider> WebRTCProvider::create()
{
    return LibWebRTCProviderWKC::create();
}

bool WebRTCProvider::webRTCAvailable()
{
    return true;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC) && PLATFORM(WKC)
