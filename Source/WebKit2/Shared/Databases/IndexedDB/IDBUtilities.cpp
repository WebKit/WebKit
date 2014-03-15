/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#include "config.h"
#include "IDBUtilities.h"

#include <WebCore/SecurityOrigin.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebKit {

String uniqueDatabaseIdentifier(const String& databaseName, const WebCore::SecurityOrigin& openingOrigin, const WebCore::SecurityOrigin& mainFrameOrigin)
{
    StringBuilder stringBuilder;

    String originString = openingOrigin.toString();
    if (originString == "null")
        return String();
    stringBuilder.append(originString);
    stringBuilder.append("_");

    originString = mainFrameOrigin.toString();
    if (originString == "null")
        return String();
    stringBuilder.append(originString);

    stringBuilder.append("_");
    stringBuilder.append(databaseName);

    return stringBuilder.toString();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
