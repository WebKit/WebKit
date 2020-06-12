/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CONTEXT_MENUS)

#include "ShareableBitmap.h"
#include "WebContextMenuItemData.h"
#include "WebHitTestResultData.h"
#include <wtf/EnumTraits.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class ContextMenuContext;
}

namespace WebKit {

class ContextMenuContextData {
public:
    enum class Type : bool {
        ContextMenu,
        ServicesMenu,
    };

    ContextMenuContextData();
    ContextMenuContextData(const WebCore::IntPoint& menuLocation, const Vector<WebKit::WebContextMenuItemData>& menuItems, const WebCore::ContextMenuContext&);

    Type type() const { return m_type; }
    const WebCore::IntPoint& menuLocation() const { return m_menuLocation; }
    const Vector<WebKit::WebContextMenuItemData>& menuItems() const { return m_menuItems; }

    const WebHitTestResultData& webHitTestResultData() const { return m_webHitTestResultData; }
    const String& selectedText() const { return m_selectedText; }

#if ENABLE(SERVICE_CONTROLS)
    ContextMenuContextData(const WebCore::IntPoint& menuLocation, const Vector<uint8_t>& selectionData, const Vector<String>& selectedTelephoneNumbers, bool isEditable)
        : m_type(Type::ServicesMenu)
        , m_menuLocation(menuLocation)
        , m_controlledSelectionData(selectionData)
        , m_selectedTelephoneNumbers(selectedTelephoneNumbers)
        , m_selectionIsEditable(isEditable)
    {
    }

    ShareableBitmap* controlledImage() const { return m_controlledImage.get(); }
    const Vector<uint8_t>& controlledSelectionData() const { return m_controlledSelectionData; }
    const Vector<String>& selectedTelephoneNumbers() const { return m_selectedTelephoneNumbers; }

    bool isServicesMenu() const { return m_type == ContextMenuContextData::Type::ServicesMenu; }
    bool controlledDataIsEditable() const;
#endif

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, ContextMenuContextData&);

private:
    Type m_type;

    WebCore::IntPoint m_menuLocation;
    Vector<WebKit::WebContextMenuItemData> m_menuItems;

    WebHitTestResultData m_webHitTestResultData;
    String m_selectedText;

#if ENABLE(SERVICE_CONTROLS)
    RefPtr<ShareableBitmap> m_controlledImage;
    Vector<uint8_t> m_controlledSelectionData;
    Vector<String> m_selectedTelephoneNumbers;
    bool m_selectionIsEditable;
#endif
};

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
