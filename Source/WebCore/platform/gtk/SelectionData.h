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

#include "Image.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class SelectionData : public RefCounted<SelectionData> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<SelectionData> create()
    {
        return adoptRef(*new SelectionData);
    }

    void setText(const String&);
    const String& text() const { return m_text; }
    bool hasText() const { return !m_text.isEmpty(); }
    void clearText() { m_text = emptyString(); }

    void setMarkup(const String& newMarkup) { m_markup = newMarkup; }
    const String& markup() const { return m_markup; }
    bool hasMarkup() const { return !m_markup.isEmpty(); }
    void clearMarkup() { m_markup = emptyString(); }

    void setURL(const URL&, const String&);
    const URL& url() const { return m_url; }
    const String& urlLabel() const;
    bool hasURL() const { return !m_url.isEmpty() && m_url.isValid(); }
    void clearURL() { m_url = URL(); }

    void setURIList(const String&);
    const String& uriList() const { return m_uriList; }
    const Vector<String>& filenames() const { return m_filenames; }
    bool hasURIList() const { return !m_uriList.isEmpty(); }
    bool hasFilenames() const { return !m_filenames.isEmpty(); }
    void clearURIList() { m_uriList = emptyString(); }

    void setImage(Image* newImage) { m_image = newImage; }
    Image* image() const { return m_image.get(); }
    bool hasImage() const { return m_image; }
    void clearImage() { m_image = nullptr; }

    void setUnknownTypeData(const String& type, const String& data) { m_unknownTypeData.set(type, data); }
    String unknownTypeData(const String& type) const { return m_unknownTypeData.get(type); }
    const HashMap<String, String>& unknownTypes() const { return m_unknownTypeData; }
    bool hasUnknownTypeData() const { return !m_unknownTypeData.isEmpty(); }

    void setCanSmartReplace(bool canSmartReplace) { m_canSmartReplace = canSmartReplace; }
    bool canSmartReplace() const { return m_canSmartReplace; }

    void clearAll();
    void clearAllExceptFilenames();

private:
    String m_text;
    String m_markup;
    URL m_url;
    String m_uriList;
    Vector<String> m_filenames;
    RefPtr<Image> m_image;
    HashMap<String, String> m_unknownTypeData;
    bool m_canSmartReplace { false };
};

} // namespace WebCore
