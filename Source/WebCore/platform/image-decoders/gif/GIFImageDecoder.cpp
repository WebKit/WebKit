/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include <wtf/PassOwnPtr.h>

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

namespace WebCore {

GIFImageDecoder::GIFImageDecoder(ImageSource::AlphaOption alphaOption,
                                 ImageSource::GammaAndColorProfileOption gammaAndColorProfileOption)
    : ImageDecoder(alphaOption, gammaAndColorProfileOption)
    , m_alreadyScannedThisDataForFrameCount(true)
    , m_repetitionCount(cAnimationLoopOnce)
    , m_readOffset(0)
{
}

GIFImageDecoder::~GIFImageDecoder()
{
}

void GIFImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
    if (failed())
        return;

    ImageDecoder::setData(data, allDataReceived);

    // We need to rescan the frame count, as the new data may have changed it.
    m_alreadyScannedThisDataForFrameCount = false;
}

bool GIFImageDecoder::isSizeAvailable()
{
    if (!ImageDecoder::isSizeAvailable())
         decode(0, GIFSizeQuery);

    return ImageDecoder::isSizeAvailable();
}

bool GIFImageDecoder::setSize(unsigned width, unsigned height)
{
    if (ImageDecoder::isSizeAvailable() && size() == IntSize(width, height))
        return true;

    if (!ImageDecoder::setSize(width, height))
        return false;

    prepareScaleDataIfNecessary();
    return true;
}

size_t GIFImageDecoder::frameCount()
{
    if (!m_alreadyScannedThisDataForFrameCount) {
        // FIXME: Scanning all the data has O(n^2) behavior if the data were to
        // come in really slowly.  Might be interesting to try to clone our
        // existing read session to preserve state, but for now we just crawl
        // all the data.  Note that this is no worse than what ImageIO does on
        // Mac right now (it also crawls all the data again).
        GIFImageReader reader(0);
        reader.read((const unsigned char*)m_data->data(), m_data->size(), GIFFrameCountQuery, static_cast<unsigned>(-1));
        m_alreadyScannedThisDataForFrameCount = true;
        m_frameBufferCache.resize(reader.images_count);
        for (int i = 0; i < reader.images_count; ++i)
            m_frameBufferCache[i].setPremultiplyAlpha(m_premultiplyAlpha);
    }

    return m_frameBufferCache.size();
}

int GIFImageDecoder::repetitionCount() const
{
    // This value can arrive at any point in the image data stream.  Most GIFs
    // in the wild declare it near the beginning of the file, so it usually is
    // set by the time we've decoded the size, but (depending on the GIF and the
    // packets sent back by the webserver) not always.  If the reader hasn't
    // seen a loop count yet, it will return cLoopCountNotSeen, in which case we
    // should default to looping once (the initial value for
    // |m_repetitionCount|).
    //
    // There are two additional wrinkles here.  First, ImageSource::clear() may
    // destroy the reader, making the result from the reader _less_
    // authoritative on future calls if the recreated reader hasn't seen the
    // loop count.  We don't need to special-case this because in this case the
    // new reader will once again return cLoopCountNotSeen, and we won't
    // overwrite the cached correct value.
    //
    // Second, a GIF might never set a loop count at all, in which case we
    // should continue to treat it as a "loop once" animation.  We don't need
    // special code here either, because in this case we'll never change
    // |m_repetitionCount| from its default value.
    if (m_reader && (m_reader->loop_count != cLoopCountNotSeen))
        m_repetitionCount = m_reader->loop_count;
    return m_repetitionCount;
}

ImageFrame* GIFImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    ImageFrame& frame = m_frameBufferCache[index];
    if (frame.status() != ImageFrame::FrameComplete)
        decode(index + 1, GIFFullQuery);
    return &frame;
}

bool GIFImageDecoder::setFailed()
{
    m_reader.clear();
    return ImageDecoder::setFailed();
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
    const Vector<ImageFrame>::iterator end(m_frameBufferCache.begin() + clearBeforeFrame);

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
    //   * If the frame is complete, but is DisposeOverwritePrevious, we'll
    //     skip over it in future initFrameBuffer() calls.  We can clear it
    //     unless it's |end|, and keep scanning.  For any other disposal method,
    //     stop scanning, as we've found the frame initFrameBuffer() will need
    //     next.
    //   * If the frame is partial, we're decoding it, so don't clear it; if it
    //     has a disposal method other than DisposeOverwritePrevious, stop
    //     scanning, as we'll only need this frame when decoding the next one.
    Vector<ImageFrame>::iterator i(end);
    for (; (i != m_frameBufferCache.begin()) && ((i->status() == ImageFrame::FrameEmpty) || (i->disposalMethod() == ImageFrame::DisposeOverwritePrevious)); --i) {
        if ((i->status() == ImageFrame::FrameComplete) && (i != end))
            i->clearPixelData();
    }

    // Now |i| holds the last frame we need to preserve; clear prior frames.
    for (Vector<ImageFrame>::iterator j(m_frameBufferCache.begin()); j != i; ++j) {
        ASSERT(j->status() != ImageFrame::FramePartial);
        if (j->status() != ImageFrame::FrameEmpty)
            j->clearPixelData();
    }
}

