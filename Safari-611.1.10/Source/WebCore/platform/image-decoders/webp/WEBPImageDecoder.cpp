/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WEBPImageDecoder.h"
#include <wtf/UniqueArray.h>

#if USE(WEBP)

namespace WebCore {

// Convenience function to improve code readability, as WebPDemuxGetFrame is +1 based.
bool webpFrameAtIndex(WebPDemuxer* demuxer, size_t index, WebPIterator* webpFrame)
{
    return WebPDemuxGetFrame(demuxer, index + 1, webpFrame);
}

WEBPImageDecoder::WEBPImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
{
}

WEBPImageDecoder::~WEBPImageDecoder() = default;

void WEBPImageDecoder::setData(SharedBuffer& data, bool allDataReceived)
{
    if (failed())
        return;

    // We need to ensure that the header is parsed everytime new data arrives, as the number
    // of frames may change until we have the complete data. If the size has not been obtained
    // yet, the call to ScalableImageDecoder::setData() will call parseHeader() and set
    // m_headerParsed to true, so the call to parseHeader() at the end won't do anything. If the size
    // is available, then parseHeader() is only called once.
    m_headerParsed = false;
    ScalableImageDecoder::setData(data, allDataReceived);
    parseHeader();
}

RepetitionCount WEBPImageDecoder::repetitionCount() const
{
    if (failed())
        return RepetitionCountOnce;

    return m_repetitionCount ? m_repetitionCount : RepetitionCountInfinite;
}

ScalableImageDecoderFrame* WEBPImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    // The size of m_frameBufferCache may be smaller than the index requested. This can happen
    // because new data may have arrived with a bigger frameCount, but decode() hasn't been called
    // yet, which is the one that resizes the cache.
    if ((m_frameBufferCache.size() > index) && m_frameBufferCache[index].isComplete())
        return &m_frameBufferCache[index];

    decode(index, isAllDataReceived());

    return &m_frameBufferCache[index];
}

size_t WEBPImageDecoder::findFirstRequiredFrameToDecode(size_t frameIndex, WebPDemuxer* demuxer)
{
    // The first frame doesn't depend on any other.
    if (!frameIndex)
        return 0;

    // Go backwards and find the first complete frame.
    size_t firstIncompleteFrame = frameIndex;
    for (; firstIncompleteFrame; --firstIncompleteFrame) {
        if (m_frameBufferCache[firstIncompleteFrame - 1].isComplete())
            break;
    }

    // Check if there are any independent frames between firstIncompleteFrame and frameIndex.
    for (size_t firstIndependentFrame = frameIndex; firstIndependentFrame > firstIncompleteFrame ; --firstIndependentFrame) {
        WebPIterator webpFrame;
        if (!webpFrameAtIndex(demuxer, firstIndependentFrame, &webpFrame))
            continue;

        IntRect frameRect(webpFrame.x_offset, webpFrame.y_offset, webpFrame.width, webpFrame.height);
        if (!frameRect.contains({ { }, size() }))
            continue;

        // This frame covers the whole area and doesn't have alpha, so it can be rendered without
        // dependencies.
        if (!webpFrame.has_alpha)
            return firstIndependentFrame;

        // This frame covers the whole area and its disposalMethod is RestoreToBackground, which means
        // that the next frame will be rendered on top of a transparent background, and can be decoded
        // without dependencies. This can only be checked for frames prior to frameIndex.
        if (firstIndependentFrame < frameIndex && m_frameBufferCache[firstIndependentFrame].disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToBackground)
            return firstIndependentFrame + 1;
    }

    return firstIncompleteFrame;
}

void WEBPImageDecoder::decode(size_t frameIndex, bool allDataReceived)
{
    if (failed())
        return;

    // This can be executed both in the main thread (when not using async decoding) or in the decoding thread.
    // When executed in the decoding thread, a call to setData() from the main thread may change the data
    // the WebPDemuxer is using, leaving it in an inconsistent state, so we need to protect the data.
    RefPtr<SharedBuffer::DataSegment> protectedData(m_data);
    WebPData inputData = { reinterpret_cast<const uint8_t*>(protectedData->data()), protectedData->size() };
    WebPDemuxState demuxerState;
    WebPDemuxer* demuxer = WebPDemuxPartial(&inputData, &demuxerState);
    if (!demuxer) {
        setFailed();
        return;
    }

    m_frameBufferCache.resize(m_frameCount);

    // It is a fatal error if all data is received and we have decoded all frames available but the file is truncated.
    if (frameIndex >= m_frameBufferCache.size() - 1 && allDataReceived && demuxer && demuxerState != WEBP_DEMUX_DONE) {
        WebPDemuxDelete(demuxer);
        setFailed();
        return;
    }

    for (size_t i = findFirstRequiredFrameToDecode(frameIndex, demuxer); i <= frameIndex; i++)
        decodeFrame(i, demuxer);

    WebPDemuxDelete(demuxer);
}

