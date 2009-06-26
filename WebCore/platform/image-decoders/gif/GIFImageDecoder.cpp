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

namespace WebCore {

class GIFImageDecoderPrivate {
public:
    GIFImageDecoderPrivate(GIFImageDecoder* decoder = 0)
        : m_reader(decoder)
        , m_readOffset(0)
    {
    }

    ~GIFImageDecoderPrivate()
    {
        m_reader.close();
    }

    bool decode(SharedBuffer* data, 
                GIFImageDecoder::GIFQuery query = GIFImageDecoder::GIFFullQuery,
                unsigned int haltFrame = -1)
    {
        return m_reader.read((const unsigned char*)data->data() + m_readOffset, data->size() - m_readOffset, 
                             query,
                             haltFrame);
    }

    unsigned frameCount() const { return m_reader.images_count; }
    int repetitionCount() const { return m_reader.loop_count; }

    void setReadOffset(unsigned o) { m_readOffset = o; }

    bool isTransparent() const { return m_reader.frame_reader->is_transparent; }

    void getColorMap(unsigned char*& map, unsigned& size) const
    {
        if (m_reader.frame_reader->is_local_colormap_defined) {
            map = m_reader.frame_reader->local_colormap;
            size = (unsigned)m_reader.frame_reader->local_colormap_size;
        } else {
            map = m_reader.global_colormap;
            size = m_reader.global_colormap_size;
        }
    }

    unsigned frameXOffset() const { return m_reader.frame_reader->x_offset; }
    unsigned frameYOffset() const { return m_reader.frame_reader->y_offset; }
    unsigned frameWidth() const { return m_reader.frame_reader->width; }
    unsigned frameHeight() const { return m_reader.frame_reader->height; }

    int transparentPixel() const { return m_reader.frame_reader->tpixel; }

    unsigned duration() const { return m_reader.frame_reader->delay_time; }

private:
    GIFImageReader m_reader;
    unsigned m_readOffset;
};

GIFImageDecoder::GIFImageDecoder()
    : m_frameCountValid(true)
    , m_repetitionCount(cAnimationLoopOnce)
    , m_reader(0)
{
}

GIFImageDecoder::~GIFImageDecoder()
{
    delete m_reader;
}

// Take the data and store it.
void GIFImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
    if (m_failed)
        return;

    // Cache our new data.
    ImageDecoder::setData(data, allDataReceived);

    // Our frame count is now unknown.
    m_frameCountValid = false;

    // Create the GIF reader.
    if (!m_reader && !m_failed)
        m_reader = new GIFImageDecoderPrivate(this);
}

// Whether or not the size information has been decoded yet.
bool GIFImageDecoder::isSizeAvailable()
{
    if (!ImageDecoder::isSizeAvailable() && !failed() && m_reader)
         decode(GIFSizeQuery, 0);

    return ImageDecoder::isSizeAvailable();
}

// The total number of frames for the image.  Will scan the image data for the answer
// (without necessarily decoding all of the individual frames).
int GIFImageDecoder::frameCount()
{
    // If the decoder had an earlier error, we will just return what we had decoded
    // so far.
    if (!m_frameCountValid) {
        // FIXME: Scanning all the data has O(n^2) behavior if the data were to come in really
        // slowly.  Might be interesting to try to clone our existing read session to preserve
        // state, but for now we just crawl all the data.  Note that this is no worse than what
        // ImageIO does on Mac right now (it also crawls all the data again).
        GIFImageDecoderPrivate reader;
        // This function may fail, but we want to keep any partial data it may
        // have decoded, so don't mark it is invalid. If there is an overflow
        // or some serious error, m_failed will have gotten set for us.
        reader.decode(m_data.get(), GIFFrameCountQuery);
        m_frameCountValid = true;
        m_frameBufferCache.resize(reader.frameCount());
    }

    return m_frameBufferCache.size();
}

// The number of repetitions to perform for an animation loop.
int GIFImageDecoder::repetitionCount() const
{
    // This value can arrive at any point in the image data stream.  Most GIFs
    // in the wild declare it near the beginning of the file, so it usually is
    // set by the time we've decoded the size, but (depending on the GIF and the
    // packets sent back by the webserver) not always.  Our caller is
    // responsible for waiting until image decoding has finished to ask this if
    // it needs an authoritative answer.  In the meantime, we should default to
    // "loop once".
    if (m_reader) {
        // Added wrinkle: ImageSource::clear() may destroy the reader, making
        // the result from the reader _less_ authoritative on future calls.  To
        // detect this, the reader returns cLoopCountNotSeen (-2) instead of
        // cAnimationLoopOnce (-1) when its current incarnation hasn't actually
        // seen a loop count yet; in this case we return our previously-cached
        // value.
        const int repetitionCount = m_reader->repetitionCount();
        if (repetitionCount != cLoopCountNotSeen)
            m_repetitionCount = repetitionCount;
    }
    return m_repetitionCount;
}

