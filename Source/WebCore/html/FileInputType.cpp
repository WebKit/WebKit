/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2014 Google Inc. All rights reserved.
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
#include "ContainerNodeInlines.h"
#include "DOMFormData.h"
#include "DirectoryFileListCreator.h"
#include "DragData.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "Event.h"
#include "File.h"
#include "FileChooser.h"
#include "FileList.h"
#include "FormController.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "InputTypeNames.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "MIMETypeRegistry.h"
#include "RenderFileUploadControl.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "UserAgentParts.h"
#include "UserGestureIndicator.h"
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(MAC)
#include "ImageUtilities.h"
#include "UTIUtilities.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FileInputType);

using namespace HTMLNames;

FileInputType::FileInputType(HTMLInputElement& element)
    : BaseClickableWithKeyInputType(Type::File, element)
    , m_fileList(FileList::create())
{
    ASSERT(needsShadowSubtree());
}

FileInputType::~FileInputType()
{
    if (RefPtr fileChooser = m_fileChooser)
        fileChooser->invalidate();

    if (m_fileIconLoader)
        m_fileIconLoader->invalidate();
}

std::pair<Vector<FileChooserFileInfo>, String> FileInputType::filesFromFormControlState(const FormControlState& state)
{
    Vector<FileChooserFileInfo> files;
    size_t size = state.size() - 1;
    files.reserveInitialCapacity(size / 2);
    for (size_t i = 0; i < size; i += 2)
        files.append({ state[i], { }, state[i + 1] });

    return { files, state[size] };
}

const AtomString& FileInputType::formControlType() const
{
    return InputTypeNames::file();
}

FormControlState FileInputType::saveFormControlState() const
{
    if (m_fileList->isEmpty())
        return { };

    auto length = Checked<size_t>(m_fileList->files().size()) * Checked<size_t>(2) + Checked<size_t>(1);

    Vector<AtomString> stateVector;
    stateVector.reserveInitialCapacity(length);
    for (auto& file : m_fileList->files()) {
        stateVector.append(AtomString { file->path() });
        stateVector.append(AtomString { file->name() });
    }
    stateVector.append(AtomString { m_displayString });
    return FormControlState { WTF::move(stateVector) };
}

void FileInputType::restoreFormControlState(const FormControlState& state)
{
    auto [files, displayString] = filesFromFormControlState(state);
    filesChosen(files, displayString);
}

bool FileInputType::appendFormData(DOMFormData& formData) const
{
    ASSERT(element());
    Ref element = *this->element();
    Ref fileList = m_fileList;

    auto name = element->name();

    // If no filename at all is entered, return successful but empty.
    // Null would be more logical, but Netscape posts an empty file. Argh.
    if (fileList->isEmpty()) {
        Ref document = element->document();
        auto file = File::create(document.ptr(), Blob::create(document.ptr(), { }, defaultMIMEType()), emptyString());
        formData.append(name, file);
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
    return protectedElement()->multiple() ? validationMessageValueMissingForMultipleFileText() : validationMessageValueMissingForFileText();
}

void FileInputType::handleDOMActivateEvent(Event& event)
{
    ASSERT(element());

    if (protectedElement()->isDisabledFormControl())
        return;

    if (!UserGestureIndicator::processingUserGesture())
        return;

    showPicker();
    event.setDefaultHandled();
}

void FileInputType::showPicker()
{
    ASSERT(element());
    if (auto* chrome = this->chrome()) {
        applyFileChooserSettings();
        chrome->runOpenPanel(*element()->document().protectedFrame(), *protectedFileChooser());
    }
}

bool FileInputType::allowsShowPickerAcrossFrames()
{
    return true;
}

RenderPtr<RenderElement> FileInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    // FIXME: https://github.com/llvm/llvm-project/pull/142471 Moving style is not unsafe.
    SUPPRESS_UNCOUNTED_ARG return createRenderer<RenderFileUploadControl>(*protectedElement(), WTF::move(style));
}

bool FileInputType::canSetStringValue() const
{
    return false;
}

String FileInputType::firstElementPathForInputValue() const
{
    if (m_fileList->isEmpty())
        return { };

    // HTML5 tells us that we're supposed to use this goofy value for
    // file input controls. Historically, browsers revealed the real
    // file path, but that's a privacy problem. Code on the web
    // decided to try to parse the value by looking for backslashes
    // (because that's what Windows file paths use). To be compatible
    // with that code, we make up a fake path for the file.
    return makeString("C:\\fakepath\\"_s, protectedFiles()->file(0).name());
}

