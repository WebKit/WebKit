/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ICOImageDecoder.h"

namespace WebCore {

// Number of bits in .ICO/.CUR used to store the directory and its entries,
// respectively (doesn't match sizeof values for member structs since we omit
// some fields).
static const size_t sizeOfDirectory = 6;
static const size_t sizeOfDirEntry = 16;

void ICOImageDecoder::decodeImage(SharedBuffer* data)
{
    // Read and process directory.
    if ((m_decodedOffset < sizeOfDirectory) && !processDirectory(data))
        return;

    // Read and process directory entries.
    if ((m_decodedOffset < (sizeOfDirectory + (m_directory.idCount * sizeOfDirEntry)))
        && !processDirectoryEntries(data))
        return;

    // Check if this entry is a PNG; we need 4 bytes to check the magic number.
    if (m_imageType == Unknown) {
        if (data->size() < (m_dirEntry.dwImageOffset + 4))
            return;
        m_imageType =
            strncmp(&data->data()[m_dirEntry.dwImageOffset], "\x89PNG", 4) ?
            BMP : PNG;
    }

    // Decode selected entry.
    if (m_imageType == PNG)
        decodePNG(data);
    else {
        // Note that we don't try to limit the bytes we give to the decoder to
        // just the size specified in the icon directory.  If the size given in
        // the directory is insufficient to decode the whole image, the image is
        // corrupt anyway, so whatever we do may be wrong.  The easiest choice
        // (which we do here) is to simply aggressively consume bytes until we
        // run out of bytes, finish decoding, or hit a sequence that makes the
        // decoder fail.
        decodeBMP(data);
    }
}

RGBA32Buffer* ICOImageDecoder::frameBufferAtIndex(size_t index)
{
    return (m_imageType == PNG) ? m_pngDecoder.frameBufferAtIndex(0) :
        BMPImageReader::frameBufferAtIndex(0);
}

bool ICOImageDecoder::isSizeAvailable() const
{
    return (m_imageType == PNG) ? m_pngDecoder.isSizeAvailable() :
        BMPImageReader::isSizeAvailable();
}

IntSize ICOImageDecoder::size() const
{
    return (m_imageType == PNG) ? m_pngDecoder.size() : BMPImageReader::size();
}

bool ICOImageDecoder::processDirectory(SharedBuffer* data)
{
    // Read directory.
    ASSERT(!m_decodedOffset);
    if (data->size() < sizeOfDirectory)
        return false;
    const uint16_t fileType = readUint16(data, 2);
    m_directory.idCount = readUint16(data, 4);
    m_decodedOffset = sizeOfDirectory;

    // See if this is an icon filetype we understand, and make sure we have at
    // least one entry in the directory.
    enum {
        ICON = 1,
        CURSOR = 2,
    };
    if (((fileType != ICON) && (fileType != CURSOR)) ||
            (m_directory.idCount == 0))
        m_failed = true;

    return !m_failed;
}

bool ICOImageDecoder::processDirectoryEntries(SharedBuffer* data)
{
    // Read directory entries.
    ASSERT(m_decodedOffset == sizeOfDirectory);
    if ((m_decodedOffset > data->size()) || (data->size() - m_decodedOffset) <
            (m_directory.idCount * sizeOfDirEntry))
        return false;
    for (int i = 0; i < m_directory.idCount; ++i) {
        const IconDirectoryEntry dirEntry = readDirectoryEntry(data);
        if ((i == 0) || isBetterEntry(dirEntry))
            m_dirEntry = dirEntry;
    }

    // Make sure the specified image offset is past the end of the directory
    // entries, and that the offset isn't so large that it overflows when we add
    // 4 bytes to it (which we do in decodeImage() while ensuring it's safe to
    // examine the first 4 bytes of the image data).
    if ((m_dirEntry.dwImageOffset < m_decodedOffset) ||
            ((m_dirEntry.dwImageOffset + 4) < m_dirEntry.dwImageOffset)) {
      m_failed = true;
      return false;
    }

    // Ready to decode the image at the specified offset.
    m_decodedOffset = m_headerOffset = m_dirEntry.dwImageOffset;
    return true;
}

ICOImageDecoder::IconDirectoryEntry ICOImageDecoder::readDirectoryEntry(
    SharedBuffer* data)
{
    // Read icon data.
    IconDirectoryEntry entry;
    entry.bWidth = static_cast<uint8_t>(data->data()[m_decodedOffset]);
    if (entry.bWidth == 0)
        entry.bWidth = 256;
    entry.bHeight = static_cast<uint8_t>(data->data()[m_decodedOffset + 1]);
    if (entry.bHeight == 0)
        entry.bHeight = 256;
    entry.wBitCount = readUint16(data, 6);
    entry.dwImageOffset = readUint32(data, 12);

    // Some icons don't have a bit depth, only a color count.  Convert the
    // color count to the minimum necessary bit depth.  It doesn't matter if
    // this isn't quite what the bitmap info header says later, as we only use
    // this value to determine which icon entry is best.
    if (!entry.wBitCount) {
        uint8_t colorCount = data->data()[m_decodedOffset + 2];
        if (colorCount) {
            for (--colorCount; colorCount; colorCount >>= 1)
                ++entry.wBitCount;
        }
    }

    m_decodedOffset += sizeOfDirEntry;
    return entry;
}

bool ICOImageDecoder::isBetterEntry(const IconDirectoryEntry& entry) const
{
    const IntSize entrySize(entry.bWidth, entry.bHeight);
    const IntSize dirEntrySize(m_dirEntry.bWidth, m_dirEntry.bHeight);
    const int entryArea = entry.bWidth * entry.bHeight;
    const int dirEntryArea = m_dirEntry.bWidth * m_dirEntry.bHeight;

    if ((entrySize != dirEntrySize) && !m_preferredIconSize.isEmpty()) {
        // An icon of exactly the preferred size is best.
        if (entrySize == m_preferredIconSize)
            return true;
        if (dirEntrySize == m_preferredIconSize)
            return false;

        // The icon closest to the preferred area without being smaller is
        // better.
        if (entryArea != dirEntryArea) {
            return (entryArea < dirEntryArea)
                && (entryArea >= (m_preferredIconSize.width() * m_preferredIconSize.height()));
        }
    }

    // Larger icons are better.
    if (entryArea != dirEntryArea)
        return (entryArea > dirEntryArea);

    // Higher bit-depth icons are better.
    return (entry.wBitCount > m_dirEntry.wBitCount);
}

void ICOImageDecoder::decodePNG(SharedBuffer* data)
{
    // Copy out PNG data to a separate vector and instantiate PNG decoder.
    // It would be nice to save this copy, if I could figure out how to just
    // offset the perceived start of |data| by |m_dirEntry.dwImageOffset| when
    // passing it to setData()...
    RefPtr<SharedBuffer> pngData(
        SharedBuffer::create(&data->data()[m_dirEntry.dwImageOffset],
                             data->size() - m_dirEntry.dwImageOffset));
    m_pngDecoder.setData(pngData.get(), true);

    // Decode PNG as a side effect of asking for the frame.  Strangely, it's
    // seemingly unsafe to call decode() or isSizeAvailable() before calling
    // this, as this is the only function that enlarges the framebuffer to
    // nonzero size, and before this happens any decoded image data is silently
    // thrown away and never decoded again (!).
    m_pngDecoder.frameBufferAtIndex(0);
    m_failed = m_pngDecoder.failed();

    // Sanity-check that the size is what we expected.
    if (isSizeAvailable() && ((size().width() != m_dirEntry.bWidth) ||
            (size().height() != m_dirEntry.bHeight)))
        m_failed = true;
}

}
