/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "IconDatabase.h"

#include "Logging.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/Image.h>
#include <WebCore/SQLiteTransaction.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {
using namespace WebCore;

// This version number is in the DB and marks the current generation of the schema
// Currently, a mismatched schema causes the DB to be wiped and reset.
static const int currentDatabaseVersion = 6;

// Icons expire once every 4 days.
static const Seconds iconExpirationTime { 60 * 60 * 24 * 4 };

// We are not interested in icons that have been unused for more than 30 days.
static const Seconds notUsedIconExpirationTime { 60 * 60 * 24 * 30 };

// Loaded icons are cleared after 30 seconds of being requested.
static const Seconds loadedIconExpirationTime { 30_s };

IconDatabase::IconDatabase(const String& path, AllowDatabaseWrite allowDatabaseWrite)
    : m_workQueue(WorkQueue::create("org.webkit.IconDatabase"))
    , m_allowDatabaseWrite(allowDatabaseWrite)
    , m_clearLoadedIconsTimer(RunLoop::main(), this, &IconDatabase::clearLoadedIconsTimerFired)
{
    ASSERT(isMainThread());
    m_clearLoadedIconsTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);

    // We initialize the database synchronously, it's hopefully fast enough because it makes
    // the implementation a lot simpler.
    BinarySemaphore semaphore;
    m_workQueue->dispatch([&] {
        if (allowDatabaseWrite == AllowDatabaseWrite::No && !FileSystem::fileExists(path)) {
            semaphore.signal();
            return;
        }

        auto databaseDirectory = FileSystem::directoryName(path);
        FileSystem::makeAllDirectories(databaseDirectory);
        if (!m_db.open(path)) {
            LOG_ERROR("Unable to open favicon database at path %s - %s", path.utf8().data(), m_db.lastErrorMsg());
            semaphore.signal();
            return;
        }

        auto databaseVersionNumber = SQLiteStatement(m_db, "SELECT value FROM IconDatabaseInfo WHERE key = 'Version';").getColumnInt(0);
        if (databaseVersionNumber > currentDatabaseVersion) {
            LOG(IconDatabase, "Database version number %d is greater than our current version number %d - closing the database to prevent overwriting newer versions",
                databaseVersionNumber, currentDatabaseVersion);
            m_db.close();
            semaphore.signal();
            return;
        }

        if (databaseVersionNumber < currentDatabaseVersion) {
            if (m_allowDatabaseWrite == AllowDatabaseWrite::No) {
                m_db.close();
                semaphore.signal();
                return;
            }

            m_db.clearAllTables();
        }

        // Reduce sqlite RAM cache size from default 2000 pages (~1.5kB per page). 3MB of cache for icon database is overkill.
        SQLiteStatement(m_db, "PRAGMA cache_size = 200;").executeCommand();

        if (allowDatabaseWrite == AllowDatabaseWrite::Yes) {
            m_pruneTimer = makeUnique<RunLoop::Timer<IconDatabase>>(RunLoop::current(), this, &IconDatabase::pruneTimerFired);
            m_pruneTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
        }

        if (!createTablesIfNeeded())
            populatePageURLToIconURLMap();

        semaphore.signal();
    });
    semaphore.wait();
}

IconDatabase::~IconDatabase()
{
    ASSERT(isMainThread());

    BinarySemaphore semaphore;
    m_workQueue->dispatch([&] {
        if (m_db.isOpen()) {
            m_pruneTimer = nullptr;
            clearStatements();
            m_db.close();
        }
        semaphore.signal();
    });
    semaphore.wait();
}

