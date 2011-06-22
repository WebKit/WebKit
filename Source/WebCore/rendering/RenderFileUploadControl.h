/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
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
#include "FileIconLoader.h"
#include "RenderBlock.h"

namespace WebCore {

class Chrome;
class HTMLInputElement;

// Each RenderFileUploadControl contains a RenderButton (for opening the file chooser), and
// sufficient space to draw a file icon and filename. The RenderButton has a shadow node
// associated with it to receive click/hover events.

class RenderFileUploadControl : public RenderBlock, private FileChooserClient, private FileIconLoaderClient {
public:
    RenderFileUploadControl(HTMLInputElement*);
    virtual ~RenderFileUploadControl();

    virtual bool isFileUploadControl() const { return true; }

    void click();

    void receiveDroppedFiles(const Vector<String>&);

    String buttonValue();
    String fileTextValue() const;
    
private:
    virtual const char* renderName() const { return "RenderFileUploadControl"; }

    virtual void updateFromElement();
    virtual void computePreferredLogicalWidths();
    virtual void paintObject(PaintInfo&, const IntPoint&);

    virtual bool requiresForcedStyleRecalcPropagation() const { return true; }

    // FileChooserClient implementation.
    virtual void filesChosen(const Vector<String>&);

    // FileIconLoaderClient implementation.
    virtual void updateRendering(PassRefPtr<Icon>);

#if ENABLE(DIRECTORY_UPLOAD)
    void receiveDropForDirectoryUpload(const Vector<String>&);
#endif

    Chrome* chrome() const;
    int maxFilenameWidth() const;
    
    virtual VisiblePosition positionForPoint(const IntPoint&);

    HTMLInputElement* uploadButton() const;
    void requestIcon(const Vector<String>&) const;

    RefPtr<FileIconLoader> m_iconLoader;
    RefPtr<Icon> m_icon;
};

inline RenderFileUploadControl* toRenderFileUploadControl(RenderObject* object)
{
    ASSERT(!object || object->isFileUploadControl());
    return static_cast<RenderFileUploadControl*>(object);
}

inline const RenderFileUploadControl* toRenderFileUploadControl(const RenderObject* object)
{
    ASSERT(!object || object->isFileUploadControl());
    return static_cast<const RenderFileUploadControl*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderFileUploadControl(const RenderFileUploadControl*);

} // namespace WebCore

#endif // RenderFileUploadControl_h
