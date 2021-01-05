/*
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "wtf/URL.h"

#include <String.h>
#include <wtf/text/CString.h>

#include <Url.h>
#include "wtf/URLParser.h"

namespace WTF {

URL::URL(const BUrl& url)
{
	// The only way to fully initialize an URL is by parsing it from a string.
	// So let's do just that.
	URLParser parser(url.UrlString().String());
	*this = parser.result();
}

URL::operator BUrl() const
{
    BUrl converted;
    converted.SetProtocol(protocol().utf8().data());
    converted.SetUserName(user());
    converted.SetPassword(password());
    converted.SetHost(host().utf8().data());
    if (port())
        converted.SetPort(*port());
    converted.SetPath(path().utf8().data());
    converted.SetRequest(query().utf8().data());
    if (hasFragmentIdentifier())
        converted.SetFragment(fragmentIdentifier().utf8().data());
    return converted;
}

} // namespace WebCore

