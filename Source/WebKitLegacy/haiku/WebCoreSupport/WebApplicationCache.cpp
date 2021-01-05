/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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

#include "WebApplicationCache.h"

#include <wtf/RetainPtr.h>
#include <ApplicationCache.h>
#include <ApplicationCacheStorage.h>
#include "SecurityOrigin.h"

#include <FindDirectory.h>
#include <Path.h>

// WebApplicationCache ---------------------------------------------------------------------------

WebApplicationCache::WebApplicationCache()
    : m_refCount(0)
{
}

WebApplicationCache::~WebApplicationCache()
{
}

WebApplicationCache* WebApplicationCache::createInstance()
{
    WebApplicationCache* instance = new WebApplicationCache();
    return instance;
}

static String applicationCachePath()
{
	BPath path;
	find_directory(B_USER_CACHE_DIRECTORY, &path);
	path.Append("webkit");

    return path.Path();
}

WebCore::ApplicationCacheStorage& WebApplicationCache::storage()
{
    static WebCore::ApplicationCacheStorage& storage
		= WebCore::ApplicationCacheStorage::create(applicationCachePath(),
			"ApplicationCache").leakRef();

    return storage;
}

