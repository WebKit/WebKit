/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "FileInputType.h"

#include "Chrome.h"
#include "DragData.h"
#include "Event.h"
#include "File.h"
#include "FileList.h"
#include "FileSystem.h"
#include "FormController.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "InputTypeNames.h"
#include "LocalizedStrings.h"
#include "RenderFileUploadControl.h"
#include "ScriptController.h"
#include "ShadowRoot.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

class UploadButtonElement final : public HTMLInputElement {
public:
    static PassRefPtr<UploadButtonElement> create(Document&);
    static PassRefPtr<UploadButtonElement> createForMultiple(Document&);

private:
    UploadButtonElement(Document&);
};

PassRefPtr<UploadButtonElement> UploadButtonElement::create(Document& document)
{
    RefPtr<UploadButtonElement> button = adoptRef(new UploadButtonElement(document));
    button->setValue(fileButtonChooseFileLabel());
    return button.release();
}

PassRefPtr<UploadButtonElement> UploadButtonElement::createForMultiple(Document& document)
{
    RefPtr<UploadButtonElement> button = adoptRef(new UploadButtonElement(document));
    button->setValue(fileButtonChooseMultipleFilesLabel());
    return button.release();
}

UploadButtonElement::UploadButtonElement(Document& document)
    : HTMLInputElement(inputTag, document, 0, false)
{
    setType(AtomicString("button", AtomicString::ConstructFromLiteral));
    setPseudo(AtomicString("-webkit-file-upload-button", AtomicString::ConstructFromLiteral));
}

FileInputType::FileInputType(HTMLInputElement& element)
    : BaseClickableWithKeyInputType(element)
    , m_fileList(FileList::create())
{
}

FileInputType::~FileInputType()
{
    if (m_fileChooser)
        m_fileChooser->invalidate();

#if !PLATFORM(IOS)
    // FIXME: Is this correct? Why don't we do this on iOS?
    if (m_fileIconLoader)
        m_fileIconLoader->invalidate();
#endif
}

Vector<FileChooserFileInfo> FileInputType::filesFromFormControlState(const FormControlState& state)
{
    Vector<FileChooserFileInfo> files;
    for (size_t i = 0; i < state.valueSize(); i += 2) {
        if (!state[i + 1].isEmpty())
            files.append(FileChooserFileInfo(state[i], state[i + 1]));
        else
            files.append(FileChooserFileInfo(state[i]));
    }
    return files;
}

const AtomicString& FileInputType::formControlType() const
{
    return InputTypeNames::file();
}

FormControlState FileInputType::saveFormControlState() const
{
    if (m_fileList->isEmpty())
        return FormControlState();
    FormControlState state;
    unsigned numFiles = m_fileList->length();
    for (unsigned i = 0; i < numFiles; ++i) {
        state.append(m_fileList->item(i)->path());
        state.append(m_fileList->item(i)->name());
    }
    return state;
}

void FileInputType::restoreFormControlState(const FormControlState& state)
{
    if (state.valueSize() % 2)
        return;
    filesChosen(filesFromFormControlState(state));
}

bool FileInputType::appendFormData(FormDataList& encoding, bool multipart) const
{
    FileList* fileList = element().files();
    unsigned numFiles = fileList->length();
    if (!multipart) {
        // Send only the basenames.
        // 4.10.16.4 and 4.10.16.6 sections in HTML5.

        // Unlike the multipart case, we have no special handling for the empty
        // fileList because Netscape doesn't support for non-multipart
        // submission of file inputs, and Firefox doesn't add "name=" query
        // parameter.
        for (unsigned i = 0; i < numFiles; ++i)
            encoding.appendData(element().name(), fileList->item(i)->name());
        return true;
    }

    // If no filename at all is entered, return successful but empty.
    // Null would be more logical, but Netscape posts an empty file. Argh.
    if (!numFiles) {
        encoding.appendBlob(element().name(), File::create(""));
        return true;
    }

    for (unsigned i = 0; i < numFiles; ++i)
        encoding.appendBlob(element().name(), fileList->item(i));
    return true;
}

