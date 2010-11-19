/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "Event.h"
#include "File.h"
#include "FileList.h"
#include "FormDataList.h"
#include "HTMLInputElement.h"
#include "LocalizedStrings.h"
#include "RenderFileUploadControl.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

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
    return value.isEmpty();
}

String FileInputType::valueMissingText() const
{
    return element()->multiple() ? validationMessageValueMissingForMultipleFileText() : validationMessageValueMissingForFileText();
}

bool FileInputType::handleDOMActivateEvent(Event* event)
{
    if (element()->disabled() || !element()->renderer())
        return false;
    toRenderFileUploadControl(element()->renderer())->click();
    event->setDefaultHandled();
    return true;
}

RenderObject* FileInputType::createRenderer(RenderArena* arena, RenderStyle*) const
{
    return new (arena) RenderFileUploadControl(element());
}

} // namespace WebCore
