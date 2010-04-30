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

using WebCore::GIFImageDecoder;

// Define the Mozilla macro setup so that we can leave the macros alone.
#define PR_BEGIN_MACRO  do {
#define PR_END_MACRO    } while (0)

/*
 * GETN(n, s) requests at least 'n' bytes available from 'q', at start of state 's'
 *
 * Note, the hold will never need to be bigger than 256 bytes to gather up in the hold,
 * as each GIF block (except colormaps) can never be bigger than 256 bytes.
 * Colormaps are directly copied in the resp. global_colormap or dynamically allocated local_colormap.
 * So a fixed buffer in GIFImageReader is good enough.
 * This buffer is only needed to copy left-over data from one GifWrite call to the next
 */
#define GETN(n,s)                    \
  PR_BEGIN_MACRO                     \
    bytes_to_consume = (n);      \
    state = (s);                 \
  PR_END_MACRO

/* Get a 16-bit value stored in little-endian format */
#define GETINT16(p)   ((p)[1]<<8|(p)[0])

//******************************************************************************
// Send the data to the display front-end.
bool GIFImageReader::output_row()
{
  GIFFrameReader* gs = frame_reader;

  int drow_start, drow_end;

  drow_start = drow_end = gs->irow;

  /*
   * Haeberli-inspired hack for interlaced GIFs: Replicate lines while
   * displaying to diminish the "venetian-blind" effect as the image is
   * loaded. Adjust pixel vertical positions to avoid the appearance of the
   * image crawling up the screen as successive passes are drawn.
   */
  if (gs->progressive_display && gs->interlaced && gs->ipass < 4) {
    unsigned row_dup = 0, row_shift = 0;

    switch (gs->ipass) {
    case 1:
      row_dup = 7;
      row_shift = 3;
      break;
    case 2:
      row_dup = 3;
      row_shift = 1;
      break;
    case 3:
      row_dup = 1;
      row_shift = 0;
      break;
    default:
      break;
    }

    drow_start -= row_shift;
    drow_end = drow_start + row_dup;

    /* Extend if bottom edge isn't covered because of the shift upward. */
    if (((gs->height - 1) - drow_end) <= row_shift)
      drow_end = gs->height - 1;

    /* Clamp first and last rows to upper and lower edge of image. */
    if (drow_start < 0)
      drow_start = 0;
    if ((unsigned)drow_end >= gs->height)
      drow_end = gs->height - 1;
  }

  /* Protect against too much image data */
  if ((unsigned)drow_start >= gs->height)
    return true;

  // CALLBACK: Let the client know we have decoded a row.
  if (clientptr && frame_reader &&
      !clientptr->haveDecodedRow(images_count - 1, frame_reader->rowbuf, frame_reader->rowend,
                                 drow_start, drow_end - drow_start + 1,
                                 gs->progressive_display && gs->interlaced && gs->ipass > 1))
    return false;

  gs->rowp = gs->rowbuf;

  if (!gs->interlaced)
    gs->irow++;
  else {
    do {
      switch (gs->ipass)
      {
        case 1:
          gs->irow += 8;
          if (gs->irow >= gs->height) {
            gs->ipass++;
            gs->irow = 4;
          }
          break;

        case 2:
          gs->irow += 8;
          if (gs->irow >= gs->height) {
            gs->ipass++;
            gs->irow = 2;
          }
          break;

        case 3:
          gs->irow += 4;
          if (gs->irow >= gs->height) {
            gs->ipass++;
            gs->irow = 1;
          }
          break;

        case 4:
          gs->irow += 2;
          if (gs->irow >= gs->height){
            gs->ipass++;
            gs->irow = 0;
          }
          break;

        default:
          break;
      }
    } while (gs->irow > (gs->height - 1));
  }

  return true;
}

