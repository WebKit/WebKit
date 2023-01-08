/*
 * Copyright (C) 2007-2008, 2014-2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FTPDirectoryDocument.h"

#if ENABLE(FTPDIR)

#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLDocumentParser.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "Text.h"
#include <wtf/GregorianDateTime.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FTPDirectoryDocument);

using namespace HTMLNames;

FTPDirectoryDocument::FTPDirectoryDocument(Frame* frame, const Settings& settings, const URL& url)
    : HTMLDocument(frame, settings, url, { })
{
#if !LOG_DISABLED
    LogFTP.state = WTFLogChannelState::On;
#endif
}

}

#endif // ENABLE(FTPDIR)
