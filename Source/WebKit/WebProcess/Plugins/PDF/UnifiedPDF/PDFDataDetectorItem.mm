/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PDFDataDetectorItem.h"

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)

#include "PDFKitSPI.h"
#include <pal/spi/cocoa/DataDetectorsCoreSPI.h>
#include <pal/spi/mac/DataDetectorsSPI.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/cf/TypeCastsCF.h>

#include "PDFKitSoftlink.h"
#include <pal/cocoa/DataDetectorsCoreSoftLink.h>
#include <pal/mac/DataDetectorsSoftLink.h>

namespace WebKit {

static bool hasActionsForResult(DDScannerResult *dataDetectorResult)
{
    RetainPtr ddActionsManagerClass = PAL::getDDActionsManagerClass();
    return [[ddActionsManagerClass.get() sharedManager] hasActionsForResult:RetainPtr { [dataDetectorResult coreResult] }.get() actionContext:nil];
}

static bool resultIsPastDate(DDScannerResult *dataDetectorResult, PDFPage *pdfPage)
{
    RetainPtr referenceDate = [[[pdfPage document] documentAttributes] objectForKey:RetainPtr { get_PDFKit_PDFDocumentCreationDateAttribute() }.get()];
    RetainPtr referenceTimeZone = adoptCF(CFTimeZoneCopyDefault());
    return PAL::softLink_DataDetectorsCore_DDResultIsPastDate(RetainPtr { [dataDetectorResult coreResult] }.get(), (CFDateRef)referenceDate.get(), (CFTimeZoneRef)referenceTimeZone.get());
}

Ref<PDFDataDetectorItem> PDFDataDetectorItem::create(DDScannerResult *dataDetectorResult, PDFPage *pdfPage)
{
    return adoptRef(*new PDFDataDetectorItem(dataDetectorResult, pdfPage));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFDataDetectorItem);

PDFDataDetectorItem::PDFDataDetectorItem(DDScannerResult *dataDetectorResult, PDFPage *pdfPage)
    : m_result { dataDetectorResult }
    , m_page { pdfPage }
    , m_hasActions { hasActionsForResult(dataDetectorResult) }
    , m_isPastDate { resultIsPastDate(dataDetectorResult, pdfPage) }
{
}

DDScannerResult *PDFDataDetectorItem::scannerResult() const
{
    return m_result.get();
}

bool PDFDataDetectorItem::hasActions() const
{
    return m_hasActions;
}

bool PDFDataDetectorItem::isPastDate() const
{
    return m_isPastDate;
}

RetainPtr<PDFSelection> PDFDataDetectorItem::selection() const
{
    return [m_page selectionForRange:[m_result urlificationRange]];
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF_DATA_DETECTION)
