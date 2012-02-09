/*
 * Copyright (C) 2012 Samsung Electronics
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include "Download.h"

#include "FileDownloaderEfl.h"
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {

void Download::start(WebPage* initiatingWebPage)
{
    m_fileDownloader = FileDownloaderEfl::create(this);
    m_fileDownloader->start(initiatingWebPage, m_request);
}

void Download::startWithHandle(WebPage* initiatingPage, ResourceHandle* handle, const ResourceResponse& response)
{
    notImplemented();
}

void Download::cancel()
{
    notImplemented();
}

void Download::platformInvalidate()
{
    notImplemented();
}

void Download::didDecideDestination(const String& destination, bool allowOverwrite)
{
    notImplemented();
}

void Download::platformDidFinish()
{
    notImplemented();
}

void Download::receivedCredential(const AuthenticationChallenge& authenticationChallenge, const Credential& credential)
{
    notImplemented();
}

void Download::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& authenticationChallenge)
{
    notImplemented();
}

void Download::receivedCancellation(const AuthenticationChallenge& authenticationChallenge)
{
    notImplemented();
}

void Download::useCredential(const WebCore::AuthenticationChallenge& authenticationChallenge, const WebCore::Credential& credential)
{
    notImplemented();
}

void Download::continueWithoutCredential(const WebCore::AuthenticationChallenge& authenticationChallenge)
{
    notImplemented();
}

void Download::cancelAuthenticationChallenge(const WebCore::AuthenticationChallenge& authenticationChallenge)
{
    notImplemented();
}

} // namespace WebKit
