/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Saari <saari@netscape.com>
 *   Apple Computer
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
The Graphics Interchange Format(c) is the copyright property of CompuServe
Incorporated. Only CompuServe Incorporated is authorized to define, redefine,
enhance, alter, modify or change in any way the definition of the format.

CompuServe Incorporated hereby grants a limited, non-exclusive, royalty-free
license for the use of the Graphics Interchange Format(sm) in computer
software; computer software utilizing GIF(sm) must acknowledge ownership of the
Graphics Interchange Format and its Service Mark by CompuServe Incorporated, in
User and Technical Documentation. Computer software utilizing GIF, which is
distributed or may be distributed without User or Technical Documentation must
display to the screen or printer a message acknowledging ownership of the
Graphics Interchange Format and the Service Mark by CompuServe Incorporated; in
this case, the acknowledgement may be displayed in an opening screen or leading
banner, or a closing screen or trailing banner. A message such as the following
may be used:

    "The Graphics Interchange Format(c) is the Copyright property of
    CompuServe Incorporated. GIF(sm) is a Service Mark property of
    CompuServe Incorporated."

For further information, please contact :

    CompuServe Incorporated
    Graphics Technology Department
    5000 Arlington Center Boulevard
    Columbus, Ohio  43220
    U. S. A.

CompuServe Incorporated maintains a mailing list with all those individuals and
organizations who wish to receive copies of this document when it is corrected
or revised. This service is offered free of charge; please provide us with your
mailing address.
*/

#include "config.h"
#include "GIFImageReader.h"

#include <string.h>
#include "GIFImageDecoder.h"
#include "ImageSource.h"

using WebCore::GIFImageDecoder;

// GETN(n, s) requests at least 'n' bytes available from 'q', at start of state 's'.
//
// Note, the hold will never need to be bigger than 256 bytes to gather up in the hold,
// as each GIF block (except colormaps) can never be bigger than 256 bytes.
// Colormaps are directly copied in the resp. global_colormap or dynamically allocated local_colormap.
// So a fixed buffer in GIFImageReader is good enough.
// This buffer is only needed to copy left-over data from one GifWrite call to the next
#define GETN(n, s) \
    do { \
        m_bytesToConsume = (n); \
        m_state = (s); \
    } while (0)

// Get a 16-bit value stored in little-endian format.
#define GETINT16(p)   ((p)[1]<<8|(p)[0])

