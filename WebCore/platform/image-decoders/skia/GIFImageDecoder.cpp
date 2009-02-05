/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
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
bool GIFImageDecoder::isSizeAvailable() const
{
    // If we have pending data to decode, send it to the GIF reader now.
    if (!ImageDecoder::isSizeAvailable() && m_reader) {
        if (m_failed)
            return false;

        // The decoder will go ahead and aggressively consume everything up until the first
        // size is encountered.
        decode(GIFSizeQuery, 0);
    }

    return !m_failed && ImageDecoder::isSizeAvailable();
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
        decode(GIFFullQuery, index+1); // Decode this frame.
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
void GIFImageDecoder::decode(GIFQuery query, unsigned haltAtFrame) const
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
        if (!prepEmptyFrameBuffer(buffer)) {
            buffer->setStatus(RGBA32Buffer::FrameComplete);
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
            // This next line isn't currently necessary since the alpha state is
            // currently carried along in the Skia bitmap data, but it's safe,
            // future-proof, and parallel to the Cairo code.
            buffer->setHasAlpha(prevBuffer->hasAlpha());
        } else {
            // We want to clear the previous frame to transparent, without
            // affecting pixels in the image outside of the frame.
            const IntRect& prevRect = prevBuffer->rect();
            if ((frameIndex == 0)
                || prevRect.contains(IntRect(IntPoint(0, 0), size()))) {
                // Clearing the first frame, or a frame the size of the whole
                // image, results in a completely empty image.
                prepEmptyFrameBuffer(buffer);
            } else {
              // Copy the whole previous buffer, then clear just its frame.
              buffer->copyBitmapData(*prevBuffer);
              // Unnecessary (but safe); see comments on the similar call above.
              buffer->setHasAlpha(prevBuffer->hasAlpha());
              SkBitmap& bitmap = buffer->bitmap();
              for (int y = prevRect.y(); y < prevRect.bottom(); ++y) {
                  for (int x = prevRect.x(); x < prevRect.right(); ++x)
                      buffer->setRGBA(bitmap.getAddr32(x, y), 0, 0, 0, 0);
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

bool GIFImageDecoder::prepEmptyFrameBuffer(RGBA32Buffer* buffer) const
{
    if (!buffer->setSize(size().width(), size().height()))
        return false;
    // This next line isn't currently necessary since Skia's eraseARGB() sets
    // this for us, but we do it for similar reasons to the setHasAlpha() calls
    // in initFrameBuffer() above.
    buffer->setHasAlpha(true);
    return true;
}

void GIFImageDecoder::haveDecodedRow(unsigned frameIndex,
                                     unsigned char* rowBuffer,   // Pointer to single scanline temporary buffer
                                     unsigned char* rowEnd,
                                     unsigned rowNumber,  // The row index
                                     unsigned repeatCount,  // How many times to repeat the row
                                     bool writeTransparentPixels)
{
    // Initialize the frame if necessary.
    RGBA32Buffer& buffer = m_frameBufferCache[frameIndex];
    if ((buffer.status() == RGBA32Buffer::FrameEmpty) && !initFrameBuffer(frameIndex))
        return;

    // Do nothing for bogus data.
    if (rowBuffer == 0 || static_cast<int>(m_reader->frameYOffset() + rowNumber) >= size().height())
        return;

    unsigned colorMapSize;
    unsigned char* colorMap;
    m_reader->getColorMap(colorMap, colorMapSize);
    if (!colorMap)
        return;

    // The buffers that we draw are the entire image's width and height, so a final output frame is
    // width * height RGBA32 values in size.
    //
    // A single GIF frame, however, can be smaller than the entire image, i.e., it can represent some sub-rectangle
    // within the overall image.  The rows we are decoding are within this
    // sub-rectangle.  This means that if the GIF frame's sub-rectangle is (x,y,w,h) then row 0 is really row
    // y, and each row goes from x to x+w.
    unsigned dstPos = (m_reader->frameYOffset() + rowNumber) * size().width() + m_reader->frameXOffset();
    unsigned* dst = buffer.bitmap().getAddr32(0, 0) + dstPos;
    unsigned* dstEnd = dst + size().width() - m_reader->frameXOffset();
    unsigned* currDst = dst;
    unsigned char* currentRowByte = rowBuffer;

    while (currentRowByte != rowEnd && currDst < dstEnd) {
        if ((!m_reader->isTransparent() || *currentRowByte != m_reader->transparentPixel()) && *currentRowByte < colorMapSize) {
            unsigned colorIndex = *currentRowByte * 3;
            unsigned red = colorMap[colorIndex];
            unsigned green = colorMap[colorIndex + 1];
            unsigned blue = colorMap[colorIndex + 2];
            RGBA32Buffer::setRGBA(currDst, red, green, blue, 255);
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
                RGBA32Buffer::setRGBA(currDst, 0, 0, 0, 0);
        }
        currDst++;
        currentRowByte++;
    }

    if (repeatCount > 1) {
        // Copy the row |repeatCount|-1 times.
        unsigned num = currDst - dst;
        unsigned data_size = num * sizeof(unsigned);
        unsigned width = size().width();
        unsigned* end = buffer.bitmap().getAddr32(0, 0) + width * size().height();
        currDst = dst + width;
        for (unsigned i = 1; i < repeatCount; i++) {
            if (currDst + num > end) // Protect against a buffer overrun from a bogus repeatCount.
                break;
            memcpy(currDst, dst, data_size);
            currDst += width;
        }
    }
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
        if (buffer.rect().contains(IntRect(IntPoint(0, 0), size())))
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