void GIFImageDecoder::decodingHalted(unsigned bytesLeft)
{
    m_readOffset = m_data->size() - bytesLeft;
}

bool GIFImageDecoder::haveDecodedRow(unsigned frameIndex, unsigned char* rowBuffer, unsigned char* rowEnd, unsigned rowNumber, unsigned repeatCount, bool writeTransparentPixels)
{
    const GIFFrameReader* frameReader = m_reader->frame_reader;
    // The pixel data and coordinates supplied to us are relative to the frame's
    // origin within the entire image size, i.e.
    // (frameReader->x_offset, frameReader->y_offset).  There is no guarantee
    // that (rowEnd - rowBuffer) == (size().width() - frameReader->x_offset), so
    // we must ensure we don't run off the end of either the source data or the
    // row's X-coordinates.
    int xBegin = upperBoundScaledX(frameReader->x_offset);
    int yBegin = upperBoundScaledY(frameReader->y_offset + rowNumber);
    int xEnd = lowerBoundScaledX(std::min(static_cast<int>(frameReader->x_offset + (rowEnd - rowBuffer)), size().width()) - 1, xBegin + 1) + 1;
    int yEnd = lowerBoundScaledY(std::min(static_cast<int>(frameReader->y_offset + rowNumber + repeatCount), size().height()) - 1, yBegin + 1) + 1;
    if (!rowBuffer || (xBegin < 0) || (yBegin < 0) || (xEnd <= xBegin) || (yEnd <= yBegin))
        return true;

    // Get the colormap.
    const unsigned char* colorMap;
    unsigned colorMapSize;
    if (frameReader->is_local_colormap_defined) {
        colorMap = frameReader->local_colormap;
        colorMapSize = (unsigned)frameReader->local_colormap_size;
    } else {
        colorMap = m_reader->global_colormap;
        colorMapSize = m_reader->global_colormap_size;
    }
    if (!colorMap)
        return true;

    // Initialize the frame if necessary.
    ImageFrame& buffer = m_frameBufferCache[frameIndex];
    if ((buffer.status() == ImageFrame::FrameEmpty) && !initFrameBuffer(frameIndex))
        return false;

    // Write one row's worth of data into the frame.  
    for (int x = xBegin; x < xEnd; ++x) {
        const unsigned char sourceValue = *(rowBuffer + (m_scaled ? m_scaledColumns[x] : x) - frameReader->x_offset);
        if ((!frameReader->is_transparent || (sourceValue != frameReader->tpixel)) && (sourceValue < colorMapSize)) {
            const size_t colorIndex = static_cast<size_t>(sourceValue) * 3;
            buffer.setRGBA(x, yBegin, colorMap[colorIndex], colorMap[colorIndex + 1], colorMap[colorIndex + 2], 255);
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
                buffer.setRGBA(x, yBegin, 0, 0, 0, 0);
        }
    }

    // Tell the frame to copy the row data if need be.
    if (repeatCount > 1)
        buffer.copyRowNTimes(xBegin, xEnd, yBegin, yEnd);

    return true;
}

