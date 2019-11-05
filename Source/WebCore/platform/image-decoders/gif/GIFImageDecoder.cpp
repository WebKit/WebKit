/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GIFImageDecoder.h"

#include "GIFImageReader.h"
#include <limits>

namespace WebCore {

GIFImageDecoder::GIFImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
{
}

GIFImageDecoder::~GIFImageDecoder() = default;

void GIFImageDecoder::setData(SharedBuffer& data, bool allDataReceived)
{
    if (failed())
        return;

    ScalableImageDecoder::setData(data, allDataReceived);
    if (m_reader)
        m_reader->setData(*m_data);
}

bool GIFImageDecoder::setSize(const IntSize& size)
{
    if (ScalableImageDecoder::encodedDataStatus() >= EncodedDataStatus::SizeAvailable && this->size() == size)
        return true;

    return ScalableImageDecoder::setSize(size);
}

size_t GIFImageDecoder::frameCount() const
{
    const_cast<GIFImageDecoder*>(this)->decode(std::numeric_limits<unsigned>::max(), GIFFrameCountQuery, isAllDataReceived());
    return m_frameBufferCache.size();
}

RepetitionCount GIFImageDecoder::repetitionCount() const
{
    // This value can arrive at any point in the image data stream.  Most GIFs
    // in the wild declare it near the beginning of the file, so it usually is
    // set by the time we've decoded the size, but (depending on the GIF and the
    // packets sent back by the webserver) not always.  If the reader hasn't
    // seen a loop count yet, it will return cLoopCountNotSeen, in which case we
    // should default to looping once (the initial value for
    // |m_repetitionCount|).
    //
    // There are some additional wrinkles here. First, ImageSource::clear()
    // may destroy the reader, making the result from the reader _less_
    // authoritative on future calls if the recreated reader hasn't seen the
    // loop count.  We don't need to special-case this because in this case the
    // new reader will once again return cLoopCountNotSeen, and we won't
    // overwrite the cached correct value.
    //
    // Second, a GIF might never set a loop count at all, in which case we
    // should continue to treat it as a "loop once" animation.  We don't need
    // special code here either, because in this case we'll never change
    // |m_repetitionCount| from its default value.
    //
    // Third, we use the same GIFImageReader for counting frames and we might
    // see the loop count and then encounter a decoding error which happens
    // later in the stream. It is also possible that no frames are in the
    // stream. In these cases we should just loop once.
    if (failed() || (m_reader && (!m_reader->imagesCount())))
        m_repetitionCount = RepetitionCountOnce;
    else if (m_reader && m_reader->loopCount() != cLoopCountNotSeen)
        m_repetitionCount = m_reader->loopCount() > 0 ? m_reader->loopCount() + 1 : m_reader->loopCount();
    return m_repetitionCount;
}

size_t GIFImageDecoder::findFirstRequiredFrameToDecode(size_t frameIndex)
{
    // The first frame doesn't depend on any other.
    if (!frameIndex)
        return 0;

    for (size_t i = frameIndex; i > 0; --i) {
        auto& frame = m_frameBufferCache[i - 1];

        // Frames with disposal method RestoreToPrevious are useless, skip them.
        if (frame.disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToPrevious)
            continue;

        // At this point the disposal method can be Unspecified, DoNotDispose or RestoreToBackground.
        // In every case, if the frame is complete we can start decoding the next one.
        if (frame.isComplete())
            return i;

        // If the disposal method of this frame is RestoreToBackground and it fills the whole area,
        // the next frame's backing store is initialized to transparent, so we start decoding with it.
        if (frame.disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToBackground) {
            // We cannot use frame.backingStore()->frameRect() here, because it has been cleared
            // when the frame was removed from the cache. We need to get the values from the
            // reader context.
            const auto* frameContext = m_reader->frameContext(i - 1);
            ASSERT(frameContext);
            IntRect frameRect(frameContext->xOffset, frameContext->yOffset, frameContext->width, frameContext->height);
            if (frameRect.contains({ { }, size() }))
                return i;
        }
    }

    return 0;
}

ScalableImageDecoderFrame* GIFImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete()) {
        for (auto i = findFirstRequiredFrameToDecode(index); i <= index; i++)
            decode(i + 1, GIFFullQuery, isAllDataReceived());
    }

    return &frame;
}

