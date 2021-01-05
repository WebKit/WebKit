/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ResourceRequest.h"

#include "CookieJar.h"
#include "NetworkingContext.h"

#include <support/Locker.h>
#include <UrlProtocolRoster.h>
#include <UrlRequest.h>
#include <HttpRequest.h>
#include <LocaleRoster.h>
#include <wtf/text/CString.h>

namespace WebCore {

BUrlRequest* ResourceRequest::toNetworkRequest(BUrlContext* context)
{
    BUrlRequest* request = BUrlProtocolRoster::MakeRequest(url());

    if (!request) {
        m_url = WTF::aboutBlankURL(); // This tells the ResourceLoader we failed.
        return NULL;
    }

    if (context)
        request->SetContext(context);

    if (timeoutInterval() > 0)
        request->SetTimeout(timeoutInterval());

    BHttpRequest* httpRequest = dynamic_cast<BHttpRequest*>(request);
    if (httpRequest != NULL) {
        const HTTPHeaderMap &headers = httpHeaderFields();
        BHttpHeaders* requestHeaders = new BHttpHeaders();

        for (HTTPHeaderMap::const_iterator it = headers.begin(),
                end = headers.end(); it != end; ++it)
        {
            requestHeaders->AddHeader(it->key.utf8().data(),
                it->value.utf8().data());
        }

        if (!fUsername.IsEmpty()) {
            httpRequest->SetUserName(fUsername);
            httpRequest->SetPassword(fPassword);
        }

        if (requestHeaders->HasHeader("Accept-Language") < 0) {
            // Add the default languages
            BMessage message;
            BLocaleRoster::Default()->GetPreferredLanguages(&message);

            BString languages;
            BString language;
            for(int i = 0; message.FindString("language", i, &language) == B_OK; i++) {
                if (i != 0)
                    languages << ',';
                languages << language;
                languages << ";q=";
                // This will lead to negative priorities if there are more than
                // 100 languages. Hopefully no one can read that much...
                languages << 1 - (i / 100.f);

                int underscore = language.FindFirst('_');
                if (underscore > 0) {
                    // Some page only accept 2-letter language codes (eg Google
                    // account login). Include that if we have a country suffix
                    language.Truncate(underscore);
                    languages << ',';
                    languages << language;
                    languages << ";q=";
                    languages << 1 - (i / 100.f);
                }
            }

            requestHeaders->AddHeader("Accept-Language", languages);
         }

        httpRequest->AdoptHeaders(requestHeaders);
    }

    return request;
}


void ResourceRequest::setCredentials(const char* username, const char* password)
{
    fUsername = username;
    fPassword = password;
}

}

