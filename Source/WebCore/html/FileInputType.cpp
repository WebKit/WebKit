/*
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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
#include "DOMFormData.h"
#include "DragData.h"
#include "ElementChildIterator.h"
#include "Event.h"
#include "File.h"
#include "FileList.h"
#include "FileListCreator.h"
#include "FormController.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "InputTypeNames.h"
#include "LocalizedStrings.h"
#include "RenderFileUploadControl.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "UserGestureIndicator.h"
#include <wtf/FileSystem.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
class UploadButtonElement;
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::UploadButtonElement)
    static bool isType(const WebCore::Element& element) { return element.isUploadButton(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Element>(node) && isType(downcast<WebCore::Element>(node)); }
SPECIALIZE_TYPE_TRAITS_END()

namespace WebCore {

using namespace HTMLNames;

class UploadButtonElement final : public HTMLInputElement {
    WTF_MAKE_ISO_ALLOCATED_INLINE(UploadButtonElement);
public:
    static Ref<UploadButtonElement> create(Document&);
    static Ref<UploadButtonElement> createForMultiple(Document&);

private:
    bool isUploadButton() const override { return true; }
    
    UploadButtonElement(Document&);
};

Ref<UploadButtonElement> UploadButtonElement::create(Document& document)
{
    auto button = adoptRef(*new UploadButtonElement(document));
    button->setValue(fileButtonChooseFileLabel());
    return button;
}

Ref<UploadButtonElement> UploadButtonElement::createForMultiple(Document& document)
{
    auto button = adoptRef(*new UploadButtonElement(document));
    button->setValue(fileButtonChooseMultipleFilesLabel());
    return button;
}

UploadButtonElement::UploadButtonElement(Document& document)
    : HTMLInputElement(inputTag, document, 0, false)
{
    setType(AtomString("button", AtomString::ConstructFromLiteral));
    setPseudo(AtomString("-webkit-file-upload-button", AtomString::ConstructFromLiteral));
}

FileInputType::FileInputType(HTMLInputElement& element)
    : BaseClickableWithKeyInputType(element)
    , m_fileList(FileList::create())
{
}

FileInputType::~FileInputType()
{
    if (m_fileListCreator)
        m_fileListCreator->cancel();

    if (m_fileChooser)
        m_fileChooser->invalidate();

    if (m_fileIconLoader)
        m_fileIconLoader->invalidate();
}

Vector<FileChooserFileInfo> FileInputType::filesFromFormControlState(const FormControlState& state)
{
    Vector<FileChooserFileInfo> files;
    size_t size = state.size();
    files.reserveInitialCapacity(size / 2);
    for (size_t i = 0; i < size; i += 2) {
        if (!state[i + 1].isEmpty())
            files.uncheckedAppend({ state[i], state[i + 1] });
        else
            files.uncheckedAppend({ state[i] });
    }
    return files;
}

const AtomString& FileInputType::formControlType() const
{
    return InputTypeNames::file();
}

FormControlState FileInputType::saveFormControlState() const
{
    if (m_fileList->isEmpty())
        return { };

    auto length = Checked<size_t>(m_fileList->files().size()) * Checked<size_t>(2);

    Vector<String> stateVector;
    stateVector.reserveInitialCapacity(length.unsafeGet());
    for (auto& file : m_fileList->files()) {
        stateVector.uncheckedAppend(file->path());
        stateVector.uncheckedAppend(file->name());
    }
    return FormControlState { WTFMove(stateVector) };
}

void FileInputType::restoreFormControlState(const FormControlState& state)
{
    filesChosen(filesFromFormControlState(state));
}

bool FileInputType::appendFormData(DOMFormData& formData, bool multipart) const
{
    ASSERT(element());
    auto fileList = makeRefPtr(element()->files());
    ASSERT(fileList);

    auto name = element()->name();

    if (!multipart) {
        // Send only the basenames.
        // 4.10.16.4 and 4.10.16.6 sections in HTML5.

        // Unlike the multipart case, we have no special handling for the empty
        // fileList because Netscape doesn't support for non-multipart
        // submission of file inputs, and Firefox doesn't add "name=" query
        // parameter.
        for (auto& file : fileList->files())
            formData.append(name, file->name());
        return true;
    }

    // If no filename at all is entered, return successful but empty.
    // Null would be more logical, but Netscape posts an empty file. Argh.
    if (fileList->isEmpty()) {
        formData.append(name, File::create(element()->document().sessionID(), emptyString()));
        return true;
    }


    for (auto& file : fileList->files())
        formData.append(name, file.get());
    return true;
}

bool FileInputType::valueMissing(const String& value) const
{
    ASSERT(element());
    return element()->isRequired() && value.isEmpty();
}

String FileInputType::valueMissingText() const
{
    ASSERT(element());
    return element()->multiple() ? validationMessageValueMissingForMultipleFileText() : validationMessageValueMissingForFileText();
}

void FileInputType::handleDOMActivateEvent(Event& event)
{
    ASSERT(element());
    auto& input = *element();

    if (input.isDisabledFormControl())
        return;

    if (!UserGestureIndicator::processingUserGesture())
        return;

    if (auto* chrome = this->chrome()) {
        FileChooserSettings settings;
        settings.allowsDirectories = allowsDirectories();
        settings.allowsMultipleFiles = input.hasAttributeWithoutSynchronization(multipleAttr);
        settings.acceptMIMETypes = input.acceptMIMETypes();
        settings.acceptFileExtensions = input.acceptFileExtensions();
        settings.selectedFiles = m_fileList->paths();
#if ENABLE(MEDIA_CAPTURE)
        settings.mediaCaptureType = input.mediaCaptureType();
#endif
        applyFileChooserSettings(settings);
        chrome->runOpenPanel(*input.document().frame(), *m_fileChooser);
    }

    event.setDefaultHandled();
}

RenderPtr<RenderElement> FileInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    return createRenderer<RenderFileUploadControl>(*element(), WTFMove(style));
}

bool FileInputType::canSetStringValue() const
{
    return false;
}

FileList* FileInputType::files()
{
    return m_fileList.ptr();
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
        value = { };
        return true;
    }

    // HTML5 tells us that we're supposed to use this goofy value for
    // file input controls. Historically, browsers revealed the real
    // file path, but that's a privacy problem. Code on the web
    // decided to try to parse the value by looking for backslashes
    // (because that's what Windows file paths use). To be compatible
    // with that code, we make up a fake path for the file.
    value = makeString("C:\\fakepath\\", m_fileList->file(0).name());
    return true;
}

void FileInputType::setValue(const String&, bool, TextFieldEventBehavior)
{
    // FIXME: Should we clear the file list, or replace it with a new empty one here? This is observable from JavaScript through custom properties.
    m_fileList->clear();
    m_icon = nullptr;
    ASSERT(element());
    element()->invalidateStyleForSubtree();
}

bool FileInputType::isFileUpload() const
{
    return true;
}

void FileInputType::createShadowSubtree()
{
    ASSERT(element());
    ASSERT(element()->shadowRoot());
    element()->userAgentShadowRoot()->appendChild(element()->multiple() ? UploadButtonElement::createForMultiple(element()->document()): UploadButtonElement::create(element()->document()));
}

void FileInputType::disabledStateChanged()
{
    ASSERT(element());
    ASSERT(element()->shadowRoot());

    auto root = element()->userAgentShadowRoot();
    if (!root)
        return;
    
    if (auto button = makeRefPtr(childrenOfType<UploadButtonElement>(*root).first()))
        button->setBooleanAttribute(disabledAttr, element()->isDisabledFormControl());
}

void FileInputType::attributeChanged(const QualifiedName& name)
{
    if (name == multipleAttr) {
        if (auto* element = this->element()) {
            ASSERT(element->shadowRoot());
            if (auto root = element->userAgentShadowRoot()) {
                if (auto button = makeRefPtr(childrenOfType<UploadButtonElement>(*root).first()))
                    button->setValue(element->multiple() ? fileButtonChooseMultipleFilesLabel() : fileButtonChooseFileLabel());
            }
        }
    }
    BaseClickableWithKeyInputType::attributeChanged(name);
}

void FileInputType::requestIcon(const Vector<String>& paths)
{
    if (!paths.size()) {
        iconLoaded(nullptr);
        return;
    }

    auto* chrome = this->chrome();
    if (!chrome) {
        iconLoaded(nullptr);
        return;
    }

    if (m_fileIconLoader)
        m_fileIconLoader->invalidate();

    FileIconLoaderClient& client = *this;
    m_fileIconLoader = makeUnique<FileIconLoader>(client);

    chrome->loadIconForFiles(paths, *m_fileIconLoader);
}

void FileInputType::applyFileChooserSettings(const FileChooserSettings& settings)
{
    if (m_fileChooser)
        m_fileChooser->invalidate();

    m_fileChooser = FileChooser::create(this, settings);
}

bool FileInputType::allowsDirectories() const
{
    if (!RuntimeEnabledFeatures::sharedFeatures().directoryUploadEnabled())
        return false;
    ASSERT(element());
    return element()->hasAttributeWithoutSynchronization(webkitdirectoryAttr);
}

void FileInputType::setFiles(RefPtr<FileList>&& files)
{
    setFiles(WTFMove(files), RequestIcon::Yes);
}

void FileInputType::setFiles(RefPtr<FileList>&& files, RequestIcon shouldRequestIcon)
{
    if (!files)
        return;

    ASSERT(element());
    Ref<HTMLInputElement> protectedInputElement(*element());

    unsigned length = files->length();

    bool pathsChanged = false;
    if (length != m_fileList->length())
        pathsChanged = true;
    else {
        for (unsigned i = 0; i < length; ++i) {
            if (files->file(i).path() != m_fileList->file(i).path()) {
                pathsChanged = true;
                break;
            }
        }
    }

    m_fileList = files.releaseNonNull();

    protectedInputElement->setFormControlValueMatchesRenderer(true);
    protectedInputElement->updateValidity();

    if (shouldRequestIcon == RequestIcon::Yes) {
        Vector<String> paths;
        paths.reserveInitialCapacity(length);
        for (auto& file : m_fileList->files())
            paths.uncheckedAppend(file->path());
        requestIcon(paths);
    }

    if (protectedInputElement->renderer())
        protectedInputElement->renderer()->repaint();

    if (pathsChanged) {
        // This call may cause destruction of this instance.
        // input instance is safe since it is ref-counted.
        protectedInputElement->dispatchChangeEvent();
    }
    protectedInputElement->setChangedSinceLastFormControlChangeEvent(false);
}

void FileInputType::filesChosen(const Vector<FileChooserFileInfo>& paths, const String& displayString, Icon* icon)
{
    if (!displayString.isEmpty())
        m_displayString = displayString;

    if (m_fileListCreator)
        m_fileListCreator->cancel();

    auto shouldResolveDirectories = allowsDirectories() ? FileListCreator::ShouldResolveDirectories::Yes : FileListCreator::ShouldResolveDirectories::No;
    auto shouldRequestIcon = icon ? RequestIcon::Yes : RequestIcon::No;
    m_fileListCreator = FileListCreator::create(element()->document().sessionID(), paths, shouldResolveDirectories, [this, shouldRequestIcon](Ref<FileList>&& fileList) {
        setFiles(WTFMove(fileList), shouldRequestIcon);
        m_fileListCreator = nullptr;
    });

    if (icon && !m_fileList->isEmpty())
        iconLoaded(icon);
}

String FileInputType::displayString() const
{
    return m_displayString;
}

void FileInputType::iconLoaded(RefPtr<Icon>&& icon)
{
    if (m_icon == icon)
        return;

    m_icon = WTFMove(icon);
    ASSERT(element());
    if (auto* renderer = element()->renderer())
        renderer->repaint();
}

#if ENABLE(DRAG_SUPPORT)
bool FileInputType::receiveDroppedFiles(const DragData& dragData)
{
    auto paths = dragData.asFilenames();
    if (paths.isEmpty())
        return false;

    ASSERT(element());
    if (element()->hasAttributeWithoutSynchronization(multipleAttr)) {
        Vector<FileChooserFileInfo> files;
        files.reserveInitialCapacity(paths.size());
        for (auto& path : paths)
            files.uncheckedAppend({ path });

        filesChosen(files);
    } else
        filesChosen({ FileChooserFileInfo { paths[0] } });

    return true;
}
#endif // ENABLE(DRAG_SUPPORT)

Icon* FileInputType::icon() const
{
    return m_icon.get();
}

String FileInputType::defaultToolTip() const
{
    unsigned listSize = m_fileList->length();
    if (!listSize) {
        ASSERT(element());
        if (element()->multiple())
            return fileButtonNoFilesSelectedLabel();
        return fileButtonNoFileSelectedLabel();
    }

    StringBuilder names;
    for (unsigned i = 0; i < listSize; ++i) {
        names.append(m_fileList->file(i).name());
        if (i != listSize - 1)
            names.append('\n');
    }
    return names.toString();
}


} // namespace WebCore
