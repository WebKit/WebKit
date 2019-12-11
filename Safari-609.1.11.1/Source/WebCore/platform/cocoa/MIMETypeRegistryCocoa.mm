/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "MIMETypeRegistry.h"

#include <pal/spi/cocoa/NSURLFileTypeMappingsSPI.h>

namespace WebCore {

String MIMETypeRegistry::getMIMETypeForExtension(const String& extension)
{
    return [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:(NSString *)extension];
}

Vector<String> MIMETypeRegistry::getExtensionsForMIMEType(const String& type)
{
    NSArray *stringsArray = [[NSURLFileTypeMappings sharedMappings] extensionsForMIMEType:(NSString *)type];
    Vector<String> stringsVector = Vector<String>();
    unsigned count = [stringsArray count];
    if (count > 0) {
        NSEnumerator* enumerator = [stringsArray objectEnumerator];
        NSString* string;
        while ((string = [enumerator nextObject]) != nil)
            stringsVector.append(string);
    }
    return stringsVector;
}

String MIMETypeRegistry::getPreferredExtensionForMIMEType(const String& type)
{
    // System Previews accept some non-standard MIMETypes, so we can't rely on
    // the file type mappings.
    if (isSystemPreviewMIMEType(type))
        return "usdz"_s;

    return [[NSURLFileTypeMappings sharedMappings] preferredExtensionForMIMEType:(NSString *)type];
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String& MIMEType)
{
#if ENABLE(PDFKIT_PLUGIN)
    // FIXME: This should test if we're actually going to use PDFPlugin,
    // but we only know that in WebKit2 at the moment. This is not a problem
    // in practice because if we don't have PDFPlugin and we go to instantiate the
    // plugin, there won't exist an application plugin supporting these MIME types.
    if (isPDFOrPostScriptMIMEType(MIMEType))
        return true;
#else
    UNUSED_PARAM(MIMEType);
#endif

    return false;
}

}
