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

class FileInputType final : public BaseClickableWithKeyInputType, private FileChooserClient, private FileIconLoaderClient {
public:
    explicit FileInputType(HTMLInputElement&);
    virtual ~FileInputType();

    static Vector<FileChooserFileInfo> filesFromFormControlState(const FormControlState&);

private:
    const AtomicString& formControlType() const override;
    FormControlState saveFormControlState() const override;
    void restoreFormControlState(const FormControlState&) override;
    bool appendFormData(FormDataList&, bool) const override;
    bool valueMissing(const String&) const override;
    String valueMissingText() const override;
    void handleDOMActivateEvent(Event&) override;
    RenderPtr<RenderElement> createInputRenderer(RenderStyle&&) override;
    bool canSetStringValue() const override;
    bool canChangeFromAnotherType() const override;
    FileList* files() override;
    void setFiles(PassRefPtr<FileList>) override;
#if PLATFORM(IOS)
    String displayString() const override;
#endif
    bool canSetValue(const String&) override;
    bool getTypeSpecificValue(String&) override; // Checked first, before internal storage or the value attribute.
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;

#if ENABLE(DRAG_SUPPORT)
    bool receiveDroppedFiles(const DragData&) override;
#endif

    Icon* icon() const override;
    bool isFileUpload() const override;
    void createShadowSubtree() override;
    void disabledAttributeChanged() override;
    void multipleAttributeChanged() override;
    String defaultToolTip() const override;

    // FileChooserClient implementation.
    void filesChosen(const Vector<FileChooserFileInfo>&) override;
#if PLATFORM(IOS)
    void filesChosen(const Vector<FileChooserFileInfo>&, const String& displayString, Icon*) override;
#endif

    // FileIconLoaderClient implementation.
    void updateRendering(PassRefPtr<Icon>) override;

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
