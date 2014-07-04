/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef ContextMenuContextData_h
#define ContextMenuContextData_h

#if ENABLE(CONTEXT_MENUS)

#include "ShareableBitmap.h"
#include "WebHitTestResult.h"

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebCore {
class ContextMenuContext;
}

namespace WebKit {

enum TelephoneNumberContextTag { TelephoneNumberContext };

class ContextMenuContextData {
public:
    ContextMenuContextData();
    ContextMenuContextData(TelephoneNumberContextTag);
    ContextMenuContextData(const WebCore::ContextMenuContext&);

    ContextMenuContextData(const ContextMenuContextData&);
    ContextMenuContextData& operator=(const ContextMenuContextData&);
    
    const WebHitTestResult::Data& webHitTestResultData() const { return m_webHitTestResultData; }

#if ENABLE(SERVICE_CONTROLS)
    ContextMenuContextData(const Vector<uint8_t>& selectionData, const Vector<String>& selectedTelephoneNumbers, bool isEditable)
        : m_isTelephoneNumberContext(false)
        , m_controlledSelectionData(selectionData)
        , m_selectedTelephoneNumbers(selectedTelephoneNumbers)
        , m_selectionIsEditable(isEditable)
    { }

    const ShareableBitmap::Handle& controlledImageHandle() const { return m_controlledImageHandle; }
    const Vector<uint8_t>& controlledSelectionData() const { return m_controlledSelectionData; }
    const Vector<String>& selectedTelephoneNumbers() const { return m_selectedTelephoneNumbers; }

    bool controlledDataIsEditable() const;
    bool needsServicesMenu() const { return !m_controlledImageHandle.isNull() || !m_controlledSelectionData.isEmpty(); }
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    bool isTelephoneNumberContext() const { return m_isTelephoneNumberContext; }
#endif

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, ContextMenuContextData&);

private:

    WebHitTestResult::Data m_webHitTestResultData;
    bool m_isTelephoneNumberContext;

#if ENABLE(SERVICE_CONTROLS)
    ShareableBitmap::Handle m_controlledImageHandle;
    Vector<uint8_t> m_controlledSelectionData;
    Vector<String> m_selectedTelephoneNumbers;
    bool m_selectionIsEditable;
#endif
};

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
#endif // ContextMenuContextData_h
