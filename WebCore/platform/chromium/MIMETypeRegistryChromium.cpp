/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
#include "MIMETypeRegistry.h"

#include "ChromiumBridge.h"
#include "CString.h"
#include "MediaPlayer.h"

// NOTE: Unlike other ports, we don't use the shared implementation bits in
// MIMETypeRegistry.cpp.  Instead, we need to route most functions via the
// ChromiumBridge to the embedder.

namespace WebCore {

// Checks if any of the plugins handle this extension, and if so returns the
// plugin's mime type for this extension.  Otherwise returns an empty string.
// See PluginsChromium.cpp for the implementation of this function.
String getPluginMimeTypeFromExtension(const String& extension);

String MIMETypeRegistry::getMIMETypeForExtension(const String &ext)
{
    return ChromiumBridge::mimeTypeForExtension(ext);
}

// Returns the file extension if one is found.  Does not include the dot in the
// filename.  E.g., 'html'.
String MIMETypeRegistry::getPreferredExtensionForMIMEType(const String& type)
{
    // Prune out any parameters in case they happen to have snuck in there...
    // FIXME: Is this really necessary??
    String mimeType = type.substring(0, static_cast<unsigned>(type.find(';')));

    String ext = ChromiumBridge::preferredExtensionForMIMEType(type);
    if (!ext.isEmpty() && ext[0] == '.')
        ext = ext.substring(1);

    return ext;
}

String MIMETypeRegistry::getMIMETypeForPath(const String& path)
{
    int pos = path.reverseFind('.');
    if (pos < 0)
        return "application/octet-stream";
    String extension = path.substring(pos + 1);
    String mimeType = getMIMETypeForExtension(extension);
    if (mimeType.isEmpty()) {
        // If there's no mimetype registered for the extension, check to see
        // if a plugin can handle the extension.
        mimeType = getPluginMimeTypeFromExtension(extension);
    }
    if (mimeType.isEmpty())
        return "application/octet-stream";
    return mimeType;
}

bool MIMETypeRegistry::isSupportedImageMIMEType(const String& mimeType)
{ 
    return ChromiumBridge::isSupportedImageMIMEType(mimeType);
}

bool MIMETypeRegistry::isSupportedImageResourceMIMEType(const String& mimeType)
{ 
    return isSupportedImageMIMEType(mimeType); 
}

bool MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(const String& mimeType)
{
    // FIXME: Fill this out. See: http://trac.webkit.org/changeset/30888
    return isSupportedImageMIMEType(mimeType);
}

bool MIMETypeRegistry::isSupportedJavaScriptMIMEType(const String& mimeType)
{
    return ChromiumBridge::isSupportedJavaScriptMIMEType(mimeType);
}
    
bool MIMETypeRegistry::isSupportedNonImageMIMEType(const String& mimeType)
{
    return ChromiumBridge::isSupportedNonImageMIMEType(mimeType);
}

bool MIMETypeRegistry::isSupportedMediaMIMEType(const String& mimeType)
{
    HashSet<String> supportedMediaMIMETypes;
#if ENABLE(VIDEO)
    MediaPlayer::getSupportedTypes(supportedMediaMIMETypes);
#endif
    return !mimeType.isEmpty() && supportedMediaMIMETypes.contains(mimeType);
}

bool MIMETypeRegistry::isJavaAppletMIMEType(const String& mimeType)
{
    // Since this set is very limited and is likely to remain so we won't bother with the overhead
    // of using a hash set.
    // Any of the MIME types below may be followed by any number of specific versions of the JVM,
    // which is why we use startsWith()
    return mimeType.startsWith("application/x-java-applet", false) 
        || mimeType.startsWith("application/x-java-bean", false) 
        || mimeType.startsWith("application/x-java-vm", false);
}

String MIMETypeRegistry::getMediaMIMETypeForExtension(const String&)
{
    return String();
}

static HashSet<String>& dummyHashSet()
{
    ASSERT_NOT_REACHED();
    static HashSet<String> dummy;
    return dummy;
}

// NOTE: the following methods should never be reached
HashSet<String>& MIMETypeRegistry::getSupportedImageMIMETypes() { return dummyHashSet(); }
HashSet<String>& MIMETypeRegistry::getSupportedImageResourceMIMETypes() { return dummyHashSet(); }
HashSet<String>& MIMETypeRegistry::getSupportedImageMIMETypesForEncoding() { return dummyHashSet(); }
HashSet<String>& MIMETypeRegistry::getSupportedNonImageMIMETypes() { return dummyHashSet(); }
HashSet<String>& MIMETypeRegistry::getSupportedMediaMIMETypes() { return dummyHashSet(); }

} // namespace WebCore