void WEBPImageDecoder::decodeFrame(size_t frameIndex, WebPDemuxer* demuxer)
{
    if (failed())
        return;

    WebPIterator webpFrame;
    if (!webpFrameAtIndex(demuxer, frameIndex, &webpFrame))
        return;

    const uint8_t* dataBytes = reinterpret_cast<const uint8_t*>(webpFrame.fragment.bytes);
    size_t dataSize = webpFrame.fragment.size;
    bool blend = webpFrame.blend_method == WEBP_MUX_BLEND ? true : false;

    ASSERT(m_frameBufferCache.size() > frameIndex);
    auto& buffer = m_frameBufferCache[frameIndex];
    buffer.setDuration(Seconds::fromMilliseconds(webpFrame.duration));
    buffer.setDisposalMethod(webpFrame.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND ? ScalableImageDecoderFrame::DisposalMethod::RestoreToBackground : ScalableImageDecoderFrame::DisposalMethod::DoNotDispose);
    ASSERT(!buffer.isComplete());

    if (buffer.isInvalid() && !initFrameBuffer(frameIndex, &webpFrame)) {
        setFailed();
        return;
    }

    WebPDecBuffer decoderBuffer;
    WebPInitDecBuffer(&decoderBuffer);
    decoderBuffer.colorspace = MODE_RGBA;
    decoderBuffer.u.RGBA.stride = webpFrame.width * sizeof(uint32_t);
    decoderBuffer.u.RGBA.size = decoderBuffer.u.RGBA.stride * webpFrame.height;
    decoderBuffer.is_external_memory = 1;
    auto p = makeUniqueArray<uint8_t>(decoderBuffer.u.RGBA.size);
    decoderBuffer.u.RGBA.rgba = p.get();
    if (!decoderBuffer.u.RGBA.rgba) {
        setFailed();
        return;
    }

    WebPIDecoder* decoder = WebPINewDecoder(&decoderBuffer);
    if (!decoder) {
        setFailed();
        return;
    }

    switch (WebPIUpdate(decoder, dataBytes, dataSize)) {
    case VP8_STATUS_OK:
        applyPostProcessing(frameIndex, decoder, decoderBuffer, blend);
        buffer.setDecodingStatus(DecodingStatus::Complete);
        break;
    case VP8_STATUS_SUSPENDED:
        if (!isAllDataReceived()) {
            applyPostProcessing(frameIndex, decoder, decoderBuffer, blend);
            buffer.setDecodingStatus(DecodingStatus::Partial);
            break;
        }
        FALLTHROUGH;
    default:
        setFailed();
    }

    WebPIDelete(decoder);
}

bool WEBPImageDecoder::initFrameBuffer(size_t frameIndex, const WebPIterator* webpFrame)
{
    if (frameIndex >= frameCount())
        return false;

    auto& buffer = m_frameBufferCache[frameIndex];

    // Initialize the frame rect in our buffer.
    IntRect frameRect(webpFrame->x_offset, webpFrame->y_offset, webpFrame->width, webpFrame->height);

    // Make sure the frameRect doesn't extend outside the buffer.
    frameRect.intersect({ { }, size() });

    if (!frameIndex || !m_frameBufferCache[frameIndex - 1].backingStore()) {
        // This frame doesn't rely on any previous data.
        if (!buffer.initialize(size(), m_premultiplyAlpha))
            return false;
    } else {
        const auto& prevBuffer = m_frameBufferCache[frameIndex - 1];
        ASSERT(prevBuffer.isComplete());

        // Preserve the last frame as the starting state for this frame.
        if (!prevBuffer.backingStore() || !buffer.initialize(*prevBuffer.backingStore()))
            return false;

        if (prevBuffer.disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToBackground) {
            // We want to clear the previous frame to transparent, without
            // affecting pixels in the image outside of the frame.
            const IntRect& prevRect = prevBuffer.backingStore()->frameRect();
            buffer.backingStore()->clearRect(prevRect);
        }
    }

    buffer.setHasAlpha(webpFrame->has_alpha);
    buffer.backingStore()->setFrameRect(frameRect);

    return true;
}