bool GIFImageDecoder::frameComplete(unsigned frameIndex, unsigned frameDuration, ImageFrame::FrameDisposalMethod disposalMethod)
{
    // Initialize the frame if necessary.  Some GIFs insert do-nothing frames,
    // in which case we never reach haveDecodedRow() before getting here.
    ImageFrame& buffer = m_frameBufferCache[frameIndex];
    if ((buffer.status() == ImageFrame::FrameEmpty) && !initFrameBuffer(frameIndex))
        return false; // initFrameBuffer() has already called setFailed().

    buffer.setStatus(ImageFrame::FrameComplete);
    buffer.setDuration(frameDuration);
    buffer.setDisposalMethod(disposalMethod);

    if (!m_currentBufferSawAlpha) {
        // The whole frame was non-transparent, so it's possible that the entire
        // resulting buffer was non-transparent, and we can setHasAlpha(false).
        if (buffer.originalFrameRect().contains(IntRect(IntPoint(), scaledSize())))
            buffer.setHasAlpha(false);
        else if (frameIndex) {
            // Tricky case.  This frame does not have alpha only if everywhere
            // outside its rect doesn't have alpha.  To know whether this is
            // true, we check the start state of the frame -- if it doesn't have
            // alpha, we're safe.
            //
            // First skip over prior DisposeOverwritePrevious frames (since they
            // don't affect the start state of this frame) the same way we do in
            // initFrameBuffer().
            const ImageFrame* prevBuffer = &m_frameBufferCache[--frameIndex];
            while (frameIndex && (prevBuffer->disposalMethod() == ImageFrame::DisposeOverwritePrevious))
                prevBuffer = &m_frameBufferCache[--frameIndex];

            // Now, if we're at a DisposeNotSpecified or DisposeKeep frame, then
            // we can say we have no alpha if that frame had no alpha.  But
            // since in initFrameBuffer() we already copied that frame's alpha
            // state into the current frame's, we need do nothing at all here.
            //
            // The only remaining case is a DisposeOverwriteBgcolor frame.  If
            // it had no alpha, and its rect is contained in the current frame's
            // rect, we know the current frame has no alpha.
            if ((prevBuffer->disposalMethod() == ImageFrame::DisposeOverwriteBgcolor) && !prevBuffer->hasAlpha() && buffer.originalFrameRect().contains(prevBuffer->originalFrameRect()))
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

    m_reader.clear();
}

void GIFImageDecoder::decode(unsigned haltAtFrame, GIFQuery query)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT("GIFImageDecoder::decode", this, 0);
#endif
    if (failed())
        return;

    if (!m_reader)
        m_reader = adoptPtr(new GIFImageReader(this));

    // If we couldn't decode the image but we've received all the data, decoding
    // has failed.
    if (!m_reader->read((const unsigned char*)m_data->data() + m_readOffset, m_data->size() - m_readOffset, query, haltAtFrame) && isAllDataReceived())
        setFailed();
}

bool GIFImageDecoder::initFrameBuffer(unsigned frameIndex)
{
    // Initialize the frame rect in our buffer.
    const GIFFrameReader* frameReader = m_reader->frame_reader;
    IntRect frameRect(frameReader->x_offset, frameReader->y_offset, frameReader->width, frameReader->height);

    // Make sure the frameRect doesn't extend outside the buffer.
    if (frameRect.maxX() > size().width())
        frameRect.setWidth(size().width() - frameReader->x_offset);
    if (frameRect.maxY() > size().height())
        frameRect.setHeight(size().height() - frameReader->y_offset);

    ImageFrame* const buffer = &m_frameBufferCache[frameIndex];
    int left = upperBoundScaledX(frameRect.x());
    int right = lowerBoundScaledX(frameRect.maxX(), left);
    int top = upperBoundScaledY(frameRect.y());
    int bottom = lowerBoundScaledY(frameRect.maxY(), top);
    buffer->setOriginalFrameRect(IntRect(left, top, right - left, bottom - top));
    
    if (!frameIndex) {
        // This is the first frame, so we're not relying on any previous data.
        if (!buffer->setSize(scaledSize().width(), scaledSize().height()))
            return setFailed();
    } else {
        // The starting state for this frame depends on the previous frame's
        // disposal method.
        //
        // Frames that use the DisposeOverwritePrevious method are effectively
        // no-ops in terms of changing the starting state of a frame compared to
        // the starting state of the previous frame, so skip over them.  (If the
        // first frame specifies this method, it will get treated like
        // DisposeOverwriteBgcolor below and reset to a completely empty image.)
        const ImageFrame* prevBuffer = &m_frameBufferCache[--frameIndex];
        ImageFrame::FrameDisposalMethod prevMethod = prevBuffer->disposalMethod();
        while (frameIndex && (prevMethod == ImageFrame::DisposeOverwritePrevious)) {
            prevBuffer = &m_frameBufferCache[--frameIndex];
            prevMethod = prevBuffer->disposalMethod();
        }
        ASSERT(prevBuffer->status() == ImageFrame::FrameComplete);

        if ((prevMethod == ImageFrame::DisposeNotSpecified) || (prevMethod == ImageFrame::DisposeKeep)) {
            // Preserve the last frame as the starting state for this frame.
            if (!buffer->copyBitmapData(*prevBuffer))
                return setFailed();
        } else {
            // We want to clear the previous frame to transparent, without
            // affecting pixels in the image outside of the frame.
            const IntRect& prevRect = prevBuffer->originalFrameRect();
            const IntSize& bufferSize = scaledSize();
            if (!frameIndex || prevRect.contains(IntRect(IntPoint(), scaledSize()))) {
                // Clearing the first frame, or a frame the size of the whole
                // image, results in a completely empty image.
                if (!buffer->setSize(bufferSize.width(), bufferSize.height()))
                    return setFailed();
            } else {
              // Copy the whole previous buffer, then clear just its frame.
              if (!buffer->copyBitmapData(*prevBuffer))
                  return setFailed();
              for (int y = prevRect.y(); y < prevRect.maxY(); ++y) {
                  for (int x = prevRect.x(); x < prevRect.maxX(); ++x)
                      buffer->setRGBA(x, y, 0, 0, 0, 0);
              }
              if ((prevRect.width() > 0) && (prevRect.height() > 0))
                  buffer->setHasAlpha(true);
            }
        }
    }

    // Update our status to be partially complete.
    buffer->setStatus(ImageFrame::FramePartial);

    // Reset the alpha pixel tracker for this frame.
    m_currentBufferSawAlpha = false;
    return true;
}

} // namespace WebCore
