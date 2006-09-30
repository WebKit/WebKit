/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "DocLoader.h"

#include "Cache.h"
#include "CachedCSSStyleSheet.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "Document.h"
#include "Frame.h"
#include "LoaderFunctions.h"
#include "loader.h"

namespace WebCore {

DocLoader::DocLoader(Frame *frame, Document* doc)
{
    m_cachePolicy = CachePolicyVerify;
    m_expireDate = 0;
    m_bautoloadImages = true;
    m_frame = frame;
    m_doc = doc;
    m_loadInProgress = false;

    Cache::init();
    Cache::docloaders->add(this);
}

DocLoader::~DocLoader()
{
    Cache::docloaders->remove(this);
}

void DocLoader::setExpireDate(time_t _expireDate)
{
    m_expireDate = _expireDate;
}

bool DocLoader::needReload(const KURL& fullURL)
{
    bool reload = false;
    if (m_cachePolicy == CachePolicyVerify) {
       if (!m_reloadedURLs.contains(fullURL.url())) {
          CachedResource* existing = Cache::get(fullURL.url());
          if (existing && existing->isExpired()) {
             Cache::remove(existing);
             m_reloadedURLs.add(fullURL.url());
             reload = true;
          }
       }
    } else if ((m_cachePolicy == CachePolicyReload) || (m_cachePolicy == CachePolicyRefresh)) {
       if (!m_reloadedURLs.contains(fullURL.url())) {
          CachedResource* existing = Cache::get(fullURL.url());
          if (existing)
             Cache::remove(existing);
          m_reloadedURLs.add(fullURL.url());
          reload = true;
       }
    }
    return reload;
}

CachedImage *DocLoader::requestImage(const String& url)
{
    KURL fullURL = m_doc->completeURL(url.deprecatedString());

    if (CheckIfReloading(this))
        setCachePolicy(CachePolicyReload);

    bool reload = needReload(fullURL);

    CachedImage *cachedObject = Cache::requestImage(this, fullURL, reload, m_expireDate);
    CheckCacheObjectStatus(this, cachedObject);
    return cachedObject;
}

CachedCSSStyleSheet *DocLoader::requestCSSStyleSheet(const String& url, const String& charset)
{
    KURL fullURL = m_doc->completeURL(url.deprecatedString());

    if (CheckIfReloading(this))
        setCachePolicy(CachePolicyReload);

    bool reload = needReload(fullURL);

    CachedCSSStyleSheet* cachedObject = Cache::requestCSSStyleSheet(this, url, reload, m_expireDate, charset);
    CheckCacheObjectStatus(this, cachedObject);
    return cachedObject;
}

CachedScript* DocLoader::requestScript(const String& url, const String& charset)
{
    KURL fullURL = m_doc->completeURL(url.deprecatedString());

    if (CheckIfReloading(this))
        setCachePolicy(CachePolicyReload);

    bool reload = needReload(fullURL);

    CachedScript *cachedObject = Cache::requestScript(this, url, reload, m_expireDate, charset);
    CheckCacheObjectStatus(this, cachedObject);
    return cachedObject;
}

#ifdef XSLT_SUPPORT
CachedXSLStyleSheet* DocLoader::requestXSLStyleSheet(const String& url)
{
    KURL fullURL = m_doc->completeURL(url.deprecatedString());
    
    if (CheckIfReloading(this))
        setCachePolicy(CachePolicyReload);
    
    bool reload = needReload(fullURL);
    
    CachedXSLStyleSheet *cachedObject = Cache::requestXSLStyleSheet(this, url, reload, m_expireDate);
    CheckCacheObjectStatus(this, cachedObject);
    return cachedObject;
}
#endif

#ifdef XBL_SUPPORT
CachedXBLDocument* DocLoader::requestXBLDocument(const String& url)
{
    KURL fullURL = m_doc->completeURL(url.deprecatedString());
    
    // FIXME: Is this right for XBL?
    if (m_frame && m_frame->onlyLocalReferences() && fullURL.protocol() != "file") return 0;
    
    if (CheckIfReloading(this))
        setCachePolicy(CachePolicyReload);
    
    bool reload = needReload(fullURL);
    
    CachedXBLDocument *cachedObject = Cache::requestXBLDocument(this, url, reload, m_expireDate);
    CheckCacheObjectStatus(this, cachedObject);
    return cachedObject;
}
#endif

void DocLoader::setAutoloadImages(bool enable)
{
    if (enable == m_bautoloadImages)
        return;

    m_bautoloadImages = enable;

    if (!m_bautoloadImages)
        return;

    HashMap<String, CachedResource*>::iterator end = m_docObjects.end();
    for (HashMap<String, CachedResource*>::iterator it = m_docObjects.begin(); it != end; ++it) {
        CachedResource* co = it->second;
        if (co->type() == CachedResource::ImageResource) {
            CachedImage *img = const_cast<CachedImage*>(static_cast<const CachedImage *>(co));

            CachedResource::Status status = img->status();
            if (status != CachedResource::Unknown)
                continue;

            Cache::loader()->load(this, img, true);
        }
    }
}

void DocLoader::setCachePolicy(CachePolicy cachePolicy)
{
    m_cachePolicy = cachePolicy;
}

void DocLoader::removeCachedObject(CachedResource* o) const
{
    m_docObjects.remove(o->url());
}

void DocLoader::setLoadInProgress(bool load)
{
    m_loadInProgress = load;
    if (!load)
        m_frame->loadDone();
}

}