void FileInputType::setValue(const String&, bool valueChanged, TextFieldEventBehavior, TextControlSetValueSelection)
{
    // FIXME: Should we clear the file list, or replace it with a new empty one here? This is observable from JavaScript through custom properties.
    if (!valueChanged)
        return;
    
    protectedFiles()->clear();
    m_icon = nullptr;
    ASSERT(element());
    Ref element = *this->element();
    element->invalidateStyleForSubtree();
    element->updateValidity();
}

void FileInputType::createShadowSubtree()
{
    ASSERT(needsShadowSubtree());
    ASSERT(element());
    ASSERT(element()->shadowRoot());

    Ref element = *this->element();
    Ref button = HTMLInputElement::create(inputTag, protect(element->document()), nullptr, false);
    {
        ScriptDisallowedScope::EventAllowedScope eventAllowedScopeBeforeAppend { button };
        button->setAttributeWithoutSynchronization(typeAttr, InputTypeNames::button());
        button->setUserAgentPart(UserAgentParts::fileSelectorButton());
        button->setValue(element->multiple() ? fileButtonChooseMultipleFilesLabel() : fileButtonChooseFileLabel());
    }
    Ref shadowRoot = *element->userAgentShadowRoot();
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { shadowRoot };
    shadowRoot->appendChild(ContainerNode::ChildChange::Source::Parser, button);
    disabledStateChanged();
}

static RefPtr<HTMLInputElement> fileSelectorButton(const Element& element)
{
    RefPtr root = element.userAgentShadowRoot();
    return root ? downcast<HTMLInputElement>(root->firstChild()) : nullptr;
}

void FileInputType::disabledStateChanged()
{
    ASSERT(element());

    Ref element = *this->element();
    if (RefPtr button = fileSelectorButton(element))
        button->setBooleanAttribute(disabledAttr, element->isDisabledFormControl());
}

