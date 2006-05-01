/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

// This file has no guards on purpose in order to detect redundant includes. This is a private header
// and so this should catch anyone trying to include this file in public cpp files.

#include <kxmlcore/Vector.h>

namespace WebCore {

#ifdef __APPLE__
class WebTextRenderer;
#endif

class Font;
class GraphicsContext;
class IntRect;

class FontData;

class FontDataSet : public Shared<FontDataSet>, Noncopyable {
public:
    FontDataSet();
    ~FontDataSet();

    void invalidate();
    
    bool isFixedPitch(const FontDescription& f) const { if (m_pitch == UnknownPitch) determinePitch(f); return m_pitch == FixedPitch; };
    void determinePitch(const FontDescription&) const;

private:
    mutable Pitch m_pitch;
#if __APPLE__
    WebTextRenderer* m_renderer;
    WebTextRenderer* getRenderer(const FontDescription&);
    mutable WebCoreFont m_webCoreFont;
    const WebCoreFont& getWebCoreFont(const FontDescription&) const;
#else
    mutable Vector<FontData*> m_fontSet;
    FontData* primaryFont(const FontDescription& desc) const;
#endif

    friend class Font;
};

}

