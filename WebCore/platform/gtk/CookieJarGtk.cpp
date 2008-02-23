/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "CookieJar.h"

#include "KURL.h"
#include "PlatformString.h"
#include "StringHash.h"

#include <wtf/HashMap.h>

namespace WebCore {

static HashMap<String, String> cookieJar;

void setCookies(Document* /*document*/, const KURL& url, const KURL& /*policyURL*/, const String& value)
{
    cookieJar.set(url.string(), value);
}

String cookies(const Document* /*document*/, const KURL& url)
{
    return cookieJar.get(url.string());
}

bool cookiesEnabled(const Document* /*document*/)
{
    return true;
}

}
