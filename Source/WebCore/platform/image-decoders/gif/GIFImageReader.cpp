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
bool GIFImageReader::doLZW(const unsigned char *q)
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
    int cnt = m_count;
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

    for (ch = q; cnt-- > 0; ch++) {
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
    m_count = cnt;
    m_frameContext->oldcode = oldcode;
    m_frameContext->firstchar = firstchar;
    m_frameContext->datum = datum;
    m_frameContext->stackp = stackp;
    m_frameContext->rowp = rowp;
    m_frameContext->rowsRemaining = rowsRemaining;
    return true;
}


// Process data arriving from the stream for the gif decoder.
bool GIFImageReader::read(const unsigned char *buf, unsigned len, GIFImageDecoder::GIFQuery query, unsigned haltAtFrame)
{
    if (!len) {
        // No new data has come in since the last call, just ignore this call.
        return true;
    }

    const unsigned char *q = buf;

    // Add what we have so far to the block.
    // If previous call to me left something in the hold first complete current block
    // or if we are filling the colormaps, first complete the colormap.
    unsigned char* p = 0;
    if (m_state == GIFGlobalColormap)
        p = m_globalColormap;
    else if (m_state == GIFImageColormap)
        p = m_frameContext ? m_frameContext->localColormap : 0;
    else if (m_bytesInHold)
        p = m_hold;
    else
        p = 0;

    if (p || (m_state == GIFGlobalColormap) || (m_state == GIFImageColormap)) {
        // Add what we have sofar to the block
        unsigned length = len < m_bytesToConsume ? len : m_bytesToConsume;
        if (p)
            memcpy(p + m_bytesInHold, buf, length);

        if (length < m_bytesToConsume) {
            // Not enough in 'buf' to complete current block, get more
            m_bytesInHold += length;
            m_bytesToConsume -= length;
            if (m_client)
                m_client->decodingHalted(0);
            return false;
        }

        // Reset hold buffer count
        m_bytesInHold = 0;

        // Point 'q' to complete block in hold (or in colormap)
        q = p;
    }

    // Invariant:
    //    'q' is start of current to be processed block (hold, colormap or buf)
    //    'm_bytesToConsume' is number of bytes to consume from 'buf'
    //    'buf' points to the bytes to be consumed from the input buffer
    //    'len' is number of bytes left in input buffer from position 'buf'.
    //    At entrance of the for loop will 'buf' will be moved 'm_bytesToConsume'
    //    to point to next buffer, 'len' is adjusted accordingly.
    //    So that next round in for loop, q gets pointed to the next buffer.
    for (;len >= m_bytesToConsume; q = buf) {
        // Eat the current block from the buffer, q keeps pointed at current block.
        buf += m_bytesToConsume;
        len -= m_bytesToConsume;

        switch (m_state) {
        case GIFLZW:
            if (!doLZW(q))
                return false; // If doLZW() encountered an error, it has already called m_client->setFailed().
            GETN(1, GIFSubBlock);
            break;

        case GIFLZWStart: {
            // Initialize LZW parser/decoder.
            int datasize = *q;

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
            if (!strncmp((char*)q, "GIF89a", 6))
                m_version = 89;
            else if (!strncmp((char*)q, "GIF87a", 6))
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
            m_screenWidth = GETINT16(q);
            m_screenHeight = GETINT16(q + 2);

            // CALLBACK: Inform the decoderplugin of our size.
            if (m_client && !m_client->setSize(m_screenWidth, m_screenHeight))
                return false;

            m_screenBgcolor = q[5];
            m_globalColormapSize = 2 << (q[4] & 0x07);

            if ((q[4] & 0x80) && m_globalColormapSize > 0) { /* global map */
                // Get the global colormap
                const unsigned size = 3*m_globalColormapSize;

                // Malloc the color map, but only if we're not just counting frames.
                if (query != GIFImageDecoder::GIFFrameCountQuery)
                    m_globalColormap = new unsigned char[size];

                if (len < size) {
                    // Use 'hold' pattern to get the global colormap
                    GETN(size, GIFGlobalColormap);
                    break;
                }

                // Copy everything and go directly to GIFImage_start.
                if (m_globalColormap)
                    memcpy(m_globalColormap, buf, size);
                buf += size;
                len -= size;
            }

            GETN(1, GIFImageStart);

            // q[6] = Pixel Aspect Ratio
            //   Not used
            //   float aspect = (float)((q[6] + 15) / 64.0);
            break;
        }

        case GIFGlobalColormap: {
            // Everything is already copied into m_globalColormap
            GETN(1, GIFImageStart);
            break;
        }

        case GIFImageStart: {
            if (*q == ';') { // terminator.
                GETN(0, GIFDone);
                break;
            }

            if (*q == '!') { // extension.
                GETN(2, GIFExtension);
                break;
            }

            // If we get anything other than ',' (image separator), '!'
            // (extension), or ';' (trailer), there is extraneous data
            // between blocks. The GIF87a spec tells us to keep reading
            // until we find an image separator, but GIF89a says such
            // a file is corrupt. We follow GIF89a and bail out.
            if (*q != ',')
                return m_client ? m_client->setFailed() : false;

            GETN(9, GIFImageHeader);
            break;
        }

        case GIFExtension: {
            m_count = q[1];
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
            switch (*q) {
            case 0xf9:
                es = GIFControlExtension;
                m_count = std::max(m_count, 4);
                break;

            case 0x01:
                // ignoring plain text extension
                m_count = std::max(m_count, 12);
                break;

            case 0xff:
                es = GIFApplicationExtension;
                m_count = std::max(m_count, 11);
                break;

            case 0xfe:
                es = GIFConsumeComment;
                break;
            }

            if (m_count)
                GETN(m_count, es);
            else
                GETN(1, GIFImageStart);
            break;
        }

        case GIFConsumeBlock: {
            if (!*q)
                GETN(1, GIFImageStart);
            else
                GETN(*q, GIFSkipBlock);
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
                if (*q & 0x1) {
                    m_frameContext->tpixel = q[3];
                    m_frameContext->isTransparent = true;
                } else {
                    m_frameContext->isTransparent = false;
                    // ignoring gfx control extension
                }
                // NOTE: This relies on the values in the FrameDisposalMethod enum
                // matching those in the GIF spec!
                int disposalMethod = ((*q) >> 2) & 0x7;
                m_frameContext->disposalMethod = (WebCore::ImageFrame::FrameDisposalMethod)disposalMethod;

                // Some specs say 3rd bit (value 4), other specs say value 3
                // Let's choose 3 (the more popular)
                if (disposalMethod == 4)
                    m_frameContext->disposalMethod = WebCore::ImageFrame::DisposeOverwritePrevious;
                m_frameContext->delayTime = GETINT16(q + 1) * 10;
            }
            GETN(1, GIFConsumeBlock);
            break;
        }

        case GIFCommentExtension: {
            if (*q)
                GETN(*q, GIFConsumeComment);
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
            if (!strncmp((char*)q, "NETSCAPE2.0", 11) || !strncmp((char*)q, "ANIMEXTS1.0", 11))
                GETN(1, GIFNetscapeExtensionBlock);
            else
                GETN(1, GIFConsumeBlock);
            break;
        }

        // Netscape-specific GIF extension: animation looping.
        case GIFNetscapeExtensionBlock: {
            if (*q)
                GETN(*q, GIFConsumeNetscapeExtension);
            else
                GETN(1, GIFImageStart);
            break;
        }

        // Parse netscape-specific application extensions
        case GIFConsumeNetscapeExtension: {
            int netscapeExtension = q[0] & 7;

            // Loop entire animation specified # of times. Only read the loop count during the first iteration.
            if (netscapeExtension == 1) {
                m_loopCount = GETINT16(q + 1);

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
            xOffset = GETINT16(q);
            yOffset = GETINT16(q + 2);

            /* Get image width and height. */
            width  = GETINT16(q + 4);
            height = GETINT16(q + 6);

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
                if (m_client)
                    m_client->decodingHalted(len + 9);
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

                if (q[8] & 0x40) {
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

                // bits per pixel is q[8]&0x07
            }

            // has a local colormap?
            if (q[8] & 0x80) {
                int numColors = 2 << (q[8] & 0x7);
                const unsigned size = 3 * numColors;
                unsigned char *map = m_frameContext ? m_frameContext->localColormap : 0;
                if (m_frameContext && (!map || (numColors > m_frameContext->localColormapSize))) {
                    delete []map;
                    map = new unsigned char[size];
                    if (!map)
                        return m_client ? m_client->setFailed() : false;
                }

                // Switch to the new local palette after it loads
                if (m_frameContext) {
                    m_frameContext->localColormap = map;
                    m_frameContext->localColormapSize = numColors;
                    m_frameContext->isLocalColormapDefined = true;
                }

                if (len < size) {
                    // Use 'hold' pattern to get the image colormap
                    GETN(size, GIFImageColormap);
                    break;
                }

                // Copy everything and directly go to GIFLZWStart
                if (m_frameContext)
                    memcpy(m_frameContext->localColormap, buf, size);
                buf += size;
                len -= size;
            } else if (m_frameContext) {
                // Switch back to the global palette
                m_frameContext->isLocalColormapDefined = false;
            }
            GETN(1, GIFLZWStart);
            break;
        }

        case GIFImageColormap: {
            // Everything is already copied into localColormap
            GETN(1, GIFLZWStart);
            break;
        }

        case GIFSubBlock: {
            // Still working on the same image: Process next LZW data block.
            if ((m_count = *q)) {
                // Make sure there are still rows left. If the GIF data
                // is corrupt, we may not get an explicit terminator.
                if (m_frameContext && !m_frameContext->rowsRemaining) {
                    // This is an illegal GIF, but we remain tolerant.
                    GETN(1, GIFSubBlock);
                }
                GETN(m_count, GIFLZW);
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

    // Copy the leftover into m_frameContext->m_hold
    m_bytesInHold = len;
    if (len) {
        // Add what we have sofar to the block
        unsigned char* p;
        if (m_state == GIFGlobalColormap)
            p = m_globalColormap;
        else if (m_state == GIFImageColormap)
            p = m_frameContext ? m_frameContext->localColormap : 0;
        else
            p = m_hold;
        if (p)
            memcpy(p, buf, len);
        m_bytesToConsume -= len;
    }

    if (m_client)
        m_client->decodingHalted(0);
    return false;
}