void WEBPImageDecoder::applyPostProcessing(size_t frameIndex, WebPIDecoder* decoder, WebPDecBuffer& decoderBuffer, bool blend)
{
    auto& buffer = m_frameBufferCache[frameIndex];
    int decodedWidth = 0;
    int decodedHeight = 0;
    if (!WebPIDecGetRGB(decoder, &decodedHeight, &decodedWidth, 0, 0))
        return; // See also https://bugs.webkit.org/show_bug.cgi?id=74062
    if (decodedHeight <= 0)
        return;

    const IntRect& frameRect = buffer.backingStore()->frameRect();
    ASSERT_WITH_SECURITY_IMPLICATION(decodedWidth == frameRect.width());
    ASSERT_WITH_SECURITY_IMPLICATION(decodedHeight <= frameRect.height());
    const int left = frameRect.x();
    const int top = frameRect.y();

    for (int y = 0; y < decodedHeight; y++) {
        const int canvasY = top + y;
        for (int x = 0; x < decodedWidth; x++) {
            const int canvasX = left + x;
            auto* address = buffer.backingStore()->pixelAt(canvasX, canvasY);
            uint8_t* pixel = decoderBuffer.u.RGBA.rgba + (y * frameRect.width() + x) * sizeof(uint32_t);
            if (blend && (pixel[3] < 255))
                buffer.backingStore()->blendPixel(address, pixel[0], pixel[1], pixel[2], pixel[3]);
            else
                buffer.backingStore()->setPixel(address, pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }
}

void WEBPImageDecoder::parseHeader()
{
    if (m_headerParsed)
        return;

    m_headerParsed = true;

    const unsigned webpHeaderSize = 30; // RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE + VP8_FRAME_HEADER_SIZE
    if (m_data->size() < webpHeaderSize)
        return; // Await VP8X header so WebPDemuxPartial succeeds.

    WebPData inputData = { reinterpret_cast<const uint8_t*>(m_data->data()), m_data->size() };
    WebPDemuxState demuxerState;
    WebPDemuxer* demuxer = WebPDemuxPartial(&inputData, &demuxerState);
    if (!demuxer) {
        setFailed();
        return;
    }

    m_frameCount = WebPDemuxGetI(demuxer, WEBP_FF_FRAME_COUNT);
    if (!m_frameCount) {
        WebPDemuxDelete(demuxer);
        return; // Wait until the encoded image frame data arrives.
    }

    int width = WebPDemuxGetI(demuxer, WEBP_FF_CANVAS_WIDTH);
    int height = WebPDemuxGetI(demuxer, WEBP_FF_CANVAS_HEIGHT);
    if (!isSizeAvailable() && !setSize(IntSize(width, height))) {
        WebPDemuxDelete(demuxer);
        return;
    }

    m_formatFlags = WebPDemuxGetI(demuxer, WEBP_FF_FORMAT_FLAGS);
    if (!(m_formatFlags & ANIMATION_FLAG))
        m_repetitionCount = WebCore::RepetitionCountNone;
    else {
        // Since we have parsed at least one frame, even if partially,
        // the global animation (ANIM) properties have been read since
        // an ANIM chunk must precede the ANMF frame chunks.
        m_repetitionCount = WebPDemuxGetI(demuxer, WEBP_FF_LOOP_COUNT);
        ASSERT(m_repetitionCount == (m_repetitionCount & 0xffff)); // Loop count is always <= 16 bits.
        if (!m_repetitionCount)
            m_repetitionCount = WebCore::RepetitionCountInfinite;
    }

    WebPDemuxDelete(demuxer);
}

void WEBPImageDecoder::clearFrameBufferCache(size_t clearBeforeFrame)
{
    if (m_frameBufferCache.isEmpty())
        return;

    // We don't want to delete the last frame in the cache, as is may be needed for
    // decoding when new data arrives. See GIFImageDecoder for the full explanation.
    clearBeforeFrame = std::min(clearBeforeFrame, m_frameBufferCache.size() - 1);

    // Also from GIFImageDecoder: We need to preserve frames such that:
    //   * We don't clear |clearBeforeFrame|.
    //   * We don't clear the frame we're currently decoding.
    //   * We don't clear any frame from which a future initFrameBuffer() call will copy bitmap data.
    //
    // In WEBP every frame depends on the previous one or none. That means that frames after clearBeforeFrame
    // won't need any frame before them to render, so we can clear them all.
    for (int i = clearBeforeFrame - 1; i >= 0; i--) {
        auto& buffer = m_frameBufferCache[i];
        if (!buffer.isInvalid())
            buffer.clear();
    }
}

} // namespace WebCore

#endif
