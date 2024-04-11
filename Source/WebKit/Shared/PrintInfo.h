/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/LengthBox.h>

#if USE(APPKIT)
OBJC_CLASS NSPrintInfo;
#elif PLATFORM(GTK)
#include <wtf/glib/GRefPtr.h>

typedef struct _GtkPageSetup GtkPageSetup;
typedef struct _GtkPrintJob GtkPrintJob;
typedef struct _GtkPrintSettings GtkPrintSettings;
#else
// FIXME: This should use the windows equivalent.
class NSPrintInfo;
#endif

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct PrintInfo {
    PrintInfo() = default;
#if PLATFORM(GTK)
    enum class PrintMode : uint8_t {
        Async,
        Sync
    };

#if HAVE(GTK_UNIX_PRINTING)
    explicit PrintInfo(GtkPrintJob*, PrintMode = PrintMode::Async);
#endif
#else
    explicit PrintInfo(NSPrintInfo *);
#endif
    PrintInfo(float pageSetupScaleFactor, float availablePaperWidth, float availablePaperHeight, WebCore::FloatBoxExtent margin
#if PLATFORM(IOS_FAMILY)
        , bool snapshotFirstPage
#endif
#if PLATFORM(GTK)
        , GRefPtr<GtkPrintSettings>&&, GRefPtr<GtkPageSetup>&&, PrintMode
#endif
        );


    // These values are in 'point' unit (and not CSS pixel).
    float pageSetupScaleFactor { 0 };
    float availablePaperWidth { 0 };
    float availablePaperHeight { 0 };
    WebCore::FloatBoxExtent margin;
#if PLATFORM(IOS_FAMILY)
    bool snapshotFirstPage { false };
#endif

#if PLATFORM(GTK)
    GRefPtr<GtkPrintSettings> printSettings;
    GRefPtr<GtkPageSetup> pageSetup;
    PrintMode printMode { PrintMode::Async };
#endif
};

} // namespace WebKit
