/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 */

#ifndef StyleSheet_H
#define StyleSheet_H

#include "StyleList.h"
#include "PlatformString.h"

namespace WebCore {

class Node;
class CachedCSSStyleSheet;
class MediaList;

class StyleSheet : public StyleList {
public:
    StyleSheet(Node* ownerNode, String href = String());
    StyleSheet(StyleSheet* parentSheet, String href = String());
    StyleSheet(StyleBase* owner, String href = String());
    StyleSheet(CachedCSSStyleSheet*, String href = String());
    virtual ~StyleSheet();

    virtual bool isStyleSheet() const { return true; }

    virtual String type() const { return String(); }

    bool disabled() const { return m_disabled; }
    void setDisabled(bool disabled) { m_disabled = disabled; styleSheetChanged(); }

    Node* ownerNode() const { return m_parentNode; }
    StyleSheet *parentStyleSheet() const;
    String href() const { return m_strHref; }
    String title() const { return m_strTitle; }
    MediaList* media() const { return m_media.get(); }
    void setMedia(MediaList*);

    virtual bool isLoading() { return false; }

    virtual void styleSheetChanged() { }
    
protected:
    Node* m_parentNode;
    String m_strHref;
    String m_strTitle;
    RefPtr<MediaList> m_media;
    bool m_disabled;
};

} // namespace

#endif