// Send the data to the display front-end.
bool GIFImageReader::outputRow()
{
    int drowStart = m_frameContext->irow;
    int drowEnd = m_frameContext->irow;

    // Haeberli-inspired hack for interlaced GIFs: Replicate lines while
    // displaying to diminish the "venetian-blind" effect as the image is
    // loaded. Adjust pixel vertical positions to avoid the appearance of the
    // image crawling up the screen as successive passes are drawn.
    if (m_frameContext->progressiveDisplay && m_frameContext->interlaced && m_frameContext->ipass < 4) {
        unsigned rowDup = 0;
        unsigned rowShift = 0;

        switch (m_frameContext->ipass) {
        case 1:
            rowDup = 7;
            rowShift = 3;
            break;
        case 2:
            rowDup = 3;
            rowShift = 1;
            break;
        case 3:
            rowDup = 1;
            rowShift = 0;
            break;
        default:
            break;
        }

        drowStart -= rowShift;
        drowEnd = drowStart + rowDup;

        // Extend if bottom edge isn't covered because of the shift upward.
        if (((m_frameContext->height - 1) - drowEnd) <= rowShift)
            drowEnd = m_frameContext->height - 1;

        // Clamp first and last rows to upper and lower edge of image.
        if (drowStart < 0)
            drowStart = 0;

        if ((unsigned)drowEnd >= m_frameContext->height)
            drowEnd = m_frameContext->height - 1;
    }

    // Protect against too much image data.
    if ((unsigned)drowStart >= m_frameContext->height)
        return true;

    // CALLBACK: Let the client know we have decoded a row.
    if (m_client && m_frameContext
        && !m_client->haveDecodedRow(m_imagesCount - 1, m_frameContext->rowbuf, m_frameContext->rowend,
            drowStart, drowEnd - drowStart + 1, m_frameContext->progressiveDisplay && m_frameContext->interlaced && m_frameContext->ipass > 1))
        return false;

    m_frameContext->rowp = m_frameContext->rowbuf;

    if (!m_frameContext->interlaced)
        m_frameContext->irow++;
    else {
        do {
            switch (m_frameContext->ipass) {
            case 1:
                m_frameContext->irow += 8;
                if (m_frameContext->irow >= m_frameContext->height) {
                    m_frameContext->ipass++;
                    m_frameContext->irow = 4;
                }
                break;

            case 2:
                m_frameContext->irow += 8;
                if (m_frameContext->irow >= m_frameContext->height) {
                    m_frameContext->ipass++;
                    m_frameContext->irow = 2;
                }
                break;

            case 3:
                m_frameContext->irow += 4;
                if (m_frameContext->irow >= m_frameContext->height) {
                    m_frameContext->ipass++;
                    m_frameContext->irow = 1;
                }
                break;

            case 4:
                m_frameContext->irow += 2;
                if (m_frameContext->irow >= m_frameContext->height) {
                    m_frameContext->ipass++;
                    m_frameContext->irow = 0;
                }
                break;

            default:
                break;
            }
        } while (m_frameContext->irow > (m_frameContext->height - 1));
    }
    return true;
}