//******************************************************************************
/* Perform Lempel-Ziv-Welch decoding */
bool GIFImageReader::do_lzw(const unsigned char *q)
{
  GIFFrameReader* gs = frame_reader;
  if (!gs)
    return true;

  int code;
  int incode;
  const unsigned char *ch;
  
  /* Copy all the decoder state variables into locals so the compiler
   * won't worry about them being aliased.  The locals will be homed
   * back into the GIF decoder structure when we exit.
   */
  int avail       = gs->avail;
  int bits        = gs->bits;
  int cnt         = count;
  int codesize    = gs->codesize;
  int codemask    = gs->codemask;
  int oldcode     = gs->oldcode;
  int clear_code  = gs->clear_code;
  unsigned char firstchar = gs->firstchar;
  int datum     = gs->datum;

  if (!gs->prefix) {
    gs->prefix = new unsigned short[MAX_BITS];
    memset(gs->prefix, 0, MAX_BITS * sizeof(short));
  }

  unsigned short *prefix  = gs->prefix;
  unsigned char *stackp   = gs->stackp;
  unsigned char *suffix   = gs->suffix;
  unsigned char *stack    = gs->stack;
  unsigned char *rowp     = gs->rowp;
  unsigned char *rowend   = gs->rowend;
  unsigned rows_remaining = gs->rows_remaining;

  if (rowp == rowend)
    return true;

#define OUTPUT_ROW                                                  \
  PR_BEGIN_MACRO                                                        \
    if (!output_row())                                                     \
      return false;                                                        \
    rows_remaining--;                                                   \
    rowp = frame_reader->rowp;                                                    \
    if (!rows_remaining)                                                \
      goto END;                                                         \
  PR_END_MACRO

  for (ch = q; cnt-- > 0; ch++)
  {
    /* Feed the next byte into the decoder's 32-bit input buffer. */
    datum += ((int) *ch) << bits;
    bits += 8;

    /* Check for underflow of decoder's 32-bit input buffer. */
    while (bits >= codesize)
    {
      /* Get the leading variable-length symbol from the data stream */
      code = datum & codemask;
      datum >>= codesize;
      bits -= codesize;

      /* Reset the dictionary to its original state, if requested */
      if (code == clear_code) {
        codesize = gs->datasize + 1;
        codemask = (1 << codesize) - 1;
        avail = clear_code + 2;
        oldcode = -1;
        continue;
      }

      /* Check for explicit end-of-stream code */
      if (code == (clear_code + 1)) {
        /* end-of-stream should only appear after all image data */
        if (!rows_remaining)
          return true;
        return clientptr ? clientptr->setFailed() : false;
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
          return clientptr ? clientptr->setFailed() : false;
      }

      while (code >= clear_code)
      {
        if (code >= MAX_BITS || code == prefix[code])
          return clientptr ? clientptr->setFailed() : false;

        // Even though suffix[] only holds characters through suffix[avail - 1],
        // allowing code >= avail here lets us be more tolerant of malformed
        // data. As long as code < MAX_BITS, the only risk is a garbled image,
        // which is no worse than refusing to display it.
        *stackp++ = suffix[code];
        code = prefix[code];

        if (stackp == stack + MAX_BITS)
          return clientptr ? clientptr->setFailed() : false;
      }

      *stackp++ = firstchar = suffix[code];

      /* Define a new codeword in the dictionary. */
      if (avail < 4096) {
        prefix[avail] = oldcode;
        suffix[avail] = firstchar;
        avail++;

        /* If we've used up all the codewords of a given length
         * increase the length of codewords by one bit, but don't
         * exceed the specified maximum codeword size of 12 bits.
         */
        if (((avail & codemask) == 0) && (avail < 4096)) {
          codesize++;
          codemask += avail;
        }
      }
      oldcode = incode;

        /* Copy the decoded data out to the scanline buffer. */
      do {
        *rowp++ = *--stackp;
        if (rowp == rowend) {
          OUTPUT_ROW;
        }
      } while (stackp > stack);
    }
  }

  END:

  /* Home the local copies of the GIF decoder state variables */
  gs->avail = avail;
  gs->bits = bits;
  gs->codesize = codesize;
  gs->codemask = codemask;
  count = cnt;
  gs->oldcode = oldcode;
  gs->firstchar = firstchar;
  gs->datum = datum;
  gs->stackp = stackp;
  gs->rowp = rowp;
  gs->rows_remaining = rows_remaining;

  return true;
}


/******************************************************************************/
/*
 * process data arriving from the stream for the gif decoder
 */

