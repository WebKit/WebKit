/*
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

#ifndef FileChooser_h
#define FileChooser_h

#include "Icon.h"
#include "PlatformString.h"
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class OpenPanelController;
#else
class OpenPanelController;
#endif
#endif

namespace WebCore {

class Document;
class RenderFileUploadControl;
class String;

class FileChooser : public Shared<FileChooser> {
public:
    static PassRefPtr<FileChooser> create(Document*, RenderFileUploadControl*);
    ~FileChooser();
    
    void openFileChooser();
    
    const String& filename() const { return m_filename; }
    String basenameForWidth(int width) const;
    
    Icon* icon() const { return m_icon.get(); }
    RenderFileUploadControl* uploadControl() const { return m_uploadControl; }
    Document* document() { return m_document; }
    
    void disconnectUploadControl();

    void chooseFile(const String& filename);
    
private:
    Document* m_document;
    String m_filename;
    RefPtr<Icon> m_icon;
    RenderFileUploadControl* m_uploadControl;

    FileChooser(Document*, RenderFileUploadControl*);
    
#if PLATFORM(MAC)
    OpenPanelController* m_controller;
#endif
};

}

#endif