// Perform Lempel-Ziv-Welch decoding.
bool GIFImageReader::doLZW(const unsigned char* block, size_t bytesInBlock)
{
    if (!m_frameContext)
        return true;

    int code;
    int incode;
    const unsigned char *ch;

    // Copy all the decoder state variables into locals so the compiler
    // won't worry about them being aliased. The locals will be homed
    // back into the GIF decoder structure when we exit.
    int avail = m_frameContext->avail;
    int bits = m_frameContext->bits;
    size_t cnt = bytesInBlock;
    int codesize = m_frameContext->codesize;
    int codemask = m_frameContext->codemask;
    int oldcode = m_frameContext->oldcode;
    int clearCode = m_frameContext->clearCode;
    unsigned char firstchar = m_frameContext->firstchar;
    int datum = m_frameContext->datum;

    if (!m_frameContext->prefix) {
        m_frameContext->prefix = new unsigned short[MAX_BITS];
        memset(m_frameContext->prefix, 0, MAX_BITS * sizeof(short));
    }

    unsigned short *prefix = m_frameContext->prefix;
    unsigned char *stackp = m_frameContext->stackp;
    unsigned char *suffix = m_frameContext->suffix;
    unsigned char *stack = m_frameContext->stack;
    unsigned char *rowp = m_frameContext->rowp;
    unsigned char *rowend = m_frameContext->rowend;
    unsigned rowsRemaining = m_frameContext->rowsRemaining;

    if (rowp == rowend)
        return true;

#define OUTPUT_ROW \
    do { \
        if (!outputRow()) \
            return false; \
        rowsRemaining--; \
        rowp = m_frameContext->rowp; \
        if (!rowsRemaining) \
            goto END; \
    } while (0)

    for (ch = block; cnt-- > 0; ch++) {
        // Feed the next byte into the decoder's 32-bit input buffer.
        datum += ((int) *ch) << bits;
        bits += 8;

        // Check for underflow of decoder's 32-bit input buffer.
        while (bits >= codesize) {
            // Get the leading variable-length symbol from the data stream.
            code = datum & codemask;
            datum >>= codesize;
            bits -= codesize;

            // Reset the dictionary to its original state, if requested.
            if (code == clearCode) {
                codesize = m_frameContext->datasize + 1;
                codemask = (1 << codesize) - 1;
                avail = clearCode + 2;
                oldcode = -1;
                continue;
            }

            // Check for explicit end-of-stream code.
            if (code == (clearCode + 1)) {
                // end-of-stream should only appear after all image data.
                if (!rowsRemaining)
                    return true;
                return m_client ? m_client->setFailed() : false;
            }

            if (oldcode == -1) {
                *rowp++ = suffix[code];
                if (rowp == rowend)
                    OUTPUT_ROW;

                firstchar = oldcode = code;
                continue;
            }

            incode = code;
            if (code >= avail) {
                *stackp++ = firstchar;
                code = oldcode;

                if (stackp == stack + MAX_BITS)
                    return m_client ? m_client->setFailed() : false;
            }

            while (code >= clearCode) {
                if (code >= MAX_BITS || code == prefix[code])
                    return m_client ? m_client->setFailed() : false;

                // Even though suffix[] only holds characters through suffix[avail - 1],
                // allowing code >= avail here lets us be more tolerant of malformed
                // data. As long as code < MAX_BITS, the only risk is a garbled image,
                // which is no worse than refusing to display it.
                *stackp++ = suffix[code];
                code = prefix[code];

                if (stackp == stack + MAX_BITS)
                    return m_client ? m_client->setFailed() : false;
            }

            *stackp++ = firstchar = suffix[code];

            // Define a new codeword in the dictionary.
            if (avail < 4096) {
                prefix[avail] = oldcode;
                suffix[avail] = firstchar;
                avail++;

                // If we've used up all the codewords of a given length
                // increase the length of codewords by one bit, but don't
                // exceed the specified maximum codeword size of 12 bits.
                if ((!(avail & codemask)) && (avail < 4096)) {
                    codesize++;
                    codemask += avail;
                }
            }
            oldcode = incode;

            // Copy the decoded data out to the scanline buffer.
            do {
                *rowp++ = *--stackp;
                if (rowp == rowend)
                    OUTPUT_ROW;
            } while (stackp > stack);
        }
    }

END:
    // Home the local copies of the GIF decoder state variables.
    m_frameContext->avail = avail;
    m_frameContext->bits = bits;
    m_frameContext->codesize = codesize;
    m_frameContext->codemask = codemask;
    m_frameContext->oldcode = oldcode;
    m_frameContext->firstchar = firstchar;
    m_frameContext->datum = datum;
    m_frameContext->stackp = stackp;
    m_frameContext->rowp = rowp;
    m_frameContext->rowsRemaining = rowsRemaining;
    return true;
}

bool GIFImageReader::decode(GIFImageDecoder::GIFQuery query, unsigned haltAtFrame)
{
    ASSERT(m_bytesRead <= m_data->size());
    return decodeInternal(m_bytesRead, m_data->size() - m_bytesRead, query, haltAtFrame);
}

