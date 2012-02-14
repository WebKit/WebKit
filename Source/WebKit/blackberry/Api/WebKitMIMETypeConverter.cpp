/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitMIMETypeConverter.h"

#include "MIMETypeRegistry.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace BlackBerry {
namespace WebKit {

bool getExtensionForMimeType(const std::string& mime, std::string& extension)
{
    String mimeType(mime.data(), mime.length());
    String preferredExtension = WebCore::MIMETypeRegistry::getPreferredExtensionForMIMEType(mimeType);
    if (preferredExtension.isEmpty())
        return false;

    CString utf8 = preferredExtension.utf8();
    extension.clear();
    extension.append(utf8.data(), utf8.length());
    return true;
}

bool getMimeTypeForExtension(const std::string& extension, std::string& mimeType)
{
    String extStr(extension.data(), extension.length());
    String mime = WebCore::MIMETypeRegistry::getMediaMIMETypeForExtension(extStr);
    if (mime.isEmpty())
        return false;

    CString utf8 = mime.utf8();
    mimeType.clear();
    mimeType.append(utf8.data(), utf8.length());
    return true;
}

} // namespace WebKit
} // namespace BlackBerry