bool GIFImageDecoder::setFailed()
{
    m_reader = nullptr;
    return ScalableImageDecoder::setFailed();
}

void GIFImageDecoder::clearFrameBufferCache(size_t clearBeforeFrame)
{
    // In some cases, like if the decoder was destroyed while animating, we
    // can be asked to clear more frames than we currently have.
    if (m_frameBufferCache.isEmpty())
        return; // Nothing to do.

    // The "-1" here is tricky.  It does not mean that |clearBeforeFrame| is the
    // last frame we wish to preserve, but rather that we never want to clear
    // the very last frame in the cache: it's empty (so clearing it is
    // pointless), it's partial (so we don't want to clear it anyway), or the
    // cache could be enlarged with a future setData() call and it could be
    // needed to construct the next frame (see comments below).  Callers can
    // always use ImageSource::clear(true, ...) to completely free the memory in
    // this case.
    clearBeforeFrame = std::min(clearBeforeFrame, m_frameBufferCache.size() - 1);
    const Vector<ScalableImageDecoderFrame>::iterator end(m_frameBufferCache.begin() + clearBeforeFrame);

    // We need to preserve frames such that:
    //   * We don't clear |end|
    //   * We don't clear the frame we're currently decoding
    //   * We don't clear any frame from which a future initFrameBuffer() call
    //     will copy bitmap data
    // All other frames can be cleared.  Because of the constraints on when
    // ImageSource::clear() can be called (see ImageSource.h), we're guaranteed
    // not to have non-empty frames after the frame we're currently decoding.
    // So, scan backwards from |end| as follows:
    //   * If the frame is empty, we're still past any frames we care about.
    //   * If the frame is complete, but is DisposalMethod::RestoreToPrevious, we'll
    //     skip over it in future initFrameBuffer() calls.  We can clear it
    //     unless it's |end|, and keep scanning.  For any other disposal method,
    //     stop scanning, as we've found the frame initFrameBuffer() will need
    //     next.
    //   * If the frame is partial, we're decoding it, so don't clear it; if it
    //     has a disposal method other than DisposalMethod::RestoreToPrevious, stop
    //     scanning, as we'll only need this frame when decoding the next one.
    Vector<ScalableImageDecoderFrame>::iterator i(end);
    for (; (i != m_frameBufferCache.begin()) && (i->isInvalid() || (i->disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToPrevious)); --i) {
        if (i->isComplete() && (i != end))
            i->clear();
    }

    // Now |i| holds the last frame we need to preserve; clear prior frames.
    for (Vector<ScalableImageDecoderFrame>::iterator j(m_frameBufferCache.begin()); j != i; ++j) {
        ASSERT(!j->isPartial());
        if (!j->isInvalid())
            j->clear();
    }
}