bool IconDatabase::createTablesIfNeeded()
{
    ASSERT(!isMainThread());

    if (m_db.tableExists("IconInfo") && m_db.tableExists("IconData") && m_db.tableExists("PageURL") && m_db.tableExists("IconDatabaseInfo"))
        return false;

    if (m_allowDatabaseWrite == AllowDatabaseWrite::No) {
        m_db.close();
        return false;
    }

    m_db.clearAllTables();

    if (!m_db.executeCommand("CREATE TABLE PageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create PageURL table in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE INDEX PageURLIndex ON PageURL (url);")) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE TABLE IconInfo (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, stamp INTEGER);")) {
        LOG_ERROR("Could not create IconInfo table in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE INDEX IconInfoIndex ON IconInfo (url, iconID);")) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE TABLE IconData (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, data BLOB);")) {
        LOG_ERROR("Could not create IconData table in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE INDEX IconDataIndex ON IconData (iconID);")) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE TABLE IconDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Could not create IconDatabaseInfo table in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand(String("INSERT INTO IconDatabaseInfo VALUES ('Version', ") + String::number(currentDatabaseVersion) + ");")) {
        LOG_ERROR("Could not insert icon database version into IconDatabaseInfo table (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }

    return true;
}

void IconDatabase::populatePageURLToIconURLMap()
{
    ASSERT(!isMainThread());

    if (!m_db.isOpen())
        return;

    String importQuery = makeString("SELECT PageURL.url, IconInfo.url, IconInfo.stamp FROM PageURL INNER JOIN IconInfo ON PageURL.iconID=IconInfo.iconID WHERE IconInfo.stamp > ", floor((WallTime::now() - notUsedIconExpirationTime).secondsSinceEpoch().seconds()), ';');
    SQLiteStatement query(m_db, importQuery);
    if (query.prepare() != SQLITE_OK) {
        LOG_ERROR("Unable to prepare icon url import query");
        return;
    }

    auto result = query.step();
    LockHolder lockHolder(m_pageURLToIconURLMapLock);
    while (result == SQLITE_ROW) {
        m_pageURLToIconURLMap.set(query.getColumnText(0), query.getColumnText(1));
        result = query.step();
    }

    startPruneTimer();
}

void IconDatabase::clearStatements()
{
    ASSERT(!isMainThread());
    ASSERT(m_db.isOpen());

    m_iconIDForIconURLStatement = nullptr;
    m_setIconIDForPageURLStatement = nullptr;
    m_iconDataStatement = nullptr;
    m_addIconStatement = nullptr;
    m_addIconDataStatement = nullptr;
    m_updateIconTimestampStatement = nullptr;
    m_deletePageURLsForIconStatement = nullptr;
    m_deleteIconDataStatement = nullptr;
    m_deleteIconStatement = nullptr;
    m_pruneIconsStatement = nullptr;
}

void IconDatabase::pruneTimerFired()
{
    ASSERT(!isMainThread());
    ASSERT(m_db.isOpen());

    if (!m_pruneIconsStatement) {
        m_pruneIconsStatement = makeUnique<SQLiteStatement>(m_db, "DELETE FROM IconInfo WHERE stamp <= (?);");
        if (m_pruneIconsStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement pruneIcons failed");
            m_pruneIconsStatement = nullptr;
            return;
        }
    }

    if (m_pruneIconsStatement->bindInt64(1, floor((WallTime::now() - notUsedIconExpirationTime).secondsSinceEpoch().seconds())) != SQLITE_OK) {
        LOG_ERROR("FaviconDatabse::pruneTimerFired failed: %s", m_db.lastErrorMsg());
        return;
    }

    SQLiteTransaction transaction(m_db);
    transaction.begin();
    if (m_pruneIconsStatement->step() == SQLITE_DONE) {
        m_db.executeCommand("DELETE FROM IconData WHERE iconID NOT IN (SELECT iconID FROM IconInfo);");
        m_db.executeCommand("DELETE FROM PageURL WHERE iconID NOT IN (SELECT iconID FROM IconInfo);");
    }
    m_pruneIconsStatement->reset();

    transaction.commit();
}

void IconDatabase::startPruneTimer()
{
    ASSERT(!isMainThread());

    if (!m_pruneTimer || !m_db.isOpen())
        return;

    if (m_pruneTimer->isActive())
        m_pruneTimer->stop();
    m_pruneTimer->startOneShot(10_s);
}

void IconDatabase::clearLoadedIconsTimerFired()
{
    ASSERT(isMainThread());

    LockHolder lockHolder(m_loadedIconsLock);
    auto now = MonotonicTime::now();
    Vector<String> iconsToRemove;
    for (auto iter : m_loadedIcons) {
        if (now - iter.value.second >= loadedIconExpirationTime)
            iconsToRemove.append(iter.key);
    }

    for (auto& iconURL : iconsToRemove)
        m_loadedIcons.remove(iconURL);

    if (!m_loadedIcons.isEmpty())
        startClearLoadedIconsTimer();
}

void IconDatabase::startClearLoadedIconsTimer()
{
    ASSERT(isMainThread());

    if (m_clearLoadedIconsTimer.isActive())
        return;

    m_clearLoadedIconsTimer.startOneShot(loadedIconExpirationTime);
}

Optional<int64_t> IconDatabase::iconIDForIconURL(const String& iconURL, bool& expired)
{
    ASSERT(!isMainThread());
    ASSERT(m_db.isOpen());

    if (!m_iconIDForIconURLStatement) {
        m_iconIDForIconURLStatement = makeUnique<SQLiteStatement>(m_db, "SELECT IconInfo.iconID, IconInfo.stamp FROM IconInfo WHERE IconInfo.url = (?);");
        if (m_iconIDForIconURLStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement iconIDForIconURL failed");
            m_iconIDForIconURLStatement = nullptr;
            return WTF::nullopt;
        }
    }

    if (m_iconIDForIconURLStatement->bindText(1, iconURL) != SQLITE_OK) {
        LOG_ERROR("FaviconDatabse::iconIDForIconURL failed: %s", m_db.lastErrorMsg());
        return WTF::nullopt;
    }

    Optional<int64_t> result;
    if (m_iconIDForIconURLStatement->step() == SQLITE_ROW) {
        result = m_iconIDForIconURLStatement->getColumnInt64(0);
        expired = m_iconIDForIconURLStatement->getColumnInt64(1) <= floor((WallTime::now() - iconExpirationTime).secondsSinceEpoch().seconds());
    }

    m_iconIDForIconURLStatement->reset();
    return result;
}

bool IconDatabase::setIconIDForPageURL(int64_t iconID, const String& pageURL)
{
    ASSERT(!isMainThread());
    ASSERT(m_db.isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_setIconIDForPageURLStatement) {
        m_setIconIDForPageURLStatement = makeUnique<SQLiteStatement>(m_db, "INSERT INTO PageURL (url, iconID) VALUES ((?), ?);");
        if (m_setIconIDForPageURLStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement setIconIDForPageURL failed");
            m_setIconIDForPageURLStatement = nullptr;
            return false;
        }
    }

    if (m_setIconIDForPageURLStatement->bindText(1, pageURL) != SQLITE_OK
        || m_setIconIDForPageURLStatement->bindInt64(2, iconID) != SQLITE_OK) {
        LOG_ERROR("FaviconDatabse::setIconIDForPageURL failed: %s", m_db.lastErrorMsg());
        return false;
    }

    if (m_setIconIDForPageURLStatement->step() != SQLITE_DONE)
        ASSERT_NOT_REACHED();

    m_setIconIDForPageURLStatement->reset();
    return true;
}

Vector<char> IconDatabase::iconData(int64_t iconID)
{
    ASSERT(!isMainThread());
    ASSERT(m_db.isOpen());

    if (!m_iconDataStatement) {
        m_iconDataStatement = makeUnique<SQLiteStatement>(m_db, "SELECT IconData.data FROM IconData WHERE IconData.iconID = (?);");
        if (m_iconDataStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement iconData failed");
            m_iconDataStatement = nullptr;
            return { };
        }
    }

    if (m_iconDataStatement->bindInt64(1, iconID) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::iconData failed: %s", m_db.lastErrorMsg());
        return { };
    }

    Vector<char> result;
    if (m_iconDataStatement->step() == SQLITE_ROW)
        m_iconDataStatement->getColumnBlobAsVector(0, result);

    m_iconDataStatement->reset();
    return result;
}

Optional<int64_t> IconDatabase::addIcon(const String& iconURL, const Vector<char>& iconData)
{
    ASSERT(!isMainThread());
    ASSERT(m_db.isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_addIconStatement) {
        m_addIconStatement = makeUnique<SQLiteStatement>(m_db, "INSERT INTO IconInfo (url, stamp) VALUES (?, 0);");
        if (m_addIconStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement addIcon failed");
            m_addIconStatement = nullptr;
            return WTF::nullopt;
        }
    }
    if (!m_addIconDataStatement) {
        m_addIconDataStatement = makeUnique<SQLiteStatement>(m_db, "INSERT INTO IconData (iconID, data) VALUES (?, ?);");
        if (m_addIconDataStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement addIconData failed");
            m_addIconDataStatement = nullptr;
            return WTF::nullopt;
        }
    }

    if (m_addIconStatement->bindText(1, iconURL) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::addIcon failed: %s", m_db.lastErrorMsg());
        return WTF::nullopt;
    }

    m_addIconStatement->step();
    m_addIconStatement->reset();

    auto iconID = m_db.lastInsertRowID();
    if (m_addIconDataStatement->bindInt64(1, iconID) != SQLITE_OK || m_addIconDataStatement->bindBlob(2, iconData.data(), iconData.size()) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::addIcon failed: %s", m_db.lastErrorMsg());
        return WTF::nullopt;
    }

    m_addIconDataStatement->step();
    m_addIconDataStatement->reset();

    return iconID;
}

void IconDatabase::updateIconTimestamp(int64_t iconID, int64_t timestamp)
{
    ASSERT(!isMainThread());
    ASSERT(m_db.isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_updateIconTimestampStatement) {
        m_updateIconTimestampStatement = makeUnique<SQLiteStatement>(m_db, "UPDATE IconInfo SET stamp = ? WHERE iconID = ?;");
        if (m_updateIconTimestampStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement updateIconTimestamp failed");
            m_updateIconTimestampStatement = nullptr;
            return;
        }
    }

    if (m_updateIconTimestampStatement->bindInt64(1, timestamp) != SQLITE_OK || m_updateIconTimestampStatement->bindInt64(2, iconID) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::updateIconTimestamp failed: %s", m_db.lastErrorMsg());
        return;
    }

    m_updateIconTimestampStatement->step();
    m_updateIconTimestampStatement->reset();
}

void IconDatabase::deleteIcon(int64_t iconID)
{
    ASSERT(!isMainThread());
    ASSERT(m_db.isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_deletePageURLsForIconStatement) {
        m_deletePageURLsForIconStatement = makeUnique<SQLiteStatement>(m_db, "DELETE FROM PageURL WHERE PageURL.iconID = (?);");
        if (m_deletePageURLsForIconStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement deletePageURLsForIcon failed");
            m_deletePageURLsForIconStatement = nullptr;
            return;
        }
    }
    if (!m_deleteIconDataStatement) {
        m_deleteIconDataStatement = makeUnique<SQLiteStatement>(m_db, "DELETE FROM IconData WHERE IconData.iconID = (?);");
        if (m_deleteIconDataStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement deleteIcon failed");
            m_deleteIconDataStatement = nullptr;
            return;
        }
    }
    if (!m_deleteIconStatement) {
        m_deleteIconStatement = makeUnique<SQLiteStatement>(m_db, "DELETE FROM IconInfo WHERE IconInfo.iconID = (?);");
        if (m_deleteIconStatement->prepare() != SQLITE_OK) {
            LOG_ERROR("Preparing statement deleteIcon failed");
            m_deleteIconStatement = nullptr;
            return;
        }
    }

    if (m_deletePageURLsForIconStatement->bindInt64(1, iconID) != SQLITE_OK
        || m_deleteIconDataStatement->bindInt64(1, iconID) != SQLITE_OK
        || m_deleteIconStatement->bindInt64(1, iconID) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::deleteIcon failed: %s", m_db.lastErrorMsg());
        return;
    }

    m_deletePageURLsForIconStatement->step();
    m_deleteIconDataStatement->step();
    m_deleteIconStatement->step();

    m_deletePageURLsForIconStatement->reset();
    m_deleteIconDataStatement->reset();
    m_deleteIconStatement->reset();
}

void IconDatabase::checkIconURLAndSetPageURLIfNeeded(const String& iconURL, const String& pageURL, AllowDatabaseWrite allowDatabaseWrite, CompletionHandler<void(bool, bool)>&& completionHandler)
{
    ASSERT(isMainThread());

    m_workQueue->dispatch([this, protectedThis = makeRef(*this), iconURL = iconURL.isolatedCopy(), pageURL = pageURL.isolatedCopy(), allowDatabaseWrite, completionHandler = WTFMove(completionHandler)]() mutable {
        bool result = false;
        bool changed = false;
        if (m_db.isOpen()) {
            bool canWriteToDatabase = m_allowDatabaseWrite == AllowDatabaseWrite::Yes && allowDatabaseWrite == AllowDatabaseWrite::Yes;
            bool expired = false;
            String cachedIconURL;
            {
                LockHolder lockHolder(m_pageURLToIconURLMapLock);
                cachedIconURL = m_pageURLToIconURLMap.get(pageURL);
            }
            if (cachedIconURL == iconURL)
                result = true;
            else if (auto iconID = iconIDForIconURL(iconURL, expired)) {
                if (expired && canWriteToDatabase) {
                    SQLiteTransaction transaction(m_db);
                    transaction.begin();
                    deleteIcon(iconID.value());
                    transaction.commit();
                } else {
                    result = true;
                    if (!canWriteToDatabase || setIconIDForPageURL(iconID.value(), pageURL)) {
                        LockHolder lockHolder(m_pageURLToIconURLMapLock);
                        m_pageURLToIconURLMap.set(pageURL, iconURL);
                        changed = true;
                    }
                }
            } else if (!canWriteToDatabase) {
                bool foundInMemoryCache;
                {
                    LockHolder lockHolder(m_loadedIconsLock);
                    foundInMemoryCache = m_loadedIcons.contains(iconURL);
                }

                if (foundInMemoryCache) {
                    result = true;
                    LockHolder lockHolder(m_pageURLToIconURLMapLock);
                    m_pageURLToIconURLMap.set(pageURL, iconURL);
                    changed = true;
                }
            }
        }
        startPruneTimer();
        RunLoop::main().dispatch([result, changed, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(result, changed);
        });
    });
}

void IconDatabase::loadIconForPageURL(const String& pageURL, AllowDatabaseWrite allowDatabaseWrite, CompletionHandler<void(NativeImagePtr&&)>&& completionHandler)
{
    ASSERT(isMainThread());

    m_workQueue->dispatch([this, protectedThis = makeRef(*this), pageURL = pageURL.isolatedCopy(), allowDatabaseWrite, timestamp = WallTime::now().secondsSinceEpoch(), completionHandler = WTFMove(completionHandler)]() mutable {
        Optional<int64_t> iconID;
        Vector<char> iconData;
        String iconURL;
        {
            LockHolder lockHolder(m_pageURLToIconURLMapLock);
            iconURL = m_pageURLToIconURLMap.get(pageURL);
        }
        if (m_db.isOpen() && !iconURL.isEmpty()) {
            bool expired;
            iconID = iconIDForIconURL(iconURL, expired);
            if (iconID) {
                LockHolder lockHolder(m_loadedIconsLock);
                if (!m_loadedIcons.contains(iconURL)) {
                    iconData = this->iconData(iconID.value());
                    m_loadedIcons.set(iconURL, std::make_pair<NativeImagePtr, MonotonicTime>(nullptr, { }));
                }
            }
            bool canWriteToDatabase = m_allowDatabaseWrite == AllowDatabaseWrite::Yes && allowDatabaseWrite == AllowDatabaseWrite::Yes;
            if (iconID && canWriteToDatabase)
                updateIconTimestamp(iconID.value(), timestamp.secondsAs<int64_t>());
        }
        startPruneTimer();
        RunLoop::main().dispatch([this, protectedThis = makeRef(*this), iconURL = WTFMove(iconURL), iconData = WTFMove(iconData), completionHandler = WTFMove(completionHandler)]() mutable {
            if (iconURL.isEmpty()) {
                completionHandler(nullptr);
                return;
            }

            LockHolder lockHolder(m_loadedIconsLock);
            auto it = m_loadedIcons.find(iconURL);
            if (it != m_loadedIcons.end() && it->value.first) {
                auto icon = it->value.first;
                it->value.second = MonotonicTime::now();
                startClearLoadedIconsTimer();
                lockHolder.unlockEarly();
                completionHandler(WTFMove(icon));
                return;
            }

            auto addResult = m_loadedIcons.set(iconURL, std::make_pair<NativeImagePtr, MonotonicTime>(nullptr, MonotonicTime::now()));
            if (!iconData.isEmpty()) {
                auto image = BitmapImage::create();
                if (image->setData(SharedBuffer::create(WTFMove(iconData)), true) < EncodedDataStatus::SizeAvailable) {
                    completionHandler(nullptr);
                    return;
                }
                addResult.iterator->value.first = image->nativeImageForCurrentFrame();
            }

            auto icon = addResult.iterator->value.first;
            startClearLoadedIconsTimer();
            lockHolder.unlockEarly();
            completionHandler(WTFMove(icon));
        });
    });
}

String IconDatabase::iconURLForPageURL(const String& pageURL)
{
    ASSERT(isMainThread());

    LockHolder lockHolder(m_pageURLToIconURLMapLock);
    return m_pageURLToIconURLMap.get(pageURL);
}

void IconDatabase::setIconForPageURL(const String& iconURL, const unsigned char* iconData, size_t iconDataSize, const String& pageURL, AllowDatabaseWrite allowDatabaseWrite, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainThread());

    // If database write is not allowed load the icon to cache it in memory only.
    if (m_allowDatabaseWrite == AllowDatabaseWrite::No || allowDatabaseWrite == AllowDatabaseWrite::No) {
        bool result = true;
        {
            LockHolder lockHolder(m_loadedIconsLock);
            auto addResult = m_loadedIcons.set(iconURL, std::make_pair<NativeImagePtr, MonotonicTime>(nullptr, { }));
            if (iconDataSize) {
                auto image = BitmapImage::create();
                if (image->setData(SharedBuffer::create(iconData, iconDataSize), true) < EncodedDataStatus::SizeAvailable)
                    result = false;
                else
                    addResult.iterator->value.first = image->nativeImageForCurrentFrame();
            }
        }
        startClearLoadedIconsTimer();
        m_workQueue->dispatch([this, protectedThis = makeRef(*this), result, iconURL = iconURL.isolatedCopy(), pageURL = pageURL.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            {
                LockHolder lockHolder(m_pageURLToIconURLMapLock);
                m_pageURLToIconURLMap.set(pageURL, iconURL);
            }
            RunLoop::main().dispatch([result, completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(result);
            });
        });
        return;
    }

    Vector<char> data;
    data.reserveInitialCapacity(iconDataSize);
    data.append(reinterpret_cast<const char*>(iconData), iconDataSize);
    m_workQueue->dispatch([this, protectedThis = makeRef(*this), iconURL = iconURL.isolatedCopy(), iconData = WTFMove(data), pageURL = pageURL.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool result = false;
        if (m_db.isOpen()) {
            SQLiteTransaction transaction(m_db);
            transaction.begin();

            bool expired = false;
            auto iconID = iconIDForIconURL(iconURL, expired);
            if (!iconID)
                iconID = addIcon(iconURL, iconData);

            if (iconID) {
                result = true;
                if (setIconIDForPageURL(iconID.value(), pageURL)) {
                    LockHolder lockHolder(m_pageURLToIconURLMapLock);
                    m_pageURLToIconURLMap.set(pageURL, iconURL);
                }
            }

            transaction.commit();
        }
        startPruneTimer();
        RunLoop::main().dispatch([result, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(result);
        });
    });
}

void IconDatabase::clear(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());

    {
        LockHolder lockHolder(m_loadedIconsLock);
        m_loadedIcons.clear();
    }
    m_workQueue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        {
            LockHolder lockHolder(m_pageURLToIconURLMapLock);
            m_pageURLToIconURLMap.clear();
        }

        if (m_db.isOpen() && m_allowDatabaseWrite == AllowDatabaseWrite::Yes) {
            clearStatements();
            m_db.clearAllTables();
            m_db.runVacuumCommand();
            createTablesIfNeeded();
        }

        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

} // namespace WebKit
