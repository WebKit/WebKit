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
 
#ifndef IconDatabase_h
#define IconDatabase_h

#if ENABLE(ICONDATABASE)
#include "SQLDatabase.h"
#endif

#include "StringHash.h"
#include "Timer.h"
#include <wtf/Noncopyable.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore { 

class Image;
class IntSize;
class IconDataCache;
class SharedBuffer;

#if ENABLE(ICONDATABASE)
class SQLTransaction;
#endif

class IconDatabase : Noncopyable {
public:
    bool open(const String& path);
    bool isOpen();
    void close();
    
    void removeAllIcons();
    
    bool isEmpty();
 
    Image* iconForPageURL(const String&, const IntSize&, bool cache = true);
    Image* iconForIconURL(const String&, const IntSize&, bool cache = true);
    String iconURLForPageURL(const String&);
    Image* defaultIcon(const IntSize&);

    void retainIconForPageURL(const String&);
    void releaseIconForPageURL(const String&);
    
    void setPrivateBrowsingEnabled(bool flag);
    bool isPrivateBrowsingEnabled() const;

    bool hasEntryForIconURL(const String&);

    bool isIconExpiredForIconURL(const String&);
    
    void setIconDataForIconURL(PassRefPtr<SharedBuffer> data, const String&);
    void setHaveNoIconForIconURL(const String&);
    
    // Returns true if the set actually took place, false if the mapping already existed
    bool setIconURLForPageURL(const String& iconURL, const String& pageURL);
    
    void setEnabled(bool enabled);
    bool enabled() const;

    bool imported();
    void setImported(bool);

    static const String& defaultDatabaseFilename();
    
private:
    IconDatabase();
    ~IconDatabase();
    friend IconDatabase* iconDatabase();

#if ENABLE(ICONDATABASE)
    // This tries to get the iconID for the IconURL and, if it doesn't exist and createIfNecessary is true,
    // it will create the entry and return the new iconID
    int64_t establishIconIDForIconURL(SQLDatabase&, const String&, bool createIfNecessary = true);
    
    // This method returns the SiteIcon for the given IconURL and, if it doesn't exist it creates it first
    IconDataCache* getOrCreateIconDataCache(const String& iconURL);

    // Remove traces of the given pageURL
    void forgetPageURL(const String& pageURL);
    
    // Remove the current database entry for this IconURL
    void forgetIconForIconURLFromDatabase(const String&);
    
    void setIconURLForPageURLInDatabase(const String&, const String&);
        
    // Called by the startup timer, this method removes all icons that are unretained
    // after initial retains are complete, and pageURLs that are dangling
    void pruneUnretainedIconsOnStartup(Timer<IconDatabase>*);
    
    // Called by the prune timer, this method periodically removes all the icons in the pending-deletion
    // queue
    void updateDatabase(Timer<IconDatabase>*);
    
    // This is called by updateDatabase and when private browsing shifts, and when the DB is closed down
    void syncDatabase();

    // Called to eliminate database inconsistency where pages point to non-existent iconIDs
    void checkForDanglingPageURLs(bool pruneIfFound);
    
    // Determine if an IconURL is still retained by anyone
    bool isIconURLRetained(const String&);
    
    // Do a quick check to make sure the database tables are in place and the db version is current
    bool isValidDatabase(SQLDatabase&);
        
    // Create the tables and triggers for the given database.
    void createDatabaseTables(SQLDatabase&);
    
    // Returns the image data for the given IconURL, checking both databases if necessary
    PassRefPtr<SharedBuffer> imageDataForIconURL(const String& iconURL);
    
    // Retains an iconURL, bringing it back from the brink if it was pending deletion
    void retainIconURL(const String& iconURL);
    
    // Releases an iconURL, putting it on the pending delete queue if it's totally released
    void releaseIconURL(const String& iconURL);
    
    // Query - Checks for at least 1 entry in the PageURL table
    bool pageURLTableIsEmptyQuery(SQLDatabase&);
    
    // Query - Returns the time stamp for an Icon entry
    int timeStampForIconURLQuery(SQLDatabase&, const String& iconURL);    
    SQLStatement* m_timeStampForIconURLStatement;
    
    // Query - Returns the IconURL for a PageURL
    String iconURLForPageURLQuery(SQLDatabase&, const String& pageURL);    
    SQLStatement* m_iconURLForPageURLStatement;
    
    // Query - Checks for the existence of the given IconURL in the Icon table
    bool hasIconForIconURLQuery(SQLDatabase& db, const String& iconURL);
    SQLStatement* m_hasIconForIconURLStatement;
    
    // Query - Deletes a PageURL from the PageURL table
    void forgetPageURLQuery(SQLDatabase& db, const String& pageURL);
    SQLStatement* m_forgetPageURLStatement;
    
    // Query - Sets the Icon.iconID for a PageURL in the PageURL table
    void setIconIDForPageURLQuery(SQLDatabase& db, int64_t, const String&);
    SQLStatement* m_setIconIDForPageURLStatement;
    
    // Query - Returns the iconID for the given IconURL
    int64_t getIconIDForIconURLQuery(SQLDatabase& db, const String& iconURL);
    SQLStatement* m_getIconIDForIconURLStatement;
    
    // Query - Creates the Icon entry for the given IconURL and returns the resulting iconID
    int64_t addIconForIconURLQuery(SQLDatabase& db, const String& iconURL);
    SQLStatement* m_addIconForIconURLStatement;
    
    // Query - Returns the image data from the given database for the given IconURL
    PassRefPtr<SharedBuffer> imageDataForIconURLQuery(SQLDatabase& db, const String& iconURL);
    SQLStatement* m_imageDataForIconURLStatement;

    // Query - Returns whether or not the "imported" flag is set
    bool importedQuery(SQLDatabase&);
    SQLStatement* m_importedStatement;
    
    // Query - Sets the "imported" flag
    void setImportedQuery(SQLDatabase&, bool);
    SQLStatement* m_setImportedStatement;

    void deleteAllPreparedStatements(bool withSync);

    SQLDatabase m_mainDB;
    SQLDatabase m_privateBrowsingDB;
    SQLDatabase* m_currentDB;
    
    IconDataCache* m_defaultIconDataCache;
    
    bool m_isEnabled;
    bool m_privateBrowsingEnabled;
    
    Timer<IconDatabase> m_startupTimer;
    Timer<IconDatabase> m_updateTimer;
    
    bool m_initialPruningComplete;
    SQLTransaction* m_initialPruningTransaction;
    SQLStatement* m_preparedPageRetainInsertStatement;
    
    bool m_imported;
    mutable bool m_isImportedSet;
    
    HashMap<String, IconDataCache*> m_iconURLToIconDataCacheMap;
    HashSet<IconDataCache*> m_iconDataCachesPendingUpdate;
    
    HashMap<String, String> m_pageURLToIconURLMap;
    HashSet<String> m_pageURLsPendingAddition;

    // This will keep track of the retaincount for each pageURL
    HashMap<String, int> m_pageURLToRetainCount;
    // This will keep track of the retaincount for each iconURL (ie - the number of pageURLs retaining this icon)
    HashMap<String, int> m_iconURLToRetainCount;

    HashSet<String> m_pageURLsPendingDeletion;
    HashSet<String> m_iconURLsPendingDeletion;
#endif
};

// Function to obtain the global icon database.
IconDatabase* iconDatabase();

} // namespace WebCore

#endif