bool GIFImageDecoder::haveDecodedRow(unsigned frameIndex, const Vector<unsigned char>& rowBuffer, size_t width, size_t rowNumber, unsigned repeatCount, bool writeTransparentPixels)
{
    const GIFFrameContext* frameContext = m_reader->frameContext();
    // The pixel data and coordinates supplied to us are relative to the frame's
    // origin within the entire image size, i.e.
    // (frameContext->xOffset, frameContext->yOffset). There is no guarantee
    // that width == (size().width() - frameContext->xOffset), so
    // we must ensure we don't run off the end of either the source data or the
    // row's X-coordinates.
    int xBegin = frameContext->xOffset;
    int yBegin = frameContext->yOffset + rowNumber;
    int xEnd = std::min(static_cast<int>(frameContext->xOffset + width), size().width());
    int yEnd = std::min(static_cast<int>(frameContext->yOffset + rowNumber + repeatCount), size().height());
    if (rowBuffer.isEmpty() || xEnd <= xBegin || yEnd <= yBegin)
        return true;

    // Get the colormap.
    const unsigned char* colorMap;
    unsigned colorMapSize;
    if (frameContext->isLocalColormapDefined) {
        colorMap = m_reader->localColormap(frameContext);
        colorMapSize = m_reader->localColormapSize(frameContext);
    } else {
        colorMap = m_reader->globalColormap();
        colorMapSize = m_reader->globalColormapSize();
    }
    if (!colorMap)
        return true;

    // Initialize the frame if necessary.
    auto& buffer = m_frameBufferCache[frameIndex];
    if ((buffer.isInvalid() && !initFrameBuffer(frameIndex)) || !buffer.hasBackingStore())
        return false;

    auto* currentAddress = buffer.backingStore()->pixelAt(xBegin, yBegin);
    // Write one row's worth of data into the frame.  
    for (int x = xBegin; x < xEnd; ++x) {
        const unsigned char sourceValue = rowBuffer[x - frameContext->xOffset];
        if ((!frameContext->isTransparent || (sourceValue != frameContext->tpixel)) && (sourceValue < colorMapSize)) {
            const size_t colorIndex = static_cast<size_t>(sourceValue) * 3;
            buffer.backingStore()->setPixel(currentAddress, colorMap[colorIndex], colorMap[colorIndex + 1], colorMap[colorIndex + 2], 255);
        } else {
            m_currentBufferSawAlpha = true;
            // We may or may not need to write transparent pixels to the buffer.
            // If we're compositing against a previous image, it's wrong, and if
            // we're writing atop a cleared, fully transparent buffer, it's
            // unnecessary; but if we're decoding an interlaced gif and
            // displaying it "Haeberli"-style, we must write these for passes
            // beyond the first, or the initial passes will "show through" the
            // later ones.
            if (writeTransparentPixels)
                buffer.backingStore()->setPixel(currentAddress, 0, 0, 0, 0);
        }
        ++currentAddress;
    }

    // Tell the frame to copy the row data if need be.
    if (repeatCount > 1)
        buffer.backingStore()->repeatFirstRow(IntRect(xBegin, yBegin, xEnd - xBegin , yEnd - yBegin));

    return true;
}

bool GIFImageDecoder::frameComplete(unsigned frameIndex, unsigned frameDuration, ScalableImageDecoderFrame::DisposalMethod disposalMethod)
{
    // Initialize the frame if necessary.  Some GIFs insert do-nothing frames,
    // in which case we never reach haveDecodedRow() before getting here.
    auto& buffer = m_frameBufferCache[frameIndex];
    if (buffer.isInvalid() && !initFrameBuffer(frameIndex))
        return false; // initFrameBuffer() has already called setFailed().

    buffer.setDecodingStatus(DecodingStatus::Complete);
    buffer.setDuration(Seconds::fromMilliseconds(frameDuration));
    buffer.setDisposalMethod(disposalMethod);

    if (!m_currentBufferSawAlpha) {
        IntRect rect = buffer.backingStore()->frameRect();
        
        // The whole frame was non-transparent, so it's possible that the entire
        // resulting buffer was non-transparent, and we can setHasAlpha(false).
        if (rect.contains(IntRect(IntPoint(), size())))
            buffer.setHasAlpha(false);
        else if (frameIndex) {
            // Tricky case.  This frame does not have alpha only if everywhere
            // outside its rect doesn't have alpha.  To know whether this is
            // true, we check the start state of the frame -- if it doesn't have
            // alpha, we're safe.
            //
            // First skip over prior DisposalMethod::RestoreToPrevious frames (since they
            // don't affect the start state of this frame) the same way we do in
            // initFrameBuffer().
            const auto* prevBuffer = &m_frameBufferCache[--frameIndex];
            while (frameIndex && (prevBuffer->disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToPrevious))
                prevBuffer = &m_frameBufferCache[--frameIndex];

            // Now, if we're at a DisposalMethod::Unspecified or DisposalMethod::DoNotDispose frame, then
            // we can say we have no alpha if that frame had no alpha.  But
            // since in initFrameBuffer() we already copied that frame's alpha
            // state into the current frame's, we need do nothing at all here.
            //
            // The only remaining case is a DisposalMethod::RestoreToBackground frame. If
            // it had no alpha, and its rect is contained in the current frame's
            // rect, we know the current frame has no alpha.
            IntRect prevRect = prevBuffer->backingStore()->frameRect();
            if ((prevBuffer->disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToBackground) && !prevBuffer->hasAlpha() && rect.contains(prevRect))
                buffer.setHasAlpha(false);
        }
    }

    return true;
}

void GIFImageDecoder::gifComplete()
{
    // Cache the repetition count, which is now as authoritative as it's ever
    // going to be.
    repetitionCount();

    m_reader = nullptr;
}