bool FileInputType::valueMissing(const String& value) const
{
    return element().isRequired() && value.isEmpty();
}

String FileInputType::valueMissingText() const
{
    return element().multiple() ? validationMessageValueMissingForMultipleFileText() : validationMessageValueMissingForFileText();
}

void FileInputType::handleDOMActivateEvent(Event* event)
{
    if (element().isDisabledFormControl())
        return;

    if (!ScriptController::processingUserGesture())
        return;

    if (Chrome* chrome = this->chrome()) {
        FileChooserSettings settings;
        HTMLInputElement& input = element();
        settings.allowsMultipleFiles = input.fastHasAttribute(multipleAttr);
        settings.acceptMIMETypes = input.acceptMIMETypes();
        settings.acceptFileExtensions = input.acceptFileExtensions();
        settings.selectedFiles = m_fileList->paths();
#if ENABLE(MEDIA_CAPTURE)
        settings.capture = input.shouldUseMediaCapture();
#endif

        applyFileChooserSettings(settings);
        chrome->runOpenPanel(input.document().frame(), m_fileChooser);
    }

    event->setDefaultHandled();
}

RenderPtr<RenderElement> FileInputType::createInputRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderFileUploadControl>(element(), WTF::move(style));
}

bool FileInputType::canSetStringValue() const
{
    return false;
}

bool FileInputType::canChangeFromAnotherType() const
{
    // Don't allow the type to be changed to file after the first type change.
    // In other engines this might mean a JavaScript programmer could set a text
    // field's value to something like /etc/passwd and then change it to a file input.
    // I don't think this would actually occur in WebKit, but this rule still may be
    // important for compatibility.
    return false;
}

FileList* FileInputType::files()
{
    return m_fileList.get();
}

bool FileInputType::canSetValue(const String& value)
{
    // For security reasons, we don't allow setting the filename, but we do allow clearing it.
    // The HTML5 spec (as of the 10/24/08 working draft) says that the value attribute isn't
    // applicable to the file upload control at all, but for now we are keeping this behavior
    // to avoid breaking existing websites that may be relying on this.
    return value.isEmpty();
}

bool FileInputType::getTypeSpecificValue(String& value)
{
    if (m_fileList->isEmpty()) {
        value = String();
        return true;
    }

    // HTML5 tells us that we're supposed to use this goofy value for
    // file input controls. Historically, browsers revealed the real
    // file path, but that's a privacy problem. Code on the web
    // decided to try to parse the value by looking for backslashes
    // (because that's what Windows file paths use). To be compatible
    // with that code, we make up a fake path for the file.
    value = "C:\\fakepath\\" + m_fileList->item(0)->name();
    return true;
}

void FileInputType::setValue(const String&, bool, TextFieldEventBehavior)
{
    // FIXME: Should we clear the file list, or replace it with a new empty one here? This is observable from JavaScript through custom properties.
    m_fileList->clear();
    m_icon.clear();
    element().setNeedsStyleRecalc();
}

PassRefPtr<FileList> FileInputType::createFileList(const Vector<FileChooserFileInfo>& files) const
{
    Vector<RefPtr<File>> fileObjects;
    for (const FileChooserFileInfo& info : files)
        fileObjects.append(File::createWithName(info.path, info.displayName));

    return FileList::create(WTF::move(fileObjects));
}

bool FileInputType::isFileUpload() const
{
    return true;
}

void FileInputType::createShadowSubtree()
{
    ASSERT(element().shadowRoot());
    element().userAgentShadowRoot()->appendChild(element().multiple() ? UploadButtonElement::createForMultiple(element().document()): UploadButtonElement::create(element().document()), IGNORE_EXCEPTION);
}

void FileInputType::disabledAttributeChanged()
{
    ASSERT(element().shadowRoot());
    UploadButtonElement* button = static_cast<UploadButtonElement*>(element().userAgentShadowRoot()->firstChild());
    if (button)
        button->setBooleanAttribute(disabledAttr, element().isDisabledFormControl());
}