bool GIFImageReader::read(const unsigned char *buf, unsigned len, 
                     GIFImageDecoder::GIFQuery query, unsigned haltAtFrame)
{
  if (!len) {
    // No new data has come in since the last call, just ignore this call.
    return true;
  }

  const unsigned char *q = buf;

  // Add what we have so far to the block
  // If previous call to me left something in the hold first complete current block
  // Or if we are filling the colormaps, first complete the colormap
  unsigned char* p = 0;
  if (state == gif_global_colormap)
    p = global_colormap;
  else if (state == gif_image_colormap)
    p = frame_reader ? frame_reader->local_colormap : 0;
  else if (bytes_in_hold)
    p = hold;
  else
    p = 0;

  if (p || (state == gif_global_colormap) || (state == gif_image_colormap)) {
    // Add what we have sofar to the block
    unsigned l = len < bytes_to_consume ? len : bytes_to_consume;
    if (p)
        memcpy(p + bytes_in_hold, buf, l);

    if (l < bytes_to_consume) {
      // Not enough in 'buf' to complete current block, get more
      bytes_in_hold += l;
      bytes_to_consume -= l;
      if (clientptr)
        clientptr->decodingHalted(0);
      return false;
    }
    // Reset hold buffer count
    bytes_in_hold = 0;
    // Point 'q' to complete block in hold (or in colormap)
    q = p;
  }

  // Invariant:
  //    'q' is start of current to be processed block (hold, colormap or buf)
  //    'bytes_to_consume' is number of bytes to consume from 'buf'
  //    'buf' points to the bytes to be consumed from the input buffer
  //    'len' is number of bytes left in input buffer from position 'buf'.
  //    At entrance of the for loop will 'buf' will be moved 'bytes_to_consume'
  //    to point to next buffer, 'len' is adjusted accordingly.
  //    So that next round in for loop, q gets pointed to the next buffer.

  for (;len >= bytes_to_consume; q=buf) {
    // Eat the current block from the buffer, q keeps pointed at current block
    buf += bytes_to_consume;
    len -= bytes_to_consume;

    switch (state)
    {
    case gif_lzw:
      if (!do_lzw(q))
        return false; // If do_lzw() encountered an error, it has already called
                      // clientptr->setFailed().
      GETN(1, gif_sub_block);
      break;

    case gif_lzw_start:
    {
      /* Initialize LZW parser/decoder */
      int datasize = *q;
      // Since we use a codesize of 1 more than the datasize, we need to ensure
      // that our datasize is strictly less than the MAX_LZW_BITS value (12).
      // This sets the largest possible codemask correctly at 4095.
      if (datasize >= MAX_LZW_BITS)
        return clientptr ? clientptr->setFailed() : false;
      int clear_code = 1 << datasize;
      if (clear_code >= MAX_BITS)
        return clientptr ? clientptr->setFailed() : false;

      if (frame_reader) {
        frame_reader->datasize = datasize;
        frame_reader->clear_code = clear_code;
        frame_reader->avail = frame_reader->clear_code + 2;
        frame_reader->oldcode = -1;
        frame_reader->codesize = frame_reader->datasize + 1;
        frame_reader->codemask = (1 << frame_reader->codesize) - 1;

        frame_reader->datum = frame_reader->bits = 0;

        /* init the tables */
        if (!frame_reader->suffix)
          frame_reader->suffix = new unsigned char[MAX_BITS];
        // Clearing the whole suffix table lets us be more tolerant of bad data.
        memset(frame_reader->suffix, 0, MAX_BITS);
        for (int i = 0; i < frame_reader->clear_code; i++)
          frame_reader->suffix[i] = i;

        if (!frame_reader->stack)
          frame_reader->stack = new unsigned char[MAX_BITS];
        frame_reader->stackp = frame_reader->stack;
      }

      GETN(1, gif_sub_block);
    }
    break;

    /* All GIF files begin with "GIF87a" or "GIF89a" */
    case gif_type:
    {
      if (!strncmp((char*)q, "GIF89a", 6))
        version = 89;
      else if (!strncmp((char*)q, "GIF87a", 6))
        version = 87;
      else
        return clientptr ? clientptr->setFailed() : false;
      GETN(7, gif_global_header);
    }
    break;

    case gif_global_header:
    {
      /* This is the height and width of the "screen" or
       * frame into which images are rendered.  The
       * individual images can be smaller than the
       * screen size and located with an origin anywhere
       * within the screen.
       */

      screen_width = GETINT16(q);
      screen_height = GETINT16(q + 2);

      // CALLBACK: Inform the decoderplugin of our size.
      if (clientptr && !clientptr->setSize(screen_width, screen_height))
        return false;
      
      screen_bgcolor = q[5];
      global_colormap_size = 2<<(q[4]&0x07);

      if ((q[4] & 0x80) && global_colormap_size > 0) { /* global map */
        // Get the global colormap
        const unsigned size = 3*global_colormap_size;
        
        // Malloc the color map, but only if we're not just counting frames.
        if (query != GIFImageDecoder::GIFFrameCountQuery)
          global_colormap = new unsigned char[size];

        if (len < size) {
          // Use 'hold' pattern to get the global colormap
          GETN(size, gif_global_colormap);
          break;
        }
        
        // Copy everything and go directly to gif_image_start.
        if (global_colormap)
            memcpy(global_colormap, buf, size);
        buf += size;
        len -= size;
      }

      GETN(1, gif_image_start);

      // q[6] = Pixel Aspect Ratio
      //   Not used
      //   float aspect = (float)((q[6] + 15) / 64.0);
    }
    break;

    case gif_global_colormap:
      // Everything is already copied into global_colormap
      GETN(1, gif_image_start);
    break;

    case gif_image_start:
    {
      if (*q == ';') { /* terminator */
        state = gif_done;
        break;
      }

      if (*q == '!') { /* extension */
        GETN(2, gif_extension);
        break;
      }

      /* If we get anything other than ',' (image separator), '!'
       * (extension), or ';' (trailer), there is extraneous data
       * between blocks. The GIF87a spec tells us to keep reading
       * until we find an image separator, but GIF89a says such
       * a file is corrupt. We follow GIF89a and bail out. */
      if (*q != ',') {
        if (images_decoded > 0) {
          /* The file is corrupt, but one or more images have
           * been decoded correctly. In this case, we proceed
           * as if the file were correctly terminated and set
           * the state to gif_done, so the GIF will display.
           */
          state = gif_done;
        } else {
          /* No images decoded, there is nothing to display. */
          return clientptr ? clientptr->setFailed() : false;
        }
        break;
      } else
        GETN(9, gif_image_header);
    }
    break;

    case gif_extension:
    {
      int len = count = q[1];
      gstate es = gif_skip_block;

      switch (*q)
      {
      case 0xf9:
        es = gif_control_extension;
        break;

      case 0x01:
        // ignoring plain text extension
        break;

      case 0xff:
        es = gif_application_extension;
        break;

      case 0xfe:
        es = gif_consume_comment;
        break;
      }

      if (len)
        GETN(len, es);
      else
        GETN(1, gif_image_start);
    }
    break;

    case gif_consume_block:
      if (!*q)
        GETN(1, gif_image_start);
      else
        GETN(*q, gif_skip_block);
    break;

    case gif_skip_block:
      GETN(1, gif_consume_block);
      break;

    case gif_control_extension:
    {
      if (query != GIFImageDecoder::GIFFrameCountQuery) {
          if (!frame_reader)
            frame_reader = new GIFFrameReader();
      }

      if (frame_reader) {
        if (*q & 0x1) {
          frame_reader->tpixel = q[3];
          frame_reader->is_transparent = true;
        } else {
          frame_reader->is_transparent = false;
          // ignoring gfx control extension
        }
        // NOTE: This relies on the values in the FrameDisposalMethod enum
        // matching those in the GIF spec!
        frame_reader->disposal_method = (WebCore::RGBA32Buffer::FrameDisposalMethod)(((*q) >> 2) & 0x7);
        // Some specs say 3rd bit (value 4), other specs say value 3
        // Let's choose 3 (the more popular)
        if (frame_reader->disposal_method == 4)
          frame_reader->disposal_method = WebCore::RGBA32Buffer::DisposeOverwritePrevious;
        frame_reader->delay_time = GETINT16(q + 1) * 10;
      }
      GETN(1, gif_consume_block);
    }
    break;

    case gif_comment_extension:
    {
      if (*q)
        GETN(*q, gif_consume_comment);
      else
        GETN(1, gif_image_start);
    }
    break;

    case gif_consume_comment:
      GETN(1, gif_comment_extension);
    break;

    case gif_application_extension:
      /* Check for netscape application extension */
      if (!strncmp((char*)q, "NETSCAPE2.0", 11) ||
        !strncmp((char*)q, "ANIMEXTS1.0", 11))
        GETN(1, gif_netscape_extension_block);
      else
        GETN(1, gif_consume_block);
    break;

    /* Netscape-specific GIF extension: animation looping */
    case gif_netscape_extension_block:
      if (*q)
        GETN(*q, gif_consume_netscape_extension);
      else
        GETN(1, gif_image_start);
    break;

    /* Parse netscape-specific application extensions */
    case gif_consume_netscape_extension:
    {
      int netscape_extension = q[0] & 7;

      /* Loop entire animation specified # of times.  Only read the
         loop count during the first iteration. */
      if (netscape_extension == 1) {
        loop_count = GETINT16(q + 1);

        GETN(1, gif_netscape_extension_block);
      }
      /* Wait for specified # of bytes to enter buffer */
      else if (netscape_extension == 2) {
        // Don't do this, this extension doesn't exist (isn't used at all) 
        // and doesn't do anything, as our streaming/buffering takes care of it all...
        // See: http://semmix.pl/color/exgraf/eeg24.htm
        GETN(1, gif_netscape_extension_block);
      } else {
        // 0,3-7 are yet to be defined netscape extension codes
        return clientptr ? clientptr->setFailed() : false;
      }

      break;
    }

    case gif_image_header:
    {
      unsigned height, width, x_offset, y_offset;
      
      /* Get image offsets, with respect to the screen origin */
      x_offset = GETINT16(q);
      y_offset = GETINT16(q + 2);

      /* Get image width and height. */
      width  = GETINT16(q + 4);
      height = GETINT16(q + 6);

      /* Work around broken GIF files where the logical screen
       * size has weird width or height.  We assume that GIF87a
       * files don't contain animations.
       */
      if ((images_decoded == 0) &&
          ((screen_height < height) || (screen_width < width) ||
           (version == 87)))
      {
        screen_height = height;
        screen_width = width;
        x_offset = 0;
        y_offset = 0;

        // CALLBACK: Inform the decoderplugin of our size.
        if (clientptr && !clientptr->setSize(screen_width, screen_height))
          return false;
      }

      /* Work around more broken GIF files that have zero image
         width or height */
      if (!height || !width) {
        height = screen_height;
        width = screen_width;
        if (!height || !width)
          return clientptr ? clientptr->setFailed() : false;
      }

      if (query == GIFImageDecoder::GIFSizeQuery || haltAtFrame == images_decoded) {
        // The decoder needs to stop.  Hand back the number of bytes we consumed from
        // buffer minus 9 (the amount we consumed to read the header).
        if (clientptr)
            clientptr->decodingHalted(len + 9);
        GETN(9, gif_image_header);
        return true;
      }
      
      images_count = images_decoded + 1;

      if (query == GIFImageDecoder::GIFFullQuery && !frame_reader)
        frame_reader = new GIFFrameReader();

      if (frame_reader) {
        frame_reader->x_offset = x_offset;
        frame_reader->y_offset = y_offset;
        frame_reader->height = height;
        frame_reader->width = width;

        /* This case will never be taken if this is the first image */
        /* being decoded. If any of the later images are larger     */
        /* than the screen size, we need to reallocate buffers.     */
        if (screen_width < width) {
          /* XXX Deviant! */

          delete []frame_reader->rowbuf;
          screen_width = width;
          frame_reader->rowbuf = new unsigned char[screen_width];
        } else if (!frame_reader->rowbuf) {
          frame_reader->rowbuf = new unsigned char[screen_width];
        }

        if (!frame_reader->rowbuf)
          return clientptr ? clientptr->setFailed() : false;
        if (screen_height < height)
          screen_height = height;

        if (q[8] & 0x40) {
          frame_reader->interlaced = true;
          frame_reader->ipass = 1;
        } else {
          frame_reader->interlaced = false;
          frame_reader->ipass = 0;
        }

        if (images_decoded == 0) {
          frame_reader->progressive_display = true;
        } else {
          /* Overlaying interlaced, transparent GIFs over
             existing image data using the Haeberli display hack
             requires saving the underlying image in order to
             avoid jaggies at the transparency edges.  We are
             unprepared to deal with that, so don't display such
             images progressively */
          frame_reader->progressive_display = false;
        }

        /* Clear state from last image */
        frame_reader->irow = 0;
        frame_reader->rows_remaining = frame_reader->height;
        frame_reader->rowend = frame_reader->rowbuf + frame_reader->width;
        frame_reader->rowp = frame_reader->rowbuf;

        /* bits per pixel is q[8]&0x07 */
      }
      
      if (q[8] & 0x80) /* has a local colormap? */
      {
        int num_colors = 2 << (q[8] & 0x7);
        const unsigned size = 3*num_colors;
        unsigned char *map = frame_reader ? frame_reader->local_colormap : 0;
        if (frame_reader && (!map || (num_colors > frame_reader->local_colormap_size))) {
          delete []map;
          map = new unsigned char[size];
          if (!map)
            return clientptr ? clientptr->setFailed() : false;
        }

        /* Switch to the new local palette after it loads */
        if (frame_reader) {
          frame_reader->local_colormap = map;
          frame_reader->local_colormap_size = num_colors;
          frame_reader->is_local_colormap_defined = true;
        }

        if (len < size) {
          // Use 'hold' pattern to get the image colormap
          GETN(size, gif_image_colormap);
          break;
        }
        // Copy everything and directly go to gif_lzw_start
        if (frame_reader)
          memcpy(frame_reader->local_colormap, buf, size);
        buf += size;
        len -= size;
      } else if (frame_reader) {
        /* Switch back to the global palette */
        frame_reader->is_local_colormap_defined = false;
      }
      GETN(1, gif_lzw_start);
    }
    break;

    case gif_image_colormap:
      // Everything is already copied into local_colormap
      GETN(1, gif_lzw_start);
    break;

    case gif_sub_block:
    {
      if ((count = *q) != 0)
      /* Still working on the same image: Process next LZW data block */
      {
        /* Make sure there are still rows left. If the GIF data */
        /* is corrupt, we may not get an explicit terminator.   */
        if (frame_reader && frame_reader->rows_remaining == 0) {
          /* This is an illegal GIF, but we remain tolerant. */
          GETN(1, gif_sub_block);
        }
        GETN(count, gif_lzw);
      }
      else
      /* See if there are any more images in this sequence. */
      {
        images_decoded++;

        // CALLBACK: The frame is now complete.
        if (clientptr && frame_reader && !clientptr->frameComplete(images_decoded - 1, frame_reader->delay_time, frame_reader->disposal_method))
          return false; // frameComplete() has already called
                        // clientptr->setFailed().

        /* Clear state from this image */
        if (frame_reader) {
            frame_reader->is_local_colormap_defined = false;
            frame_reader->is_transparent = false;
        }

        GETN(1, gif_image_start);
      }
    }
    break;

    case gif_done:
      // When the GIF is done, we can stop.
      if (clientptr)
        clientptr->gifComplete();
      return true;

    // We shouldn't ever get here.
    default:
      break;
    }
  }

  // Copy the leftover into gs->hold
  bytes_in_hold = len;
  if (len) {
    // Add what we have sofar to the block
    unsigned char* p;
    if (state == gif_global_colormap)
      p = global_colormap;
    else if (state == gif_image_colormap)
      p = frame_reader ? frame_reader->local_colormap : 0;
    else
      p = hold;
    if (p)
      memcpy(p, buf, len);
    bytes_to_consume -= len;
  }

  if (clientptr)
    clientptr->decodingHalted(0);
  return false;
}