void GIFImageDecoder::decode(unsigned haltAtFrame, GIFQuery query, bool allDataReceived)
{
    if (failed())
        return;

    if (!m_reader) {
        m_reader = makeUnique<GIFImageReader>(this);
        m_reader->setData(*m_data);
    }

    if (query == GIFSizeQuery) {
        if (!m_reader->decode(GIFSizeQuery, haltAtFrame))
            setFailed();
        return;
    }

    if (!m_reader->decode(GIFFrameCountQuery, haltAtFrame)) {
        setFailed();
        return;
    }

    m_frameBufferCache.resize(m_reader->imagesCount());

    if (query == GIFFrameCountQuery)
        return;

    if (!m_reader->decode(GIFFullQuery, haltAtFrame)) {
        setFailed();
        return;
    }

    // It is also a fatal error if all data is received but we failed to decode
    // all frames completely.
    if (allDataReceived && haltAtFrame >= m_frameBufferCache.size() && m_reader)
        setFailed();
}

bool GIFImageDecoder::initFrameBuffer(unsigned frameIndex)
{
    // Initialize the frame rect in our buffer.
    const GIFFrameContext* frameContext = m_reader->frameContext();
    IntRect frameRect(frameContext->xOffset, frameContext->yOffset, frameContext->width, frameContext->height);
    auto* const buffer = &m_frameBufferCache[frameIndex];

    if (!frameIndex) {
        // This is the first frame, so we're not relying on any previous data.
        if (!buffer->initialize(size(), m_premultiplyAlpha))
            return setFailed();
    } else {
        // The starting state for this frame depends on the previous frame's
        // disposal method.
        //
        // Frames that use the DisposalMethod::RestoreToPrevious method are effectively
        // no-ops in terms of changing the starting state of a frame compared to
        // the starting state of the previous frame, so skip over them.  (If the
        // first frame specifies this method, it will get treated like
        // DisposalMethod::RestoreToBackground below and reset to a completely empty image.)
        const auto* prevBuffer = &m_frameBufferCache[--frameIndex];
        auto prevMethod = prevBuffer->disposalMethod();
        while (frameIndex && (prevMethod == ScalableImageDecoderFrame::DisposalMethod::RestoreToPrevious)) {
            prevBuffer = &m_frameBufferCache[--frameIndex];
            prevMethod = prevBuffer->disposalMethod();
        }

        ASSERT(prevBuffer->isComplete());

        if ((prevMethod == ScalableImageDecoderFrame::DisposalMethod::Unspecified) || (prevMethod == ScalableImageDecoderFrame::DisposalMethod::DoNotDispose)) {
            // Preserve the last frame as the starting state for this frame.
            if (!prevBuffer->backingStore() || !buffer->initialize(*prevBuffer->backingStore()))
                return setFailed();
        } else {
            // We want to clear the previous frame to transparent, without
            // affecting pixels in the image outside of the frame.
            IntRect prevRect = prevBuffer->backingStore()->frameRect();
            const IntSize& bufferSize = size();
            if (!frameIndex || prevRect.contains(IntRect(IntPoint(), size()))) {
                // Clearing the first frame, or a frame the size of the whole
                // image, results in a completely empty image.
                if (!buffer->initialize(bufferSize, m_premultiplyAlpha))
                    return setFailed();
            } else {
                // Copy the whole previous buffer, then clear just its frame.
                if (!prevBuffer->backingStore() || !buffer->initialize(*prevBuffer->backingStore()))
                    return setFailed();
                buffer->backingStore()->clearRect(prevRect);
                buffer->setHasAlpha(true);
            }
        }
    }

    // Make sure the frameRect doesn't extend outside the buffer.
    if (frameRect.maxX() > size().width())
        frameRect.setWidth(size().width() - frameContext->xOffset);
    if (frameRect.maxY() > size().height())
        frameRect.setHeight(size().height() - frameContext->yOffset);

    buffer->backingStore()->setFrameRect(frameRect);

    // Update our status to be partially complete.
    buffer->setDecodingStatus(DecodingStatus::Partial);

    // Reset the alpha pixel tracker for this frame.
    m_currentBufferSawAlpha = false;
    return true;
}

} // namespace WebCore
