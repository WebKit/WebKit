/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebContentReader.h"

#import "ArchiveResource.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "Editor.h"
#import "EditorClient.h"
#import "FragmentScriptingPermission.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "HTMLAnchorElement.h"
#import "HTMLNames.h"
#import "LegacyWebArchive.h"
#import "MIMETypeRegistry.h"
#import "Text.h"
#import "UTIUtilities.h"
#import "markup.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <wtf/unicode/CharacterNames.h>

namespace WebCore {

bool WebContentReader::readURL(const URL& url, const String& title)
{
    if (url.isEmpty())
        return false;

    // FIXME: This code shoudln't be accessing selection and changing the behavior.
    if (!frame.editor().client()->hasRichlyEditableSelection()) {
        if (readPlainText([(NSURL *)url absoluteString]))
            return true;
    }

    if ([(NSURL *)url isFileURL])
        return false;

    auto anchor = HTMLAnchorElement::create(*frame.document());
    anchor->setAttributeWithoutSynchronization(HTMLNames::hrefAttr, url.string());

    String linkText = title.length() ? title : String([[(NSURL *)url absoluteString] precomposedStringWithCanonicalMapping]);
    anchor->appendChild(frame.document()->createTextNode(linkText));

    auto newFragment = frame.document()->createDocumentFragment();
    if (fragment)
        newFragment->appendChild(Text::create(*frame.document(), { &space, 1 }));
    newFragment->appendChild(anchor);
    addFragment(WTFMove(newFragment));
    return true;
}

}