void FileInputType::multipleAttributeChanged()
{
    ASSERT(element().shadowRoot());
    UploadButtonElement* button = static_cast<UploadButtonElement*>(element().userAgentShadowRoot()->firstChild());
    if (button)
        button->setValue(element().multiple() ? fileButtonChooseMultipleFilesLabel() : fileButtonChooseFileLabel());
}

void FileInputType::requestIcon(const Vector<String>& paths)
{
#if PLATFORM(IOS)
    UNUSED_PARAM(paths);
#else
    if (!paths.size())
        return;

    Chrome* chrome = this->chrome();
    if (!chrome)
        return;

    if (m_fileIconLoader)
        m_fileIconLoader->invalidate();

    m_fileIconLoader = std::make_unique<FileIconLoader>(static_cast<FileIconLoaderClient&>(*this));

    chrome->loadIconForFiles(paths, m_fileIconLoader.get());
#endif
}

void FileInputType::applyFileChooserSettings(const FileChooserSettings& settings)
{
    if (m_fileChooser)
        m_fileChooser->invalidate();

    m_fileChooser = FileChooser::create(this, settings);
}

void FileInputType::setFiles(PassRefPtr<FileList> files)
{
    if (!files)
        return;

    Ref<HTMLInputElement> input(element());

    bool pathsChanged = false;
    if (files->length() != m_fileList->length())
        pathsChanged = true;
    else {
        for (unsigned i = 0; i < files->length(); ++i) {
            if (files->item(i)->path() != m_fileList->item(i)->path()) {
                pathsChanged = true;
                break;
            }
        }
    }

    m_fileList = files;

    input->setFormControlValueMatchesRenderer(true);
    input->setNeedsValidityCheck();

    Vector<String> paths;
    for (unsigned i = 0; i < m_fileList->length(); ++i)
        paths.append(m_fileList->item(i)->path());
    requestIcon(paths);

    if (input->renderer())
        input->renderer()->repaint();

    if (pathsChanged) {
        // This call may cause destruction of this instance.
        // input instance is safe since it is ref-counted.
        input->dispatchChangeEvent();
    }
    input->setChangedSinceLastFormControlChangeEvent(false);
}

#if PLATFORM(IOS)
void FileInputType::filesChosen(const Vector<FileChooserFileInfo>& paths, const String& displayString, Icon* icon)
{
    m_displayString = displayString;
    filesChosen(paths);
    updateRendering(icon);
}

String FileInputType::displayString() const
{
    return m_displayString;
}
#endif

void FileInputType::filesChosen(const Vector<FileChooserFileInfo>& files)
{
    setFiles(createFileList(files));
}

void FileInputType::updateRendering(PassRefPtr<Icon> icon)
{
    if (m_icon == icon)
        return;

    m_icon = icon;
    if (element().renderer())
        element().renderer()->repaint();
}

#if ENABLE(DRAG_SUPPORT)
bool FileInputType::receiveDroppedFiles(const DragData& dragData)
{
    Vector<String> paths;
    dragData.asFilenames(paths);
    if (paths.isEmpty())
        return false;

    HTMLInputElement* input = &element();

    Vector<FileChooserFileInfo> files;
    for (unsigned i = 0; i < paths.size(); ++i)
        files.append(FileChooserFileInfo(paths[i]));

    if (input->fastHasAttribute(multipleAttr))
        filesChosen(files);
    else {
        Vector<FileChooserFileInfo> firstFileOnly;
        firstFileOnly.append(files[0]);
        filesChosen(firstFileOnly);
    }
    return true;
}
#endif // ENABLE(DRAG_SUPPORT)

Icon* FileInputType::icon() const
{
    return m_icon.get();
}

String FileInputType::defaultToolTip() const
{
    FileList* fileList = m_fileList.get();
    unsigned listSize = fileList->length();
    if (!listSize) {
        if (element().multiple())
            return fileButtonNoFilesSelectedLabel();
        return fileButtonNoFileSelectedLabel();
    }

    StringBuilder names;
    for (size_t i = 0; i < listSize; ++i) {
        names.append(fileList->item(i)->name());
        if (i != listSize - 1)
            names.append('\n');
    }
    return names.toString();
}


} // namespace WebCore
