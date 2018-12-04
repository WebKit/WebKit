/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Xan Lopez <xan@gnome.org>
 * Copyright (C) 2008, 2010 Collabora Ltd.
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2009, 2013 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2009, 2010, 2011, 2012 Igalia S.L.
 * Copyright (C) 2009 John Kjellberg <john.kjellberg@power.alstom.com>
 * Copyright (C) 2012 Intel Corporation
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
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ResourceHandle.h"

#if USE(SOUP)

#include "CredentialStorage.h"
#include "FileSystem.h"
#include "GUniquePtrSoup.h"
#include "HTTPParsers.h"
#include "LocalizedStrings.h"
#include "MIMETypeRegistry.h"
#include "NetworkStorageSession.h"
#include "NetworkingContext.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "SoupNetworkSession.h"
#include "TextEncoding.h"
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <sys/stat.h>
#include <sys/types.h>
#if !COMPILER(MSVC)
#include <unistd.h>
#endif
#include <wtf/CompletionHandler.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/CString.h>

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal() = default;

ResourceHandle::~ResourceHandle()
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::platformContinueSynchronousDidReceiveResponse()
{
    ASSERT_NOT_REACHED();
}

bool ResourceHandle::start()
{
    ASSERT_NOT_REACHED();
    return false;
}

void ResourceHandle::cancel()
{
    ASSERT_NOT_REACHED();
}

bool ResourceHandle::shouldUseCredentialStorage()
{
    ASSERT_NOT_REACHED();
    return false;
}

void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::platformSetDefersLoading(bool)
{
    ASSERT_NOT_REACHED();
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext*, const ResourceRequest&, StoredCredentialsPolicy, ResourceError&, ResourceResponse&, Vector<char>&)
{
    ASSERT_NOT_REACHED();
}

}

#endif
