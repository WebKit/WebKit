#include "config.h"

#include "BitmapImageSingleFrameSkia.h"

namespace WebCore {

bool BitmapImageSingleFrameSkia::isBitmapImage() const
{
    return true;
}

bool BitmapImageSingleFrameSkia::currentFrameHasAlpha()
{
    return !m_nativeImage.bitmap().isOpaque();
}

IntSize BitmapImageSingleFrameSkia::size() const
{
    return IntSize(m_nativeImage.bitmap().width(), m_nativeImage.bitmap().height());
}

// Do nothing, as we only have the one representation of data (decoded).
void BitmapImageSingleFrameSkia::destroyDecodedData(bool destroyAll)
{

}

unsigned BitmapImageSingleFrameSkia::decodedSize() const
{
    return m_nativeImage.decodedSize();
}

// We only have a single frame.
NativeImagePtr BitmapImageSingleFrameSkia::nativeImageForCurrentFrame()
{
    return &m_nativeImage;
}

#if !ASSERT_DISABLED
bool BitmapImageSingleFrameSkia::notSolidColor()
{
    return m_nativeImage.bitmap().width() != 1 || m_nativeImage.bitmap().height() != 1;
}
#endif

} // namespace WebCore