RGBA32Buffer* GIFImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index >= static_cast<size_t>(frameCount()))
        return 0;

    RGBA32Buffer& frame = m_frameBufferCache[index];
    if (frame.status() != RGBA32Buffer::FrameComplete && m_reader)
        decode(GIFFullQuery, index + 1); // Decode this frame.
    return &frame;
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
    const Vector<RGBA32Buffer>::iterator end(m_frameBufferCache.begin() + clearBeforeFrame);

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
    Vector<RGBA32Buffer>::iterator i(end);
    for (; (i != m_frameBufferCache.begin()) && ((i->status() == RGBA32Buffer::FrameEmpty) || (i->disposalMethod() == RGBA32Buffer::DisposeOverwritePrevious)); --i) {
        if ((i->status() == RGBA32Buffer::FrameComplete) && (i != end))
            i->clear();
    }

    // Now |i| holds the last frame we need to preserve; clear prior frames.
    for (Vector<RGBA32Buffer>::iterator j(m_frameBufferCache.begin()); j != i; ++j) {
        ASSERT(j->status() != RGBA32Buffer::FramePartial);
        if (j->status() != RGBA32Buffer::FrameEmpty)
            j->clear();
    }
}

// Feed data to the GIF reader.
void GIFImageDecoder::decode(GIFQuery query, unsigned haltAtFrame)
{
    if (m_failed)
        return;

    m_failed = !m_reader->decode(m_data.get(), query, haltAtFrame);
    
    if (m_failed) {
        delete m_reader;
        m_reader = 0;
    }
}

// Callbacks from the GIF reader.
bool GIFImageDecoder::sizeNowAvailable(unsigned width, unsigned height)
{
    return setSize(width, height);
}

void GIFImageDecoder::decodingHalted(unsigned bytesLeft)
{
    m_reader->setReadOffset(m_data->size() - bytesLeft);
}

bool GIFImageDecoder::initFrameBuffer(unsigned frameIndex)
{
    // Initialize the frame rect in our buffer.
    IntRect frameRect(m_reader->frameXOffset(), m_reader->frameYOffset(),
                      m_reader->frameWidth(), m_reader->frameHeight());

    // Make sure the frameRect doesn't extend past the bottom-right of the buffer.
    if (frameRect.right() > size().width())
        frameRect.setWidth(size().width() - m_reader->frameXOffset());
    if (frameRect.bottom() > size().height())
        frameRect.setHeight(size().height() - m_reader->frameYOffset());

    RGBA32Buffer* const buffer = &m_frameBufferCache[frameIndex];
    buffer->setRect(frameRect);
    
    if (frameIndex == 0) {
        // This is the first frame, so we're not relying on any previous data.
        if (!buffer->setSize(size().width(), size().height())) {
            m_failed = true;
            return false;
        }
    } else {
        // The starting state for this frame depends on the previous frame's
        // disposal method.
        //
        // Frames that use the DisposeOverwritePrevious method are effectively
        // no-ops in terms of changing the starting state of a frame compared to
        // the starting state of the previous frame, so skip over them.  (If the
        // first frame specifies this method, it will get treated like
        // DisposeOverwriteBgcolor below and reset to a completely empty image.)
        const RGBA32Buffer* prevBuffer = &m_frameBufferCache[--frameIndex];
        RGBA32Buffer::FrameDisposalMethod prevMethod =
            prevBuffer->disposalMethod();
        while ((frameIndex > 0)
               && (prevMethod == RGBA32Buffer::DisposeOverwritePrevious)) {
            prevBuffer = &m_frameBufferCache[--frameIndex];
            prevMethod = prevBuffer->disposalMethod();
        }
        ASSERT(prevBuffer->status() == RGBA32Buffer::FrameComplete);

        if ((prevMethod == RGBA32Buffer::DisposeNotSpecified) ||
                (prevMethod == RGBA32Buffer::DisposeKeep)) {
            // Preserve the last frame as the starting state for this frame.
            buffer->copyBitmapData(*prevBuffer);
        } else {
            // We want to clear the previous frame to transparent, without
            // affecting pixels in the image outside of the frame.
            const IntRect& prevRect = prevBuffer->rect();
            if ((frameIndex == 0)
                || prevRect.contains(IntRect(IntPoint(), size()))) {
                // Clearing the first frame, or a frame the size of the whole
                // image, results in a completely empty image.
                if (!buffer->setSize(size().width(), size().height())) {
                    m_failed = true;
                    return false;
                }
            } else {
              // Copy the whole previous buffer, then clear just its frame.
              buffer->copyBitmapData(*prevBuffer);
              for (int y = prevRect.y(); y < prevRect.bottom(); ++y) {
                  for (int x = prevRect.x(); x < prevRect.right(); ++x)
                      buffer->setRGBA(x, y, 0, 0, 0, 0);
              }
              if ((prevRect.width() > 0) && (prevRect.height() > 0))
                  buffer->setHasAlpha(true);
            }
        }
    }

    // Update our status to be partially complete.
    buffer->setStatus(RGBA32Buffer::FramePartial);

    // Reset the alpha pixel tracker for this frame.
    m_currentBufferSawAlpha = false;
    return true;
}

