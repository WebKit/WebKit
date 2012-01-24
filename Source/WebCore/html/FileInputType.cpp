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
#include "Event.h"
#include "File.h"
#include "FileList.h"
#include "FileSystem.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "LocalizedStrings.h"
#include "RenderFileUploadControl.h"
#include "ScriptController.h"
#include "ShadowRoot.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace HTMLNames;

class UploadButtonElement : public HTMLInputElement {
public:
    static PassRefPtr<UploadButtonElement> create(Document*);
    static PassRefPtr<UploadButtonElement> createForMultiple(Document*);

private:
    UploadButtonElement(Document*);

    virtual const AtomicString& shadowPseudoId() const;
};

PassRefPtr<UploadButtonElement> UploadButtonElement::create(Document* document)
{
    RefPtr<UploadButtonElement> button = adoptRef(new UploadButtonElement(document));
    button->setType("button");
    button->setValue(fileButtonChooseFileLabel());
    return button.release();
}

PassRefPtr<UploadButtonElement> UploadButtonElement::createForMultiple(Document* document)
{
    RefPtr<UploadButtonElement> button = adoptRef(new UploadButtonElement(document));
    button->setType("button");
    button->setValue(fileButtonChooseMultipleFilesLabel());
    return button.release();
}

UploadButtonElement::UploadButtonElement(Document* document)
    : HTMLInputElement(inputTag, document, 0, false)
{
}

const AtomicString& UploadButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, pseudoId, ("-webkit-file-upload-button"));
    return pseudoId;
}

inline FileInputType::FileInputType(HTMLInputElement* element)
    : BaseButtonInputType(element)
    , m_fileList(FileList::create())
{
}

PassOwnPtr<InputType> FileInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new FileInputType(element));
}

const AtomicString& FileInputType::formControlType() const
{
    return InputTypeNames::file();
}

bool FileInputType::appendFormData(FormDataList& encoding, bool multipart) const
{
    FileList* fileList = element()->files();
    unsigned numFiles = fileList->length();
    if (!multipart) {
        // Send only the basenames.
        // 4.10.16.4 and 4.10.16.6 sections in HTML5.

        // Unlike the multipart case, we have no special handling for the empty
        // fileList because Netscape doesn't support for non-multipart
        // submission of file inputs, and Firefox doesn't add "name=" query
        // parameter.
        for (unsigned i = 0; i < numFiles; ++i)
            encoding.appendData(element()->name(), fileList->item(i)->fileName());
        return true;
    }

    // If no filename at all is entered, return successful but empty.
    // Null would be more logical, but Netscape posts an empty file. Argh.
    if (!numFiles) {
        encoding.appendBlob(element()->name(), File::create(""));
        return true;
    }

    for (unsigned i = 0; i < numFiles; ++i)
        encoding.appendBlob(element()->name(), fileList->item(i));
    return true;
}

bool FileInputType::valueMissing(const String& value) const
{
    return element()->required() && value.isEmpty();
}

String FileInputType::valueMissingText() const
{
    return element()->multiple() ? validationMessageValueMissingForMultipleFileText() : validationMessageValueMissingForFileText();
}

void FileInputType::handleDOMActivateEvent(Event* event)
{
    if (element()->disabled() || !element()->renderer())
        return;

    if (!ScriptController::processingUserGesture())
        return;

    if (Chrome* chrome = this->chrome()) {
        FileChooserSettings settings;
        HTMLInputElement* input = element();
#if ENABLE(DIRECTORY_UPLOAD)
        settings.allowsDirectoryUpload = input->fastHasAttribute(webkitdirectoryAttr);
        settings.allowsMultipleFiles = settings.allowsDirectoryUpload || input->fastHasAttribute(multipleAttr);
#else
        settings.allowsMultipleFiles = input->fastHasAttribute(multipleAttr);
#endif
        settings.acceptMIMETypes = input->acceptMIMETypes();
        settings.selectedFiles = m_fileList->paths();
        chrome->runOpenPanel(input->document()->frame(), newFileChooser(settings));
    }
    event->setDefaultHandled();
}

RenderObject* FileInputType::createRenderer(RenderArena* arena, RenderStyle*) const
{
    return new (arena) RenderFileUploadControl(element());
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
    value = "C:\\fakepath\\" + m_fileList->item(0)->fileName();
    return true;
}

bool FileInputType::storesValueSeparateFromAttribute()
{
    return true;
}

void FileInputType::setValue(const String&, bool, bool)
{
    m_fileList->clear();
    m_icon.clear();
    element()->setNeedsStyleRecalc();
}

