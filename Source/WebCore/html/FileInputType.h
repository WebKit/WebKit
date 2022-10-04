/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "BaseClickableWithKeyInputType.h"
#include "FileChooser.h"
#include "FileIconLoader.h"
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DirectoryFileListCreator;
class DragData;
class FileList;
class Icon;

class FileInputType final : public BaseClickableWithKeyInputType, private FileChooserClient, private FileIconLoaderClient, public CanMakeWeakPtr<FileInputType> {
    template<typename DowncastedType> friend bool isInvalidInputType(const InputType&, const String&);
public:
    explicit FileInputType(HTMLInputElement&);
    virtual ~FileInputType();

    String firstElementPathForInputValue() const; // Checked first, before internal storage or the value attribute.
    FileList& files() { return m_fileList; }
    void setFiles(RefPtr<FileList>&&, WasSetByJavaScript);

    static std::pair<Vector<FileChooserFileInfo>, String> filesFromFormControlState(const FormControlState&);
    bool canSetStringValue() const final;
    bool valueMissing(const String&) const final;

private:
    const AtomString& formControlType() const final;
    FormControlState saveFormControlState() const final;
    void restoreFormControlState(const FormControlState&) final;
    bool appendFormData(DOMFormData&) const final;
    String valueMissingText() const final;
    void handleDOMActivateEvent(Event&) final;
    RenderPtr<RenderElement> createInputRenderer(RenderStyle&&) final;
    enum class RequestIcon { Yes, No };
    void setFiles(RefPtr<FileList>&&, RequestIcon, WasSetByJavaScript);
    String displayString() const final;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior, TextControlSetValueSelection) final;
    void showPicker() final;
    bool allowsShowPickerAcrossFrames() final;

#if ENABLE(DRAG_SUPPORT)
    bool receiveDroppedFilesWithImageTranscoding(const Vector<String>& paths);
    bool receiveDroppedFiles(const DragData&) final;
#endif

    Icon* icon() const final;
    void createShadowSubtree() final;
    void disabledStateChanged() final;
    void attributeChanged(const QualifiedName&) final;
    String defaultToolTip() const final;

    void filesChosen(const Vector<FileChooserFileInfo>&, const String& displayString = { }, Icon* = nullptr) final;
    void filesChosen(const Vector<String>& paths, const Vector<String>& replacementPaths = { });

    // FileIconLoaderClient implementation.
    void iconLoaded(RefPtr<Icon>&&) final;

    FileChooserSettings fileChooserSettings() const;
    void applyFileChooserSettings();
    void didCreateFileList(Ref<FileList>&&, RefPtr<Icon>&&);
    void requestIcon(const Vector<String>&);

    bool allowsDirectories() const;

    RefPtr<FileChooser> m_fileChooser;
    std::unique_ptr<FileIconLoader> m_fileIconLoader;

    Ref<FileList> m_fileList;
    RefPtr<DirectoryFileListCreator> m_directoryFileListCreator;
    RefPtr<Icon> m_icon;
    String m_displayString;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INPUT_TYPE(FileInputType, Type::File)
