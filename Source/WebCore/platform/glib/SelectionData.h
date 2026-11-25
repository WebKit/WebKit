/*
 * Copyright (C) 2009, Martin Robinson
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "ShareableBitmap.h"
#include "SharedBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class SelectionData {
    WTF_MAKE_TZONE_ALLOCATED(SelectionData);
public:
    void setText(StringImpl*);
    const RefPtr<StringImpl>& text() const LIFETIME_BOUND { return m_text; }
    bool hasText() const { return !isEmpty(m_text); }
    void clearText() { m_text = emptyString(); }

    void setMarkup(StringImpl* newMarkup) { m_markup = newMarkup; }
    const RefPtr<StringImpl>& markup() const LIFETIME_BOUND { return m_markup; }
    bool hasMarkup() const { return !isEmpty(m_markup); }
    void clearMarkup() { m_markup = emptyString(); }

    void setURL(const URL&, StringImpl*);
    const URL& url() const LIFETIME_BOUND { return m_url; }
    const RefPtr<StringImpl>& urlLabel() const LIFETIME_BOUND;
    bool hasURL() const { return !m_url.isEmpty() && m_url.isValid(); }
    void clearURL() { m_url = URL(); }

    void setURIList(StringImpl*);
    const RefPtr<StringImpl>& uriList() const LIFETIME_BOUND { return m_uriList; }
    const Vector<RefPtr<StringImpl>>& filenames() const LIFETIME_BOUND { return m_filenames; }
    bool hasURIList() const { return !isEmpty(m_uriList); }
    bool hasFilenames() const { return !m_filenames.isEmpty(); }
    void clearURIList() { m_uriList = emptyString(); }

    void setImage(RefPtr<ShareableBitmap>&& newImage) { m_image = WTF::move(newImage); }
    const RefPtr<ShareableBitmap>& image() const { return m_image; }
    bool hasImage() const { return !!m_image; }
    void clearImage() { m_image = nullptr; }

    void setCanSmartReplace(bool canSmartReplace) { m_canSmartReplace = canSmartReplace; }
    bool canSmartReplace() const { return m_canSmartReplace; }

    void addBuffer(StringImpl* type, const Ref<SharedBuffer>& buffer) { m_buffers.add(type, buffer.get()); }
    const HashMap<RefPtr<StringImpl>, Ref<SharedBuffer>>& buffers() const LIFETIME_BOUND { return m_buffers; }
    SharedBuffer* buffer(StringImpl* type) { return m_buffers.get(type); }
    void clearBuffers() { m_buffers.clear(); }

    void setCustomData(Ref<SharedBuffer>&& buffer) { m_customData = WTF::move(buffer); }
    const RefPtr<SharedBuffer>& customData() const { return m_customData; }
    bool hasCustomData() const { return !!m_customData; }
    void clearCustomData() { m_customData = nullptr; }

    void clearAll();
    void clearAllExceptFilenames();

    SelectionData(StringImpl* text, StringImpl* markup, const URL&, StringImpl* uriList, RefPtr<ShareableBitmap>&&, RefPtr<WebCore::SharedBuffer>&&, bool);
    SelectionData() = default;

private:
    RefPtr<StringImpl> m_text;
    RefPtr<StringImpl> m_markup;
    URL m_url;
    RefPtr<StringImpl> m_uriList;
    Vector<RefPtr<StringImpl>> m_filenames;
    RefPtr<ShareableBitmap> m_image;
    bool m_canSmartReplace { false };
    RefPtr<SharedBuffer> m_customData;
    HashMap<RefPtr<StringImpl>, Ref<SharedBuffer>> m_buffers;
};

} // namespace WebCore
