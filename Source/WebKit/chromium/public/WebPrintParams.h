/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebPrintParams_h
#define WebPrintParams_h

#include "WebPrintScalingOption.h"
#include <public/WebRect.h>
#include <public/WebSize.h>

namespace WebKit {

struct WebPrintParams {
    // Specifies printable content rect in points (a point is 1/72 of an inch).
    WebRect printContentArea;

    // Specifies the selected printer default printable area details in
    // points.
    WebRect printableArea;

    // Specifies the selected printer default paper size in points.
    WebSize paperSize;

    // Specifies user selected DPI for printing.
    int printerDPI;

    // Specifies whether to reduce/enlarge/retain the print contents to fit the
    // printable area. (This is used only by plugin printing).
    WebPrintScalingOption printScalingOption;

    WebPrintParams()
        : printerDPI(72)
        , printScalingOption(WebPrintScalingOptionFitToPrintableArea) { }

    WebPrintParams(const WebSize& paperSize)
        : printContentArea(WebRect(0, 0, paperSize.width, paperSize.height))
        , printableArea(WebRect(0, 0, paperSize.width, paperSize.height))
        , paperSize(paperSize)
        , printerDPI(72)
        , printScalingOption(WebPrintScalingOptionSourceSize) { }

    WebPrintParams(const WebRect& printContentArea, const WebRect& printableArea, const WebSize& paperSize, int printerDPI, WebPrintScalingOption printScalingOption)
        : printContentArea(printContentArea)
        , printableArea(printableArea)
        , paperSize(paperSize)
        , printerDPI(printerDPI)
        , printScalingOption(printScalingOption) { }
};

} // namespace WebKit

#endif
