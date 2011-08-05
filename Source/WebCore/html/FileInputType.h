/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FileInputType_h
#define FileInputType_h

#include "BaseButtonInputType.h"
#include "FileChooser.h"
#include "FileIconLoader.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class FileList;

class FileInputType : public BaseButtonInputType, private FileChooserClient, private FileIconLoaderClient {
public:
    static PassOwnPtr<InputType> create(HTMLInputElement*);

private:
    FileInputType(HTMLInputElement*);
    virtual const AtomicString& formControlType() const;
    virtual bool appendFormData(FormDataList&, bool) const;
    virtual bool valueMissing(const String&) const;
    virtual String valueMissingText() const;
    virtual void handleDOMActivateEvent(Event*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) const;
    virtual bool canSetStringValue() const;
    virtual bool canChangeFromAnotherType() const;
    virtual FileList* files();
    virtual bool canSetValue(const String&);
    virtual bool getTypeSpecificValue(String&); // Checked first, before internal storage or the value attribute.
    virtual bool storesValueSeparateFromAttribute();
    virtual void receiveDroppedFiles(const Vector<String>&);
    virtual Icon* icon() const;
    virtual bool isFileUpload() const;
    virtual void createShadowSubtree();
    virtual void multipleAttributeChanged();

    // FileChooserClient implementation.
    virtual void filesChosen(const Vector<String>&);

    // FileIconLoaderClient implementation.
    virtual void updateRendering(PassRefPtr<Icon>);

    void setFileList(const Vector<String>& paths);
#if ENABLE(DIRECTORY_UPLOAD)
    void receiveDropForDirectoryUpload(const Vector<String>&);
#endif
    void requestIcon(const Vector<String>&);

    RefPtr<FileList> m_fileList;
    RefPtr<Icon> m_icon;
};

} // namespace WebCore

#endif // FileInputType_h