void FileInputType::setFileList(const Vector<String>& paths)
{
    m_fileList->clear();
    size_t size = paths.size();

#if ENABLE(DIRECTORY_UPLOAD)
    // If a directory is being selected, the UI allows a directory to be chosen
    // and the paths provided here share a root directory somewhere up the tree;
    // we want to store only the relative paths from that point.
    if (size && element()->fastHasAttribute(webkitdirectoryAttr)) {
        // Find the common root path.
        String rootPath = directoryName(paths[0]);
        for (size_t i = 1; i < size; i++) {
            while (!paths[i].startsWith(rootPath))
                rootPath = directoryName(rootPath);
        }
        rootPath = directoryName(rootPath);
        ASSERT(rootPath.length());
        int rootLength = rootPath.length();
        if (rootPath[rootLength - 1] != '\\' && rootPath[rootLength - 1] != '/')
            rootLength += 1;
        for (size_t i = 0; i < size; i++) {
            // Normalize backslashes to slashes before exposing the relative path to script.
            String relativePath = paths[i].substring(rootLength).replace('\\', '/');
            m_fileList->append(File::createWithRelativePath(paths[i], relativePath));
        }
        return;
    }
#endif

    for (size_t i = 0; i < size; i++)
        m_fileList->append(File::create(paths[i]));
}

bool FileInputType::isFileUpload() const
{
    return true;
}

void FileInputType::createShadowSubtree()
{
    ExceptionCode ec = 0;
    element()->ensureShadowRoot()->appendChild(element()->multiple() ? UploadButtonElement::createForMultiple(element()->document()): UploadButtonElement::create(element()->document()), ec);
}

void FileInputType::multipleAttributeChanged()
{
    UploadButtonElement* button = static_cast<UploadButtonElement*>(element()->ensureShadowRoot()->firstChild());
    if (button)
        button->setValue(element()->multiple() ? fileButtonChooseMultipleFilesLabel() : fileButtonChooseFileLabel());
}

void FileInputType::requestIcon(const Vector<String>& paths)
{
    if (!paths.size())
        return;

    if (Chrome* chrome = this->chrome())
        chrome->loadIconForFiles(paths, newFileIconLoader());
}

void FileInputType::filesChosen(const Vector<String>& paths)
{
    RefPtr<HTMLInputElement> input = element();

    bool pathsChanged = false;
    if (paths.size() != m_fileList->length())
        pathsChanged = true;
    else {
        for (unsigned i = 0; i < paths.size(); ++i) {
            if (paths[i] != m_fileList->item(i)->path()) {
                pathsChanged = true;
                break;
            }
        }
    }

    setFileList(paths);

    input->setFormControlValueMatchesRenderer(true);
    input->notifyFormStateChanged();
    input->setNeedsValidityCheck();

    requestIcon(paths);

    if (input->renderer())
        input->renderer()->repaint();

    if (pathsChanged) {
        // This call may cause destruction of this instance.
        // input instance is safe since it is ref-counted.
        input->HTMLElement::dispatchChangeEvent();
    }
    input->setChangedSinceLastFormControlChangeEvent(false);
}

#if ENABLE(DIRECTORY_UPLOAD)
void FileInputType::receiveDropForDirectoryUpload(const Vector<String>& paths)
{
    if (Chrome* chrome = this->chrome()) {
        FileChooserSettings settings;
        HTMLInputElement* input = element();
        settings.allowsDirectoryUpload = true;
        settings.allowsMultipleFiles = true;
        settings.selectedFiles.append(paths[0]);
        settings.acceptMIMETypes = input->acceptMIMETypes();
        chrome->enumerateChosenDirectory(newFileChooser(settings));
    }
}
#endif

void FileInputType::updateRendering(PassRefPtr<Icon> icon)
{
    if (m_icon == icon)
        return;

    m_icon = icon;
    if (element()->renderer())
        element()->renderer()->repaint();
}

void FileInputType::receiveDroppedFiles(const Vector<String>& paths)
{
    HTMLInputElement* input = element();
#if ENABLE(DIRECTORY_UPLOAD)
    if (input->fastHasAttribute(webkitdirectoryAttr)) {
        receiveDropForDirectoryUpload(paths);
        return;
    }
#endif

    if (input->fastHasAttribute(multipleAttr))
        filesChosen(paths);
    else {
        Vector<String> firstPathOnly;
        firstPathOnly.append(paths[0]);
        filesChosen(firstPathOnly);
    }
}

Icon* FileInputType::icon() const
{
    return m_icon.get();
}

String FileInputType::defaultToolTip() const
{
    FileList* fileList = m_fileList.get();
    unsigned listSize = fileList->length();
    if (!listSize) {
        if (element()->multiple())
            return fileButtonNoFilesSelectedLabel();
        return fileButtonNoFileSelectedLabel();
    }

    StringBuilder names;
    for (size_t i = 0; i < listSize; ++i) {
        names.append(fileList->item(i)->fileName());
        if (i != listSize - 1)
            names.append('\n');
    }
    return names.toString();
}

} // namespace WebCore
