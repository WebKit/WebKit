/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "WebPageSerializer.h"

#include "KURL.h"
#include "PlatformString.h"

#include "WebFrame.h"
#include "WebPageSerializerClient.h"
#include "WebPageSerializerImpl.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebVector.h"

using namespace WebCore;

namespace WebKit {

bool WebPageSerializer::serialize(WebFrame* frame,
                                  bool recursive,
                                  WebPageSerializerClient* client,
                                  const WebVector<WebURL>& links,
                                  const WebVector<WebString>& localPaths,
                                  const WebString& localDirectoryName)
{
    WebPageSerializerImpl serializerImpl(
        frame, recursive, client, links, localPaths, localDirectoryName);
    return serializerImpl.serialize();
}

WebString WebPageSerializer::generateMetaCharsetDeclaration(const WebString& charset)
{
    return String::format("<META http-equiv=\"Content-Type\" content=\"text/html; charset=%s\">",
                          charset.utf8().data());
}

WebString WebPageSerializer::generateMarkOfTheWebDeclaration(const WebURL& url)
{
    return String::format("\n<!-- saved from url=(%04d)%s -->\n",
                          static_cast<int>(url.spec().length()),
                          url.spec().data());
}

WebString WebPageSerializer::generateBaseTagDeclaration(const WebString& baseTarget)
{
    String targetDeclaration;
    if (!baseTarget.isEmpty())
        targetDeclaration = String::format(" target=\"%s\"", baseTarget.utf8().data());
    return String::format("<BASE href=\".\"%s>", targetDeclaration.utf8().data());
}

}  // namespace WebKit
