/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#ifndef ICONDATABASE_H
#define ICONDATABASE_H

#include "config.h"

#include "IntSize.h"
#include "PlatformString.h"
#include "SQLDatabase.h"

#include <wtf/HashMap.h>

namespace WebCore { 

class Image;

class WebIcon {
public:
    WebIcon();
    ~WebIcon();
    
    void resetExpiration(time_t newExpiration = 0);
    time_t getExpiration() { return m_expire; }
    
    //getImage() inherently touch()es the icon
    Image* getImage();
    
    //incase a user wants to manually touch() the icon
    void touch();
    time_t getTouch() { return m_touch; }
        
private:
    time_t m_touch;
    time_t m_expire;
    Image* m_image;
};


class IconDatabase
{
//TODO - WebIcon is never to be used outside of IconDatabase
friend class WebIcon;
public:
    static IconDatabase* sharedIconDatabase();
    
    bool open(const String& path);
    bool isOpen() { return m_db.isOpen(); }
    void close();

    Image* iconForURL(const String&, const IntSize&, bool cache = true);
    String iconURLForURL(const String&);
    Image* defaultIcon(const IntSize&);

    void retainIconForURL(const String&);
    void releaseIconForURL(const String&);
    
    void setPrivateBrowsingEnabled(bool flag);

    bool hasIconForIconURL(const String&);
    
    //TODO - The following 3 methods were considered private in WebKit - analyze the impact of making them
    //public here in WebCore - I don't see any real badness with doing that...  afterall if Chuck Norris wants to muck
    //around with the icons in his database, he's going to anyway
    void setIconForIconURL(Image*, const String&);
    void setHaveNoIconForIconURL(const String&);
    void setIconURLForPageURL(const String& iconURL, const String& pageURL);

    static const int currentDatabaseVersion;    
private:
    IconDatabase();
    ~IconDatabase();
    
    void removeAllIcons();
    bool isEnabled();
    
    bool isValidDatabase();
    void clearDatabase();
    void recreateDatabase();
    
    static IconDatabase* m_sharedInstance;
    static const int DefaultCachedPageCount;
    
    SQLDatabase m_db;
    bool m_privateBrowsingEnabled;
    
    typedef WTF::HashMap<String, WebIcon*> WebIconMap;
    WebIconMap m_webIcons;
};

} //namespace WebCore

#endif