void GIFImageDecoder::haveDecodedRow(unsigned frameIndex,
                                     unsigned char* rowBuffer,
                                     unsigned char* rowEnd,
                                     unsigned rowNumber,
                                     unsigned repeatCount,
                                     bool writeTransparentPixels)
{
    // The pixel data and coordinates supplied to us are relative to the frame's
    // origin within the entire image size, i.e.
    // (m_reader->frameXOffset(), m_reader->frameYOffset()).
    int x = m_reader->frameXOffset();
    const int y = m_reader->frameYOffset() + rowNumber;

    // Sanity-check the arguments.
    if ((rowBuffer == 0) || (y >= size().height()))
        return;

    // Get the colormap.
    unsigned colorMapSize;
    unsigned char* colorMap;
    m_reader->getColorMap(colorMap, colorMapSize);
    if (!colorMap)
        return;

    // Initialize the frame if necessary.
    RGBA32Buffer& buffer = m_frameBufferCache[frameIndex];
    if ((buffer.status() == RGBA32Buffer::FrameEmpty) && !initFrameBuffer(frameIndex))
        return;

    // Write one row's worth of data into the frame.  There is no guarantee that
    // (rowEnd - rowBuffer) == (size().width() - m_reader->frameXOffset()), so
    // we must ensure we don't run off the end of either the source data or the
    // row's X-coordinates.
    for (unsigned char* sourceAddr = rowBuffer; (sourceAddr != rowEnd) && (x < size().width()); ++sourceAddr, ++x) {
        const unsigned char sourceValue = *sourceAddr;
        if ((!m_reader->isTransparent() || (sourceValue != m_reader->transparentPixel())) && (sourceValue < colorMapSize)) {
            const size_t colorIndex = static_cast<size_t>(sourceValue) * 3;
            buffer.setRGBA(x, y, colorMap[colorIndex], colorMap[colorIndex + 1], colorMap[colorIndex + 2], 255);
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
                buffer.setRGBA(x, y, 0, 0, 0, 0);
        }
    }

    // Tell the frame to copy the row data if need be.
    if (repeatCount > 1)
        buffer.copyRowNTimes(m_reader->frameXOffset(), x, y, std::min(y + static_cast<int>(repeatCount), size().height()));
}

void GIFImageDecoder::frameComplete(unsigned frameIndex, unsigned frameDuration, RGBA32Buffer::FrameDisposalMethod disposalMethod)
{
    // Initialize the frame if necessary.  Some GIFs insert do-nothing frames,
    // in which case we never reach haveDecodedRow() before getting here.
    RGBA32Buffer& buffer = m_frameBufferCache[frameIndex];
    if ((buffer.status() == RGBA32Buffer::FrameEmpty) && !initFrameBuffer(frameIndex))
        return;

    buffer.setStatus(RGBA32Buffer::FrameComplete);
    buffer.setDuration(frameDuration);
    buffer.setDisposalMethod(disposalMethod);

    if (!m_currentBufferSawAlpha) {
        // The whole frame was non-transparent, so it's possible that the entire
        // resulting buffer was non-transparent, and we can setHasAlpha(false).
        if (buffer.rect().contains(IntRect(IntPoint(), size())))
            buffer.setHasAlpha(false);
        else if (frameIndex > 0) {
            // Tricky case.  This frame does not have alpha only if everywhere
            // outside its rect doesn't have alpha.  To know whether this is
            // true, we check the start state of the frame -- if it doesn't have
            // alpha, we're safe.
            //
            // First skip over prior DisposeOverwritePrevious frames (since they
            // don't affect the start state of this frame) the same way we do in
            // initFrameBuffer().
            const RGBA32Buffer* prevBuffer = &m_frameBufferCache[--frameIndex];
            while ((frameIndex > 0)
                   && (prevBuffer->disposalMethod() == RGBA32Buffer::DisposeOverwritePrevious))
                prevBuffer = &m_frameBufferCache[--frameIndex];

            // Now, if we're at a DisposeNotSpecified or DisposeKeep frame, then
            // we can say we have no alpha if that frame had no alpha.  But
            // since in initFrameBuffer() we already copied that frame's alpha
            // state into the current frame's, we need do nothing at all here.
            //
            // The only remaining case is a DisposeOverwriteBgcolor frame.  If
            // it had no alpha, and its rect is contained in the current frame's
            // rect, we know the current frame has no alpha.
            if ((prevBuffer->disposalMethod() == RGBA32Buffer::DisposeOverwriteBgcolor)
                && !prevBuffer->hasAlpha() && buffer.rect().contains(prevBuffer->rect()))
                buffer.setHasAlpha(false);
        }
    }
}

void GIFImageDecoder::gifComplete()
{
    if (m_reader)
        m_repetitionCount = m_reader->repetitionCount();
    delete m_reader;
    m_reader = 0;
}

} // namespace WebCore
