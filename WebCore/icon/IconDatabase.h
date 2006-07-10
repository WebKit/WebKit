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
#include "StringHash.h"
#include "Timer.h"

#include <wtf/HashMap.h>

namespace WTF {

    template<> struct IntHash<WebCore::IntSize> {
        static unsigned hash(const WebCore::IntSize& key) { return intHash((static_cast<uint64_t>(key.width()) << 32 | key.height())); }
        static bool equal(const WebCore::IntSize& a, const WebCore::IntSize& b) { return a == b; }
    };
    template<> struct DefaultHash<WebCore::IntSize> { typedef IntHash<WebCore::IntSize> Hash; };

} // namespace WTF

namespace WebCore { 

class Image;
    
class SiteIcon {
public:
    SiteIcon(const String& url); 
    ~SiteIcon();
    
    void resetExpiration(time_t newExpiration = 0);
    time_t getExpiration();
        
    Image* getImage(const IntSize&);    
    String getIconURL() { return m_iconURL; }

private:
    String m_iconURL;
    time_t m_expire;
    Image* m_image;
    
    // FIXME - Right now WebCore::Image doesn't have a very good API for accessing multiple representations
    // Even the NSImage way of doing things that we do in WebKit isn't very clean...  once we come up with a 
    // better way of handling that, we'll likely have a map of size-to-images similar to below
    // typedef HashMap<IntSize, Image*> SizeImageMap;
    // SizeImageMap m_images;
};


class IconDatabase
{
//TODO - SiteIcon is never to be used outside of IconDatabase, so make it an internal and remove the friendness
friend class SiteIcon;
public:
    static IconDatabase* sharedIconDatabase();
    
    bool open(const String& path);
    bool isOpen() { return m_db.isOpen(); }
    void close();
 
    Image* iconForPageURL(const String&, const IntSize&, bool cache = true);
    Image* iconForIconURL(const String&, const IntSize&, bool cache = true);
    String iconURLForPageURL(const String&);
    Image* defaultIcon(const IntSize&);

    void retainIconForURL(const String&);
    void releaseIconForURL(const String&);
    
    void setPrivateBrowsingEnabled(bool flag);
    bool getPrivateBrowsingEnabled() { return m_privateBrowsingEnabled; }

    bool hasIconForIconURL(const String&);
    
    // TODO - The following 3 methods were considered private in WebKit - analyze the impact of making them
    // public here in WebCore - I don't see any real badness with doing that...  after all if Chuck Norris wants to muck
    // around with the icons in his database, he's going to anyway
    void setIconDataForIconURL(const void* data, int size, const String&);
    void setHaveNoIconForIconURL(const String&);
    void setIconURLForPageURL(const String& iconURL, const String& pageURL);

    static const int currentDatabaseVersion;    
private:
    IconDatabase();
    ~IconDatabase();
    
    // Remove the Icon and IconResource entry for this icon, as well as the SiteIcon object in memory
    void forgetIconForIconURLFromDatabase(const String&);
    
    // Wipe all icons from the DB
    void removeAllIcons();
    
    // Removes icons from the db that no longer have any PageURLs pointing to them
    // If numberToPrune is negative, ALL dangling icons will be pruned
    void pruneUnreferencedIcons(int numberToPrune);
    
    // Removes ALL icons that are unretained
    // Meant to be called just once on startup, after initial retains are complete
    // via a timer fire
    void pruneUnretainedIcons(Timer<IconDatabase>*);
    
    // Add up the retain count for an iconURL
    int totalRetainCountForIconURL(const String&);
    
    bool isEnabled();
    
    // Do a quick check to make sure the database tables are in place and the db version is current
    bool isValidDatabase();
    
    // Delete all tables from the database
    void clearDatabase();
    
    // Create the tables and triggers for the on-disk database
    void recreateDatabase();

    // Create/Delete the temporary, in-memory tables used for private browsing
    void createPrivateTables();
    void deletePrivateTables();
    
    // The following four methods will either find the iconID for a given iconURL or, if the iconURL
    // isn't in the table yet and you don't pass a false flag, will create an entry and return the resulting iconID
    int64_t establishIconIDForIconURL(const String&, bool create = true);
    int64_t establishIconIDForEscapedIconURL(const String&, bool create = true);
    int64_t establishTemporaryIconIDForIconURL(const String&, bool create = true);
    int64_t establishTemporaryIconIDForEscapedIconURL(const String&, bool create = true);
    
    // Since we store data in both the ondisk tables and temporary tables, these methods will do the work 
    // for either case
    void performSetIconURLForPageURL(int64_t iconID, const String& pageTable, const String& pageURL);
    void performSetIconDataForIconID(int64_t iconID, const String& resourceTable, const void* data, int size);
    
    // The following three methods follow the sqlite convention for blob data
    // They return a const void* which is a pointer to the data buffer, and store
    // the size of the data buffer in the int& parameter
    Vector<unsigned char> imageDataForIconID(int);
    Vector<unsigned char> imageDataForIconURL(const String&);
    Vector<unsigned char> imageDataForPageURL(const String&);
    
    static IconDatabase* m_sharedInstance;
    static const int DefaultCachedPageCount;
    
    SQLDatabase m_db;
    bool m_privateBrowsingEnabled;
    
    Timer<IconDatabase> m_startupTimer;
    
    typedef HashMap<String, SiteIcon*> SiteIconMap;
    SiteIconMap m_pageURLToSiteIcons;
    SiteIconMap m_iconURLToSiteIcons;
};

} //namespace WebCore

#endif
