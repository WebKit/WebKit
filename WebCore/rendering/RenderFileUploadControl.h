/*
 * Copyright (C) 2006, 2007 Apple Inc.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderFileUploadControl_h
#define RenderFileUploadControl_h

#include "FileChooser.h"
#include "RenderBlock.h"

namespace WebCore {

class HTMLInputElement;
    
// Each RenderFileUploadControl contains a RenderButton (for opening the file chooser), and
// sufficient space to draw a file icon and filename. The RenderButton has a shadow node
// associated with it to receive click/hover events.

class RenderFileUploadControl : public RenderBlock, private FileChooserClient {
public:
    RenderFileUploadControl(HTMLInputElement*);
    ~RenderFileUploadControl();

    virtual const char* renderName() const { return "RenderFileUploadControl"; }

    virtual void updateFromElement();
    virtual void calcPrefWidths();
    virtual void paintObject(PaintInfo&, int tx, int ty);

    void click();

    void valueChanged();
    
    void receiveDroppedFiles(const Vector<String>&);

    String buttonValue();
    String fileTextValue();
    
    bool allowsMultipleFiles();

protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

private:
    int maxFilenameWidth() const;
    PassRefPtr<RenderStyle> createButtonStyle(const RenderStyle* parentStyle) const;

    RefPtr<HTMLInputElement> m_button;
    RefPtr<FileChooser> m_fileChooser;
};

} // namespace WebCore

#endif // RenderFileUploadControl_h
