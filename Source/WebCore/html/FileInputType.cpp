/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "DirectoryFileListCreator.h"
#include "DragData.h"
#include "ElementChildIterator.h"
#include "Event.h"
#include "File.h"
#include "FileList.h"
#include "FormController.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "InputTypeNames.h"
#include "LocalizedStrings.h"
#include "MIMETypeRegistry.h"
#include "RenderFileUploadControl.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "UserGestureIndicator.h"
#include <wtf/FileSystem.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(MAC)
#include "ImageUtilities.h"
#include "UTIUtilities.h"
#endif

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
    static Ref<UploadButtonElement> createInternal(Document&, const String& value);
    bool isUploadButton() const override { return true; }
    
    UploadButtonElement(Document&);
};

Ref<UploadButtonElement> UploadButtonElement::create(Document& document)
{
    return createInternal(document, fileButtonChooseFileLabel());
}

Ref<UploadButtonElement> UploadButtonElement::createForMultiple(Document& document)
{
    return createInternal(document, fileButtonChooseMultipleFilesLabel());
}

Ref<UploadButtonElement> UploadButtonElement::createInternal(Document& document, const String& value)
{
    auto button = adoptRef(*new UploadButtonElement(document));
    static MainThreadNeverDestroyed<const AtomString> buttonName("button", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> webkitFileUploadButtonName("-webkit-file-upload-button", AtomString::ConstructFromLiteral);
    button->setType(buttonName);
    button->setPseudo(webkitFileUploadButtonName);
    button->setValue(value);
    return button;
}

UploadButtonElement::UploadButtonElement(Document& document)
    : HTMLInputElement(inputTag, document, 0, false)
{
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

    if (m_fileIconLoader)
        m_fileIconLoader->invalidate();
}

Vector<FileChooserFileInfo> FileInputType::filesFromFormControlState(const FormControlState& state)
{
    Vector<FileChooserFileInfo> files;
    size_t size = state.size();
    files.reserveInitialCapacity(size / 2);
    for (size_t i = 0; i < size; i += 2)
        files.uncheckedAppend({ state[i], { }, state[i + 1] });
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
        auto* document = element() ? &element()->document() : nullptr;
        formData.append(name, File::create(document, emptyString()));
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
        applyFileChooserSettings();
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

FileChooserSettings FileInputType::fileChooserSettings() const
{
    ASSERT(element());
    auto& input = *element();

    FileChooserSettings settings;
    settings.allowsDirectories = allowsDirectories();
    settings.allowsMultipleFiles = input.hasAttributeWithoutSynchronization(multipleAttr);
    settings.acceptMIMETypes = input.acceptMIMETypes();
    settings.acceptFileExtensions = input.acceptFileExtensions();
    settings.selectedFiles = m_fileList->paths();
#if ENABLE(MEDIA_CAPTURE)
    settings.mediaCaptureType = input.mediaCaptureType();
#endif
    return settings;
}

void FileInputType::applyFileChooserSettings()
{
    if (m_fileChooser)
        m_fileChooser->invalidate();

    m_fileChooser = FileChooser::create(this, fileChooserSettings());
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
        protectedInputElement->dispatchInputEvent();
        protectedInputElement->dispatchChangeEvent();
    }
    protectedInputElement->setChangedSinceLastFormControlChangeEvent(false);
}

void FileInputType::filesChosen(const Vector<FileChooserFileInfo>& paths, const String& displayString, Icon* icon)
{
    if (!displayString.isEmpty())
        m_displayString = displayString;

    if (m_directoryFileListCreator)
        m_directoryFileListCreator->cancel();

    auto* document = element() ? &element()->document() : nullptr;
    if (!allowsDirectories()) {
        auto files = paths.map([document](auto& fileInfo) {
            return File::create(document, fileInfo.path, fileInfo.replacementPath, fileInfo.displayName);
        });
        didCreateFileList(FileList::create(WTFMove(files)), icon);
        return;
    }

    m_directoryFileListCreator = DirectoryFileListCreator::create([this, weakThis = makeWeakPtr(*this), icon = makeRefPtr(icon)](Ref<FileList>&& fileList) mutable {
        ASSERT(isMainThread());
        if (!weakThis)
            return;
        didCreateFileList(WTFMove(fileList), WTFMove(icon));
    });
    m_directoryFileListCreator->start(document, paths);
}

void FileInputType::filesChosen(const Vector<String>& paths, const Vector<String>& replacementPaths)
{
    ASSERT(element());
    ASSERT(!paths.isEmpty());

    size_t size = element()->hasAttributeWithoutSynchronization(multipleAttr) ? paths.size() : 1;

    Vector<FileChooserFileInfo> files;
    files.reserveInitialCapacity(size);

    for (size_t i = 0; i < size; ++i)
        files.uncheckedAppend({ paths[i], i < replacementPaths.size() ? replacementPaths[i] : nullString(), { } });

    filesChosen(files);
}

void FileInputType::didCreateFileList(Ref<FileList>&& fileList, RefPtr<Icon>&& icon)
{
    auto protectedThis = makeRef(*this);

    ASSERT(!allowsDirectories() || m_directoryFileListCreator);
    m_directoryFileListCreator = nullptr;

    setFiles(WTFMove(fileList), icon ? RequestIcon::Yes : RequestIcon::No);
    if (icon && !m_fileList->isEmpty() && element())
        iconLoaded(WTFMove(icon));
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
bool FileInputType::receiveDroppedFilesWithImageTranscoding(const Vector<String>& paths)
{
#if PLATFORM(MAC)
    auto settings = fileChooserSettings();
    auto allowedMIMETypes = MIMETypeRegistry::allowedMIMETypes(settings.acceptMIMETypes, settings.acceptFileExtensions);
    
    auto transcodingPaths = findImagesForTranscoding(paths, allowedMIMETypes);
    if (transcodingPaths.isEmpty())
        return { };

    auto transcodingMIMEType = MIMETypeRegistry::preferredImageMIMETypeForEncoding(allowedMIMETypes, { });
    if (transcodingMIMEType.isNull())
        return { };

    auto transcodingUTI = WebCore::UTIFromMIMEType(transcodingMIMEType);
    auto transcodingExtension = WebCore::MIMETypeRegistry::preferredExtensionForMIMEType(transcodingMIMEType);

    auto callFilesChosen = [protectedThis = makeRef(*this), paths](const Vector<String>& replacementPaths) {
        protectedThis->filesChosen(paths, replacementPaths);
    };

    sharedImageTranscodingQueue().dispatch([callFilesChosen = WTFMove(callFilesChosen), transcodingPaths = transcodingPaths.isolatedCopy(), transcodingUTI = transcodingUTI.isolatedCopy(), transcodingExtension = transcodingExtension.isolatedCopy()]() mutable {
        ASSERT(!RunLoop::isMain());

        auto replacementPaths = transcodeImages(transcodingPaths, transcodingUTI, transcodingExtension);
        ASSERT(transcodingPaths.size() == replacementPaths.size());

        RunLoop::main().dispatch([callFilesChosen = WTFMove(callFilesChosen), replacementPaths = replacementPaths.isolatedCopy()]() {
            callFilesChosen(replacementPaths);
        });
    });

    return true;
#else
    UNUSED_PARAM(paths);
    return false;
#endif
}

bool FileInputType::receiveDroppedFiles(const DragData& dragData)
{
    auto paths = dragData.asFilenames();
    if (paths.isEmpty())
        return false;

    if (receiveDroppedFilesWithImageTranscoding(paths))
        return true;
    
    filesChosen(paths);
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
