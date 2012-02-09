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
#include "FileDownloaderEfl.h"

#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {

PassOwnPtr<FileDownloaderEfl> FileDownloaderEfl::create(Download* download)
{
    return adoptPtr(new FileDownloaderEfl(download));
}

FileDownloaderEfl::FileDownloaderEfl(Download* download)
    : m_download(download)
{
    ASSERT(download);
}

FileDownloaderEfl::~FileDownloaderEfl()
{
}

void FileDownloaderEfl::start(WebPage*, ResourceRequest&)
{
    notImplemented();
}

void FileDownloaderEfl::didReceiveResponse(ResourceHandle*, const ResourceResponse&)
{
    notImplemented();
}

void FileDownloaderEfl::didReceiveData(ResourceHandle*, const char*, int, int)
{
    notImplemented();
}

void FileDownloaderEfl::didFinishLoading(ResourceHandle*, double)
{
    notImplemented();
}

void FileDownloaderEfl::didFail(ResourceHandle*, const ResourceError&)
{
    notImplemented();
}

bool FileDownloaderEfl::shouldUseCredentialStorage(ResourceHandle*)
{
    return false;
}

void FileDownloaderEfl::didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&)
{
    notImplemented();
}

void FileDownloaderEfl::didCancelAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&)
{
    notImplemented();
}

void FileDownloaderEfl::receivedCancellation(ResourceHandle*, const AuthenticationChallenge&)
{
    notImplemented();
}

} // namespace WebKit