void FileInputType::attributeChanged(const QualifiedName& name)
{
    if (name == multipleAttr) {
        if (RefPtr element = this->element()) {
            if (RefPtr button = fileSelectorButton(*element))
                button->setValue(element->multiple() ? fileButtonChooseMultipleFilesLabel() : fileButtonChooseFileLabel());
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
    Ref element = *this->element();

    FileChooserSettings settings;
    settings.allowsDirectories = allowsDirectories();
    settings.allowsMultipleFiles = element->hasAttributeWithoutSynchronization(multipleAttr);
    settings.acceptMIMETypes = element->acceptMIMETypes();
    settings.acceptFileExtensions = element->acceptFileExtensions();
    settings.selectedFiles = protectedFiles()->paths();
#if ENABLE(MEDIA_CAPTURE)
    settings.mediaCaptureType = element->mediaCaptureType();
#endif
    return settings;
}

void FileInputType::applyFileChooserSettings()
{
    if (RefPtr fileChooser = m_fileChooser)
        fileChooser->invalidate();

    m_fileChooser = FileChooser::create(*this, fileChooserSettings());
}

bool FileInputType::allowsDirectories() const
{
    ASSERT(element());
    Ref element = *this->element();
    if (!protect(element->document())->settings().directoryUploadEnabled())
        return false;
    return element->hasAttributeWithoutSynchronization(webkitdirectoryAttr);
}

bool FileInputType::dirAutoUsesValue() const
{
    return false;
}

void FileInputType::setFiles(RefPtr<FileList>&& files, WasSetByJavaScript wasSetByJavaScript)
{
    setFiles(WTF::move(files), RequestIcon::Yes, wasSetByJavaScript);
}

void FileInputType::setFiles(RefPtr<FileList>&& files, RequestIcon shouldRequestIcon, WasSetByJavaScript wasSetByJavaScript)
{
    if (!files)
        return;

    ASSERT(element());
    Ref element = *this->element();

    unsigned length = files->length();

    bool pathsChanged = false;
    if (length != m_fileList->length())
        pathsChanged = true;
    else {
        Ref currentFiles = m_fileList;
        for (unsigned i = 0; i < length; ++i) {
            if (files->file(i).path() != currentFiles->file(i).path() || !FileSystem::fileIDsAreEqual(files->file(i).fileID(), currentFiles->file(i).fileID())) {
                pathsChanged = true;
                break;
            }
        }
    }

    m_fileList = files.releaseNonNull();

    element->setFormControlValueMatchesRenderer(true);
    element->updateValidity();

    if (shouldRequestIcon == RequestIcon::Yes)
        requestIcon(protectedFiles()->paths());

    if (CheckedPtr renderer = element->renderer())
        renderer->repaint();

    if (wasSetByJavaScript == WasSetByJavaScript::Yes)
        return;

    if (pathsChanged) {
        // This call may cause destruction of this instance.
        // input instance is safe since it is ref-counted.
        element->dispatchInputEvent();
        element->dispatchChangeEvent();
    } else
        element->dispatchCancelEvent();

    element->setChangedSinceLastFormControlChangeEvent(false);
    element->setInteractedWithSinceLastFormSubmitEvent(true);
}

void FileInputType::filesChosen(const Vector<FileChooserFileInfo>& paths, const String& displayString, Icon* icon)
{
    if (!displayString.isEmpty())
        m_displayString = displayString;

    if (RefPtr creator = m_directoryFileListCreator)
        creator->cancel();

    RefPtr document = element() ? &element()->document() : nullptr;
    if (!allowsDirectories()) {
        auto files = paths.map([document](auto& fileInfo) {
            std::optional<FileSystem::PlatformFileID> fileID;
            if (auto handle = FileSystem::openFile(fileInfo.path, FileSystem::FileOpenMode::Read); handle)
                fileID = handle.id();
            return File::create(document.get(), fileInfo.path, fileInfo.replacementPath, fileInfo.displayName, fileID);
        });
        didCreateFileList(FileList::create(WTF::move(files)), icon);
        return;
    }

    Ref creator = DirectoryFileListCreator::create([weakThis = WeakPtr { *this }, icon = RefPtr { icon }](Ref<FileList>&& fileList) mutable {
        ASSERT(isMainThread());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->didCreateFileList(WTF::move(fileList), WTF::move(icon));
    });
    creator->start(document.get(), paths);
    m_directoryFileListCreator = WTF::move(creator);
}

void FileInputType::filesChosen(const Vector<String>& paths, const Vector<String>& replacementPaths)
{
    ASSERT(element());
    ASSERT(!paths.isEmpty());

    size_t size = protectedElement()->hasAttributeWithoutSynchronization(multipleAttr) ? paths.size() : 1;

    Vector<FileChooserFileInfo> files(size, [&](size_t i) {
        return FileChooserFileInfo { paths[i], i < replacementPaths.size() ? replacementPaths[i] : nullString(), { } };
    });

    filesChosen(WTF::move(files));
}

void FileInputType::fileChoosingCancelled()
{
    ASSERT(element());
    protectedElement()->dispatchCancelEvent();
}

void FileInputType::didCreateFileList(Ref<FileList>&& fileList, RefPtr<Icon>&& icon)
{
    Ref protectedThis { *this };

    ASSERT(!allowsDirectories() || m_directoryFileListCreator);
    m_directoryFileListCreator = nullptr;

    setFiles(WTF::move(fileList), icon ? RequestIcon::No : RequestIcon::Yes, WasSetByJavaScript::No);
    if (icon && !m_fileList->isEmpty() && element())
        iconLoaded(WTF::move(icon));
}

String FileInputType::displayString() const
{
    return m_displayString;
}

void FileInputType::iconLoaded(RefPtr<Icon>&& icon)
{
    if (m_icon == icon)
        return;

    m_icon = WTF::move(icon);
    ASSERT(element());
    if (CheckedPtr renderer = element()->renderer())
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

    auto callFilesChosen = [protectedThis = Ref { *this }, paths](const Vector<String>& replacementPaths) {
        protectedThis->filesChosen(paths, replacementPaths);
    };

    sharedImageTranscodingQueueSingleton().dispatch([callFilesChosen = WTF::move(callFilesChosen), transcodingPaths = crossThreadCopy(WTF::move(transcodingPaths)), transcodingUTI = WTF::move(transcodingUTI).isolatedCopy(), transcodingExtension = WTF::move(transcodingExtension).isolatedCopy()]() mutable {
        ASSERT(!RunLoop::isMain());

        auto replacementPaths = transcodeImages(transcodingPaths, transcodingUTI, transcodingExtension);
        ASSERT(transcodingPaths.size() == replacementPaths.size());

        RunLoop::mainSingleton().dispatch([callFilesChosen = WTF::move(callFilesChosen), replacementPaths = crossThreadCopy(WTF::move(replacementPaths))] {
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
        if (protectedElement()->multiple())
            return fileButtonNoFilesSelectedLabel();
        return fileButtonNoFileSelectedLabel();
    }

    StringBuilder names;
    Ref fileList = m_fileList;
    for (unsigned i = 0; i < listSize; ++i) {
        names.append(fileList->file(i).name());
        if (i != listSize - 1)
            names.append('\n');
    }
    return names.toString();
}


} // namespace WebCore
