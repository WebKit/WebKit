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

#include "BaseClickableWithKeyInputType.h"
#include "FileChooser.h"
#include "FileIconLoader.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class DragData;
class FileList;
class Icon;

class FileInputType : public BaseClickableWithKeyInputType, private FileChooserClient, private FileIconLoaderClient {
public:
    explicit FileInputType(HTMLInputElement&);
    virtual ~FileInputType();

    static Vector<FileChooserFileInfo> filesFromFormControlState(const FormControlState&);

private:
    virtual const AtomicString& formControlType() const override;
    virtual FormControlState saveFormControlState() const override;
    virtual void restoreFormControlState(const FormControlState&) override;
    virtual bool appendFormData(FormDataList&, bool) const override;
    virtual bool valueMissing(const String&) const override;
    virtual String valueMissingText() const override;
    virtual void handleDOMActivateEvent(Event*) override;
    virtual RenderPtr<RenderElement> createInputRenderer(PassRef<RenderStyle>) override;
    virtual bool canSetStringValue() const override;
    virtual bool canChangeFromAnotherType() const override;
    virtual FileList* files() override;
    virtual void setFiles(PassRefPtr<FileList>) override;
#if PLATFORM(IOS)
    virtual String displayString() const override;
#endif
    virtual bool canSetValue(const String&) override;
    virtual bool getTypeSpecificValue(String&) override; // Checked first, before internal storage or the value attribute.
    virtual void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;

#if ENABLE(DRAG_SUPPORT)
    virtual bool receiveDroppedFiles(const DragData&) override;
#endif

    virtual Icon* icon() const override;
    virtual bool isFileUpload() const override;
    virtual void createShadowSubtree() override;
    virtual void disabledAttributeChanged() override;
    virtual void multipleAttributeChanged() override;
    virtual String defaultToolTip() const override;

    // FileChooserClient implementation.
    virtual void filesChosen(const Vector<FileChooserFileInfo>&) override;
#if PLATFORM(IOS)
    virtual void filesChosen(const Vector<FileChooserFileInfo>&, const String& displayString, Icon*) override;
#endif

    // FileIconLoaderClient implementation.
    virtual void updateRendering(PassRefPtr<Icon>) override;

    PassRefPtr<FileList> createFileList(const Vector<FileChooserFileInfo>& files) const;
    void requestIcon(const Vector<String>&);

    void applyFileChooserSettings(const FileChooserSettings&);

    RefPtr<FileChooser> m_fileChooser;
#if !PLATFORM(IOS)
    std::unique_ptr<FileIconLoader> m_fileIconLoader;
#endif

    RefPtr<FileList> m_fileList;
    RefPtr<Icon> m_icon;
#if PLATFORM(IOS)
    String m_displayString;
#endif
};

} // namespace WebCore

#endif // FileInputType_h
