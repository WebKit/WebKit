/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef GIFImageReader_h
#define GIFImageReader_h

// Define ourselves as the clientPtr.  Mozilla just hacked their C++ callback class into this old C decoder,
// so we will too.
#include "GIFImageDecoder.h"

#define MAX_LZW_BITS          12
#define MAX_BITS            4097 /* 2^MAX_LZW_BITS+1 */
#define MAX_COLORS           256
#define MAX_HOLD_SIZE        256

const int cLoopCountNotSeen = -2;

/* gif2.h  
   The interface for the GIF87/89a decoder. 
*/
// List of possible parsing states
typedef enum {
    gif_type,
    gif_global_header,
    gif_global_colormap,
    gif_image_start,            
    gif_image_header,
    gif_image_colormap,
    gif_image_body,
    gif_lzw_start,
    gif_lzw,
    gif_sub_block,
    gif_extension,
    gif_control_extension,
    gif_consume_block,
    gif_skip_block,
    gif_done,
    gif_oom,
    gif_error,
    gif_comment_extension,
    gif_application_extension,
    gif_netscape_extension_block,
    gif_consume_netscape_extension,
    gif_consume_comment
} gstate;

struct GIFFrameReader {
    /* LZW decoder state machine */
    unsigned char *stackp;              /* Current stack pointer */
    int datasize;
    int codesize;
    int codemask;
    int clear_code;             /* Codeword used to trigger dictionary reset */
    int avail;                  /* Index of next available slot in dictionary */
    int oldcode;
    unsigned char firstchar;
    int bits;                   /* Number of unread bits in "datum" */
    int datum;                /* 32-bit input buffer */

    /* Output state machine */
    int ipass;                  /* Interlace pass; Ranges 1-4 if interlaced. */
    unsigned int rows_remaining;        /* Rows remaining to be output */
    unsigned int irow;                  /* Current output row, starting at zero */
    unsigned char *rowbuf;              /* Single scanline temporary buffer */
    unsigned char *rowend;              /* Pointer to end of rowbuf */
    unsigned char *rowp;                /* Current output pointer */

    /* Parameters for image frame currently being decoded */
    unsigned int x_offset, y_offset;    /* With respect to "screen" origin */
    unsigned int height, width;
    int tpixel;                 /* Index of transparent pixel */
    WebCore::RGBA32Buffer::FrameDisposalMethod disposal_method;   /* Restore to background, leave in place, etc.*/
    unsigned char *local_colormap;    /* Per-image colormap */
    int local_colormap_size;    /* Size of local colormap array. */
    
    bool is_local_colormap_defined : 1;
    bool progressive_display : 1;    /* If TRUE, do Haeberli interlace hack */
    bool interlaced : 1;             /* TRUE, if scanlines arrive interlaced order */
    bool is_transparent : 1;         /* TRUE, if tpixel is valid */

    unsigned delay_time;        /* Display time, in milliseconds,
                                   for this image in a multi-image GIF */


    unsigned short*  prefix;          /* LZW decoding tables */
    unsigned char*   suffix;          /* LZW decoding tables */
    unsigned char*   stack;           /* Base of LZW decoder stack */


    GIFFrameReader() {
        stackp = 0;
        datasize = codesize = codemask = clear_code = avail = oldcode = 0;
        firstchar = 0;
        bits = datum = 0;
        ipass = 0;
        rows_remaining = irow = 0;
        rowbuf = rowend = rowp = 0;

        x_offset = y_offset = width = height = 0;
        tpixel = 0;
        disposal_method = WebCore::RGBA32Buffer::DisposeNotSpecified;

        local_colormap = 0;
        local_colormap_size = 0;
        is_local_colormap_defined = progressive_display = is_transparent = interlaced = false;

        delay_time = 0;

        prefix = 0;
        suffix = stack = 0;
    }
    
    ~GIFFrameReader() {
        delete []rowbuf;
        delete []local_colormap;
        delete []prefix;
        delete []suffix;
        delete []stack;
    }
};

struct GIFImageReader {
    WebCore::GIFImageDecoder* clientptr;
    /* Parsing state machine */
    gstate state;                      /* Current decoder master state */
    unsigned bytes_to_consume;         /* Number of bytes to accumulate */
    unsigned bytes_in_hold;            /* bytes accumulated so far*/
    unsigned char hold[MAX_HOLD_SIZE]; /* Accumulation buffer */
    unsigned char* global_colormap;    /* (3* MAX_COLORS in size) Default colormap if local not supplied, 3 bytes for each color  */
    
     /* Global (multi-image) state */
    int screen_bgcolor;         /* Logical screen background color */
    int version;                /* Either 89 for GIF89 or 87 for GIF87 */
    unsigned screen_width;       /* Logical screen width & height */
    unsigned screen_height;
    int global_colormap_size;    /* Size of global colormap array. */
    unsigned int images_decoded; /* Counts completed frames for animated GIFs */
    int images_count;            /* Counted all frames seen so far (including incomplete frames) */
    int loop_count;              /* Netscape specific extension block to control
                                    the number of animation loops a GIF renders. */
    
    // Not really global, but convenient to locate here.
    int count;                  /* Remaining # bytes in sub-block */
    
    GIFFrameReader* frame_reader;

    GIFImageReader(WebCore::GIFImageDecoder* client = 0) {
        clientptr = client;
        state = gif_type;
        bytes_to_consume = 6;
        bytes_in_hold = 0;
        frame_reader = 0;
        global_colormap = 0;

        screen_bgcolor = version = 0;
        screen_width = screen_height = 0;
        global_colormap_size = images_decoded = images_count = 0;
        loop_count = cLoopCountNotSeen;
        count = 0;
    }

    ~GIFImageReader() {
        close();
    }

    void close() {
        delete []global_colormap;
        global_colormap = 0;
        delete frame_reader;
        frame_reader = 0;
    }

    bool read(const unsigned char * buf, unsigned int numbytes, 
              WebCore::GIFImageDecoder::GIFQuery query = WebCore::GIFImageDecoder::GIFFullQuery, unsigned haltAtFrame = -1);

private:
    void output_row();
    int do_lzw(const unsigned char *q);
};

#endif