// Process data arriving from the stream for the gif decoder.
bool GIFImageReader::decodeInternal(size_t dataPosition, size_t len, GIFImageDecoder::GIFQuery query, unsigned haltAtFrame)
{
    if (!len) {
        // No new data has come in since the last call, just ignore this call.
        return true;
    }

    if (len < m_bytesToConsume)
        return false;

    // This loop reads as many components from |m_data| as possible.
    // At the beginning of each iteration, dataPosition will be advanced by m_bytesToConsume to
    // point to the next component. len will be decremented accordingly.
    while (len >= m_bytesToConsume) {
        const unsigned char* currentComponent = data(dataPosition);

        // Mark the current component as consumed. Note that currentComponent will remain pointed at this
        // component until the next loop iteration.
        dataPosition += m_bytesToConsume;
        len -= m_bytesToConsume;

        switch (m_state) {
        case GIFLZW:
            // m_bytesToConsume is the current component size because it hasn't been updated.
            if (!doLZW(currentComponent, m_bytesToConsume))
                return false; // If doLZW() encountered an error, it has already called m_client->setFailed().
            GETN(1, GIFSubBlock);
            break;

        case GIFLZWStart: {
            // Initialize LZW parser/decoder.
            int datasize = *currentComponent;

            // Since we use a codesize of 1 more than the datasize, we need to ensure
            // that our datasize is strictly less than the MAX_LZW_BITS value (12).
            // This sets the largest possible codemask correctly at 4095.
            if (datasize >= MAX_LZW_BITS)
                return m_client ? m_client->setFailed() : false;
            int clearCode = 1 << datasize;
            if (clearCode >= MAX_BITS)
                return m_client ? m_client->setFailed() : false;

            if (m_frameContext) {
                m_frameContext->datasize = datasize;
                m_frameContext->clearCode = clearCode;
                m_frameContext->avail = m_frameContext->clearCode + 2;
                m_frameContext->oldcode = -1;
                m_frameContext->codesize = m_frameContext->datasize + 1;
                m_frameContext->codemask = (1 << m_frameContext->codesize) - 1;
                m_frameContext->datum = m_frameContext->bits = 0;

                // Init the tables.
                if (!m_frameContext->suffix)
                    m_frameContext->suffix = new unsigned char[MAX_BITS];

                // Clearing the whole suffix table lets us be more tolerant of bad data.
                memset(m_frameContext->suffix, 0, MAX_BITS);
                for (int i = 0; i < m_frameContext->clearCode; i++)
                    m_frameContext->suffix[i] = i;

                if (!m_frameContext->stack)
                    m_frameContext->stack = new unsigned char[MAX_BITS];
                m_frameContext->stackp = m_frameContext->stack;
            }

            GETN(1, GIFSubBlock);
            break;
        }

        case GIFType: {
            // All GIF files begin with "GIF87a" or "GIF89a".
            if (!strncmp((char*)currentComponent, "GIF89a", 6))
                m_version = 89;
            else if (!strncmp((char*)currentComponent, "GIF87a", 6))
                m_version = 87;
            else
                return m_client ? m_client->setFailed() : false;
            GETN(7, GIFGlobalHeader);
            break;
        }

        case GIFGlobalHeader: {
            // This is the height and width of the "screen" or frame into which images are rendered. The
            // individual images can be smaller than the screen size and located with an origin anywhere
            // within the screen.
            m_screenWidth = GETINT16(currentComponent);
            m_screenHeight = GETINT16(currentComponent + 2);

            // CALLBACK: Inform the decoderplugin of our size.
            if (m_client && !m_client->setSize(m_screenWidth, m_screenHeight))
                return false;

            m_screenBgcolor = currentComponent[5];
            m_globalColormapSize = 2 << (currentComponent[4] & 0x07);

            if ((currentComponent[4] & 0x80) && m_globalColormapSize > 0) { /* global map */
                // Get the global colormap
                const size_t globalColormapBytes = 3 * m_globalColormapSize;
                m_globalColormapPosition = dataPosition;

                if (len < globalColormapBytes) {
                    // Wait until we have enough bytes to consume the entire colormap at once.
                    GETN(globalColormapBytes, GIFGlobalColormap);
                    break;
                }

                m_isGlobalColormapDefined = true;
                dataPosition += globalColormapBytes;
                len -= globalColormapBytes;
            }

            GETN(1, GIFImageStart);

            // currentComponent[6] = Pixel Aspect Ratio
            //   Not used
            //   float aspect = (float)((currentComponent[6] + 15) / 64.0);
            break;
        }

        case GIFGlobalColormap: {
            m_isGlobalColormapDefined = true;
            GETN(1, GIFImageStart);
            break;
        }

        case GIFImageStart: {
            if (*currentComponent == ';') { // terminator.
                GETN(0, GIFDone);
                break;
            }

            if (*currentComponent == '!') { // extension.
                GETN(2, GIFExtension);
                break;
            }

            // If we get anything other than ',' (image separator), '!'
            // (extension), or ';' (trailer), there is extraneous data
            // between blocks. The GIF87a spec tells us to keep reading
            // until we find an image separator, but GIF89a says such
            // a file is corrupt. We follow GIF89a and bail out.
            if (*currentComponent != ',')
                return m_client ? m_client->setFailed() : false;

            GETN(9, GIFImageHeader);
            break;
        }

        case GIFExtension: {
            size_t bytesInBlock = currentComponent[1];
            GIFState es = GIFSkipBlock;

            // The GIF spec mandates lengths for three of the extensions below.
            // However, it's possible for GIFs in the wild to deviate. For example,
            // some GIFs that embed ICC color profiles using GIFApplicationExtension
            // violate the spec and treat this extension block like a sort of
            // "extension + data" block, giving a size greater than 11 and filling the
            // remaining bytes with data (then following with more data blocks as
            // needed), instead of placing a true data block just after the 11 byte
            // extension block.
            //
            // Accordingly, if the specified length is larger than the required value,
            // we use it. If it's smaller, then we enforce the spec value, because the
            // parsers for these extensions expect to have the specified number of
            // bytes available, and if we don't ensure that, they could read off the
            // end of the heap buffer. (In this case, it's likely the GIF is corrupt
            // and we'll soon fail to decode anyway.)
            switch (*currentComponent) {
            case 0xf9:
                es = GIFControlExtension;
                bytesInBlock = std::max(bytesInBlock, static_cast<size_t>(4));
                break;

            case 0x01:
                // ignoring plain text extension
                bytesInBlock = std::max(bytesInBlock, static_cast<size_t>(12));
                break;

            case 0xff:
                es = GIFApplicationExtension;
                bytesInBlock = std::max(bytesInBlock, static_cast<size_t>(11));
                break;

            case 0xfe:
                es = GIFConsumeComment;
                break;
            }

            if (bytesInBlock)
                GETN(bytesInBlock, es);
            else
                GETN(1, GIFImageStart);
            break;
        }

        case GIFConsumeBlock: {
            if (!*currentComponent)
                GETN(1, GIFImageStart);
            else
                GETN(*currentComponent, GIFSkipBlock);
            break;
        }

        case GIFSkipBlock: {
            GETN(1, GIFConsumeBlock);
            break;
        }

        case GIFControlExtension: {
            if (query != GIFImageDecoder::GIFFrameCountQuery) {
                if (!m_frameContext)
                    m_frameContext = new GIFFrameContext();
            }

            if (m_frameContext) {
                if (*currentComponent & 0x1) {
                    m_frameContext->tpixel = currentComponent[3];
                    m_frameContext->isTransparent = true;
                } else {
                    m_frameContext->isTransparent = false;
                    // ignoring gfx control extension
                }
                // NOTE: This relies on the values in the FrameDisposalMethod enum
                // matching those in the GIF spec!
                int disposalMethod = ((*currentComponent) >> 2) & 0x7;
                m_frameContext->disposalMethod = (WebCore::ImageFrame::FrameDisposalMethod)disposalMethod;

                // Some specs say 3rd bit (value 4), other specs say value 3
                // Let's choose 3 (the more popular)
                if (disposalMethod == 4)
                    m_frameContext->disposalMethod = WebCore::ImageFrame::DisposeOverwritePrevious;
                m_frameContext->delayTime = GETINT16(currentComponent + 1) * 10;
            }
            GETN(1, GIFConsumeBlock);
            break;
        }

        case GIFCommentExtension: {
            if (*currentComponent)
                GETN(*currentComponent, GIFConsumeComment);
            else
                GETN(1, GIFImageStart);
            break;
        }

        case GIFConsumeComment: {
            GETN(1, GIFCommentExtension);
            break;
        }

        case GIFApplicationExtension: {
            // Check for netscape application extension.
            if (!strncmp((char*)currentComponent, "NETSCAPE2.0", 11) || !strncmp((char*)currentComponent, "ANIMEXTS1.0", 11))
                GETN(1, GIFNetscapeExtensionBlock);
            else
                GETN(1, GIFConsumeBlock);
            break;
        }

        // Netscape-specific GIF extension: animation looping.
        case GIFNetscapeExtensionBlock: {
            if (*currentComponent)
                GETN(*currentComponent, GIFConsumeNetscapeExtension);
            else
                GETN(1, GIFImageStart);
            break;
        }

        // Parse netscape-specific application extensions
        case GIFConsumeNetscapeExtension: {
            int netscapeExtension = currentComponent[0] & 7;

            // Loop entire animation specified # of times. Only read the loop count during the first iteration.
            if (netscapeExtension == 1) {
                m_loopCount = GETINT16(currentComponent + 1);

                // Zero loop count is infinite animation loop request.
                if (!m_loopCount)
                    m_loopCount = WebCore::cAnimationLoopInfinite;

                GETN(1, GIFNetscapeExtensionBlock);
            } else if (netscapeExtension == 2) {
                // Wait for specified # of bytes to enter buffer.

                // Don't do this, this extension doesn't exist (isn't used at all)
                // and doesn't do anything, as our streaming/buffering takes care of it all...
                // See: http://semmix.pl/color/exgraf/eeg24.htm
                GETN(1, GIFNetscapeExtensionBlock);
            } else {
                // 0,3-7 are yet to be defined netscape extension codes
                return m_client ? m_client->setFailed() : false;
            }
            break;
        }

        case GIFImageHeader: {
            unsigned height, width, xOffset, yOffset;

            /* Get image offsets, with respect to the screen origin */
            xOffset = GETINT16(currentComponent);
            yOffset = GETINT16(currentComponent + 2);

            /* Get image width and height. */
            width  = GETINT16(currentComponent + 4);
            height = GETINT16(currentComponent + 6);

            /* Work around broken GIF files where the logical screen
             * size has weird width or height.  We assume that GIF87a
             * files don't contain animations.
             */
            if (!m_imagesDecoded
                && ((m_screenHeight < height) || (m_screenWidth < width) || (m_version == 87))) {
                m_screenHeight = height;
                m_screenWidth = width;
                xOffset = 0;
                yOffset = 0;

                // CALLBACK: Inform the decoderplugin of our size.
                if (m_client && !m_client->setSize(m_screenWidth, m_screenHeight))
                    return false;
            }

            // Work around more broken GIF files that have zero image width or height
            if (!height || !width) {
                height = m_screenHeight;
                width = m_screenWidth;
                if (!height || !width)
                    return m_client ? m_client->setFailed() : false;
            }

            if (query == GIFImageDecoder::GIFSizeQuery || haltAtFrame == m_imagesDecoded) {
                // The decoder needs to stop. Hand back the number of bytes we consumed from
                // buffer minus 9 (the amount we consumed to read the header).
                setRemainingBytes(len + 9);
                GETN(9, GIFImageHeader);
                return true;
            }

            m_imagesCount = m_imagesDecoded + 1;

            if (query == GIFImageDecoder::GIFFullQuery && !m_frameContext)
                m_frameContext = new GIFFrameContext();

            if (m_frameContext) {
                m_frameContext->xOffset = xOffset;
                m_frameContext->yOffset = yOffset;
                m_frameContext->height = height;
                m_frameContext->width = width;

                /* This case will never be taken if this is the first image */
                /* being decoded. If any of the later images are larger     */
                /* than the screen size, we need to reallocate buffers.     */
                if (m_screenWidth < width) {
                    /* XXX Deviant! */
                    delete []m_frameContext->rowbuf;
                    m_screenWidth = width;
                    m_frameContext->rowbuf = new unsigned char[m_screenWidth];
                } else if (!m_frameContext->rowbuf)
                    m_frameContext->rowbuf = new unsigned char[m_screenWidth];

                if (!m_frameContext->rowbuf)
                    return m_client ? m_client->setFailed() : false;
                if (m_screenHeight < height)
                    m_screenHeight = height;

                if (currentComponent[8] & 0x40) {
                    m_frameContext->interlaced = true;
                    m_frameContext->ipass = 1;
                } else {
                    m_frameContext->interlaced = false;
                    m_frameContext->ipass = 0;
                }

                if (!m_imagesDecoded)
                    m_frameContext->progressiveDisplay = true;
                else {
                    // Overlaying interlaced, transparent GIFs over
                    // existing image data using the Haeberli display hack
                    // requires saving the underlying image in order to
                    // avoid jaggies at the transparency edges. We are
                    // unprepared to deal with that, so don't display such
                    // images progressively
                    m_frameContext->progressiveDisplay = false;
                }

                // Clear state from last image.
                m_frameContext->irow = 0;
                m_frameContext->rowsRemaining = m_frameContext->height;
                m_frameContext->rowend = m_frameContext->rowbuf + m_frameContext->width;
                m_frameContext->rowp = m_frameContext->rowbuf;

                // bits per pixel is currentComponent[8]&0x07
            }

            // has a local colormap?
            if (currentComponent[8] & 0x80) {
                int numColors = 2 << (currentComponent[8] & 0x7);
                const size_t localColormapBytes = 3 * numColors;

                // Switch to the new local palette after it loads
                if (m_frameContext) {
                    m_frameContext->localColormapPosition = dataPosition;
                    m_frameContext->localColormapSize = numColors;
                }

                if (len < localColormapBytes) {
                    // Wait until we have enough bytes to consume the entire colormap at once.
                    GETN(localColormapBytes, GIFImageColormap);
                    break;
                }

                if (m_frameContext)
                    m_frameContext->isLocalColormapDefined = true;
                dataPosition += localColormapBytes;
                len -= localColormapBytes;
            } else if (m_frameContext) {
                // Switch back to the global palette
                m_frameContext->isLocalColormapDefined = false;
            }
            GETN(1, GIFLZWStart);
            break;
        }

        case GIFImageColormap: {
            if (m_frameContext)
                m_frameContext->isLocalColormapDefined = true;
            GETN(1, GIFLZWStart);
            break;
        }

        case GIFSubBlock: {
            // Still working on the same image: Process next LZW data block.
            const size_t bytesInBlock = *currentComponent;
            if (bytesInBlock) {
                // Make sure there are still rows left. If the GIF data
                // is corrupt, we may not get an explicit terminator.
                if (m_frameContext && !m_frameContext->rowsRemaining) {
                    // This is an illegal GIF, but we remain tolerant.
                    GETN(1, GIFSubBlock);
                }
                GETN(bytesInBlock, GIFLZW);
            } else {
                // See if there are any more images in this sequence.
                m_imagesDecoded++;

                // CALLBACK: The frame is now complete.
                if (m_client && m_frameContext && !m_client->frameComplete(m_imagesDecoded - 1, m_frameContext->delayTime, m_frameContext->disposalMethod))
                    return false; // frameComplete() has already called m_client->setFailed().

                // Clear state from this image
                if (m_frameContext) {
                    m_frameContext->isLocalColormapDefined = false;
                    m_frameContext->isTransparent = false;
                }

                GETN(1, GIFImageStart);
            }
            break;
        }

        case GIFDone: {
            // When the GIF is done, we can stop.
            if (m_client)
                m_client->gifComplete();
            return true;
        }

        default:
            // We shouldn't ever get here.
            break;
        }
    }

    setRemainingBytes(len);
    return false;
}

void GIFImageReader::setRemainingBytes(size_t remainingBytes)
{
    ASSERT(remainingBytes <= m_data->size());
    m_bytesRead = m_data->size() - remainingBytes;
}
