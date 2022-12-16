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
    ASSERT(isMainRunLoop());
    m_clearLoadedIconsTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);

    // We initialize the database synchronously, it's hopefully fast enough because it makes
    // the implementation a lot simpler.
    m_workQueue->dispatchSync([&] {
        if (allowDatabaseWrite == AllowDatabaseWrite::No && !FileSystem::fileExists(path))
            return;

        auto databaseDirectory = FileSystem::parentPath(path);
        FileSystem::makeAllDirectories(databaseDirectory);
        if (!m_db.open(path)) {
            LOG_ERROR("Unable to open favicon database at path %s - %s", path.utf8().data(), m_db.lastErrorMsg());
            return;
        }

        auto versionStatement = m_db.prepareStatement("SELECT value FROM IconDatabaseInfo WHERE key = 'Version';"_s);
        auto databaseVersionNumber = versionStatement ? versionStatement->columnInt(0) : 0;
        if (databaseVersionNumber > currentDatabaseVersion) {
            LOG(IconDatabase, "Database version number %d is greater than our current version number %d - closing the database to prevent overwriting newer versions",
                databaseVersionNumber, currentDatabaseVersion);
            m_db.close();
            return;
        }

        if (databaseVersionNumber < currentDatabaseVersion) {
            if (m_allowDatabaseWrite == AllowDatabaseWrite::No) {
                m_db.close();
                return;
            }

            m_db.clearAllTables();
        }

        // Reduce sqlite RAM cache size from default 2000 pages (~1.5kB per page). 3MB of cache for icon database is overkill.
        m_db.executeCommand("PRAGMA cache_size = 200;"_s);

        if (allowDatabaseWrite == AllowDatabaseWrite::Yes) {
            m_pruneTimer = makeUnique<RunLoop::Timer>(RunLoop::current(), this, &IconDatabase::pruneTimerFired);
            m_pruneTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
        }

        if (!createTablesIfNeeded())
            populatePageURLToIconURLMap();
    });
}

IconDatabase::~IconDatabase()
{
    ASSERT(isMainRunLoop());

    m_workQueue->dispatchSync([&] {
        if (m_db.isOpen()) {
            m_pruneTimer = nullptr;
            clearStatements();
            m_db.close();
        }
    });
}

bool IconDatabase::createTablesIfNeeded()
{
    ASSERT(!isMainRunLoop());

    if (m_db.tableExists("IconInfo"_s) && m_db.tableExists("IconData"_s) && m_db.tableExists("PageURL"_s) && m_db.tableExists("IconDatabaseInfo"_s))
        return false;

    if (m_allowDatabaseWrite == AllowDatabaseWrite::No) {
        m_db.close();
        return false;
    }

    m_db.clearAllTables();

    if (!m_db.executeCommand("CREATE TABLE PageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);"_s)) {
        LOG_ERROR("Could not create PageURL table in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE INDEX PageURLIndex ON PageURL (url);"_s)) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE TABLE IconInfo (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, stamp INTEGER);"_s)) {
        LOG_ERROR("Could not create IconInfo table in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE INDEX IconInfoIndex ON IconInfo (url, iconID);"_s)) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE TABLE IconData (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, data BLOB);"_s)) {
        LOG_ERROR("Could not create IconData table in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE INDEX IconDataIndex ON IconData (iconID);"_s)) {
        LOG_ERROR("Could not create PageURL index in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    if (!m_db.executeCommand("CREATE TABLE IconDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);"_s)) {
        LOG_ERROR("Could not create IconDatabaseInfo table in database (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }
    auto statement = m_db.prepareStatement("INSERT INTO IconDatabaseInfo VALUES ('Version', ?);"_s);
    if (!statement || statement->bindInt(1, currentDatabaseVersion) != SQLITE_OK || !statement->executeCommand()) {
        LOG_ERROR("Could not insert icon database version into IconDatabaseInfo table (%i) - %s", m_db.lastError(), m_db.lastErrorMsg());
        m_db.close();
        return false;
    }

    return true;
}

void IconDatabase::populatePageURLToIconURLMap()
{
    ASSERT(!isMainRunLoop());

    if (!m_db.isOpen())
        return;

    auto query = m_db.prepareStatement("SELECT PageURL.url, IconInfo.url, IconInfo.stamp FROM PageURL INNER JOIN IconInfo ON PageURL.iconID=IconInfo.iconID WHERE IconInfo.stamp > (?);"_s);
    if (!query) {
        LOG_ERROR("Unable to prepare icon url import query");
        return;
    }

    if (query->bindInt64(1, floor((WallTime::now() - notUsedIconExpirationTime).secondsSinceEpoch().seconds())) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::populatePageURLToIconURLMap: failed to bind statement: %s", m_db.lastErrorMsg());
        return;
    }

    auto result = query->step();
    Locker locker { m_pageURLToIconURLMapLock };
    while (result == SQLITE_ROW) {
        m_pageURLToIconURLMap.set(query->columnText(0), query->columnText(1));
        result = query->step();
    }

    startPruneTimer();
}

void IconDatabase::clearStatements()
{
    ASSERT(!isMainRunLoop());
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
    ASSERT(!isMainRunLoop());
    ASSERT(m_db.isOpen());

    if (!m_pruneIconsStatement) {
        auto pruneIconsStatement = m_db.prepareHeapStatement("DELETE FROM IconInfo WHERE stamp <= (?);"_s);
        if (!pruneIconsStatement) {
            LOG_ERROR("Preparing statement pruneIcons failed");
            return;
        }
        m_pruneIconsStatement = pruneIconsStatement.value().moveToUniquePtr();
    }

    if (m_pruneIconsStatement->bindInt64(1, floor((WallTime::now() - notUsedIconExpirationTime).secondsSinceEpoch().seconds())) != SQLITE_OK) {
        LOG_ERROR("FaviconDatabse::pruneTimerFired failed: %s", m_db.lastErrorMsg());
        return;
    }

    SQLiteTransaction transaction(m_db);
    transaction.begin();
    if (m_pruneIconsStatement->step() == SQLITE_DONE) {
        m_db.executeCommand("DELETE FROM IconData WHERE iconID NOT IN (SELECT iconID FROM IconInfo);"_s);
        m_db.executeCommand("DELETE FROM PageURL WHERE iconID NOT IN (SELECT iconID FROM IconInfo);"_s);
    }
    m_pruneIconsStatement->reset();

    transaction.commit();
}

void IconDatabase::startPruneTimer()
{
    ASSERT(!isMainRunLoop());

    if (!m_pruneTimer || !m_db.isOpen())
        return;

    if (m_pruneTimer->isActive())
        m_pruneTimer->stop();
    m_pruneTimer->startOneShot(10_s);
}

void IconDatabase::clearLoadedIconsTimerFired()
{
    ASSERT(isMainRunLoop());

    Locker locker { m_loadedIconsLock };
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
    ASSERT(isMainRunLoop());

    if (m_clearLoadedIconsTimer.isActive())
        return;

    m_clearLoadedIconsTimer.startOneShot(loadedIconExpirationTime);
}

std::optional<int64_t> IconDatabase::iconIDForIconURL(const String& iconURL, bool& expired)
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db.isOpen());

    if (!m_iconIDForIconURLStatement) {
        auto iconIDForIconURLStatement = m_db.prepareHeapStatement("SELECT IconInfo.iconID, IconInfo.stamp FROM IconInfo WHERE IconInfo.url = (?);"_s);
        if (!iconIDForIconURLStatement) {
            LOG_ERROR("Preparing statement iconIDForIconURL failed");
            return std::nullopt;
        }
        m_iconIDForIconURLStatement = iconIDForIconURLStatement.value().moveToUniquePtr();
    }

    if (m_iconIDForIconURLStatement->bindText(1, iconURL) != SQLITE_OK) {
        LOG_ERROR("FaviconDatabse::iconIDForIconURL failed: %s", m_db.lastErrorMsg());
        return std::nullopt;
    }

    std::optional<int64_t> result;
    if (m_iconIDForIconURLStatement->step() == SQLITE_ROW) {
        result = m_iconIDForIconURLStatement->columnInt64(0);
        expired = m_iconIDForIconURLStatement->columnInt64(1) <= floor((WallTime::now() - iconExpirationTime).secondsSinceEpoch().seconds());
    }

    m_iconIDForIconURLStatement->reset();
    return result;
}

bool IconDatabase::setIconIDForPageURL(int64_t iconID, const String& pageURL)
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db.isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_setIconIDForPageURLStatement) {
        auto setIconIDForPageURLStatement = m_db.prepareHeapStatement("INSERT INTO PageURL (url, iconID) VALUES ((?), ?);"_s);
        if (!setIconIDForPageURLStatement) {
            LOG_ERROR("Preparing statement setIconIDForPageURL failed");
            return false;
        }
        m_setIconIDForPageURLStatement = setIconIDForPageURLStatement.value().moveToUniquePtr();
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

Vector<uint8_t> IconDatabase::iconData(int64_t iconID)
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db.isOpen());

    if (!m_iconDataStatement) {
        auto iconDataStatement = m_db.prepareHeapStatement("SELECT IconData.data FROM IconData WHERE IconData.iconID = (?);"_s);
        if (!iconDataStatement) {
            LOG_ERROR("Preparing statement iconData failed");
            return { };
        }
        m_iconDataStatement = iconDataStatement.value().moveToUniquePtr();
    }

    if (m_iconDataStatement->bindInt64(1, iconID) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::iconData failed: %s", m_db.lastErrorMsg());
        return { };
    }

    auto result = m_iconDataStatement->columnBlob(0);
    m_iconDataStatement->reset();
    return result;
}

std::optional<int64_t> IconDatabase::addIcon(const String& iconURL, const Vector<uint8_t>& iconData)
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db.isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_addIconStatement) {
        auto addIconStatement = m_db.prepareHeapStatement("INSERT INTO IconInfo (url, stamp) VALUES (?, 0);"_s);
        if (!addIconStatement) {
            LOG_ERROR("Preparing statement addIcon failed");
            return std::nullopt;
        }
        m_addIconStatement = addIconStatement.value().moveToUniquePtr();
    }
    if (!m_addIconDataStatement) {
        auto addIconDataStatement = m_db.prepareHeapStatement("INSERT INTO IconData (iconID, data) VALUES (?, ?);"_s);
        if (!addIconDataStatement) {
            LOG_ERROR("Preparing statement addIconData failed");
            return std::nullopt;
        }
        m_addIconDataStatement = addIconDataStatement.value().moveToUniquePtr();
    }

    if (m_addIconStatement->bindText(1, iconURL) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::addIcon failed: %s", m_db.lastErrorMsg());
        return std::nullopt;
    }

    m_addIconStatement->step();
    m_addIconStatement->reset();

    auto iconID = m_db.lastInsertRowID();
    if (m_addIconDataStatement->bindInt64(1, iconID) != SQLITE_OK || m_addIconDataStatement->bindBlob(2, iconData) != SQLITE_OK) {
        LOG_ERROR("IconDatabase::addIcon failed: %s", m_db.lastErrorMsg());
        return std::nullopt;
    }

    m_addIconDataStatement->step();
    m_addIconDataStatement->reset();

    return iconID;
}

void IconDatabase::updateIconTimestamp(int64_t iconID, int64_t timestamp)
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db.isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_updateIconTimestampStatement) {
        auto updateIconTimestampStatement = m_db.prepareHeapStatement("UPDATE IconInfo SET stamp = ? WHERE iconID = ?;"_s);
        if (!updateIconTimestampStatement) {
            LOG_ERROR("Preparing statement updateIconTimestamp failed");
            return;
        }
        m_updateIconTimestampStatement = updateIconTimestampStatement.value().moveToUniquePtr();
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
    ASSERT(!isMainRunLoop());
    ASSERT(m_db.isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_deletePageURLsForIconStatement) {
        auto deletePageURLsForIconStatement = m_db.prepareHeapStatement("DELETE FROM PageURL WHERE PageURL.iconID = (?);"_s);
        if (!deletePageURLsForIconStatement) {
            LOG_ERROR("Preparing statement deletePageURLsForIcon failed");
            return;
        }
        m_deletePageURLsForIconStatement = deletePageURLsForIconStatement.value().moveToUniquePtr();
    }
    if (!m_deleteIconDataStatement) {
        auto deleteIconDataStatement = m_db.prepareHeapStatement("DELETE FROM IconData WHERE IconData.iconID = (?);"_s);
        if (!deleteIconDataStatement) {
            LOG_ERROR("Preparing statement deleteIcon failed");
            return;
        }
        m_deleteIconDataStatement = deleteIconDataStatement.value().moveToUniquePtr();
    }
    if (!m_deleteIconStatement) {
        auto deleteIconStatement = m_db.prepareHeapStatement("DELETE FROM IconInfo WHERE IconInfo.iconID = (?);"_s);
        if (!deleteIconStatement) {
            LOG_ERROR("Preparing statement deleteIcon failed");
            return;
        }
        m_deleteIconStatement = deleteIconStatement.value().moveToUniquePtr();
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
    ASSERT(isMainRunLoop());

    m_workQueue->dispatch([this, protectedThis = Ref { *this }, iconURL = iconURL.isolatedCopy(), pageURL = pageURL.isolatedCopy(), allowDatabaseWrite, completionHandler = WTFMove(completionHandler)]() mutable {
        bool result = false;
        bool changed = false;
        if (m_db.isOpen()) {
            bool canWriteToDatabase = m_allowDatabaseWrite == AllowDatabaseWrite::Yes && allowDatabaseWrite == AllowDatabaseWrite::Yes;
            bool expired = false;
            String cachedIconURL;
            {
                Locker locker { m_pageURLToIconURLMapLock };
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
                        Locker locker { m_pageURLToIconURLMapLock };
                        m_pageURLToIconURLMap.set(pageURL, iconURL);
                        changed = true;
                    }
                }
            } else if (!canWriteToDatabase) {
                bool foundInMemoryCache;
                {
                    Locker locker { m_loadedIconsLock };
                    foundInMemoryCache = m_loadedIcons.contains(iconURL);
                }

                if (foundInMemoryCache) {
                    result = true;
                    Locker locker { m_pageURLToIconURLMapLock };
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

void IconDatabase::loadIconForPageURL(const String& pageURL, AllowDatabaseWrite allowDatabaseWrite, CompletionHandler<void(PlatformImagePtr&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    m_workQueue->dispatch([this, protectedThis = Ref { *this }, pageURL = pageURL.isolatedCopy(), allowDatabaseWrite, timestamp = WallTime::now().secondsSinceEpoch(), completionHandler = WTFMove(completionHandler)]() mutable {
        std::optional<int64_t> iconID;
        Vector<uint8_t> iconData;
        String iconURL;
        {
            Locker locker { m_pageURLToIconURLMapLock };
            iconURL = m_pageURLToIconURLMap.get(pageURL);
        }
        if (m_db.isOpen() && !iconURL.isEmpty()) {
            bool expired;
            iconID = iconIDForIconURL(iconURL, expired);
            if (iconID) {
                Locker locker { m_loadedIconsLock };
                if (!m_loadedIcons.contains(iconURL)) {
                    iconData = this->iconData(iconID.value());
                    m_loadedIcons.set(iconURL, std::make_pair<PlatformImagePtr, MonotonicTime>(nullptr, { }));
                }
            }
            bool canWriteToDatabase = m_allowDatabaseWrite == AllowDatabaseWrite::Yes && allowDatabaseWrite == AllowDatabaseWrite::Yes;
            if (iconID && canWriteToDatabase)
                updateIconTimestamp(iconID.value(), timestamp.secondsAs<int64_t>());
        }
        startPruneTimer();
        RunLoop::main().dispatch([this, protectedThis = Ref { *this }, iconURL = WTFMove(iconURL), iconData = WTFMove(iconData), completionHandler = WTFMove(completionHandler)]() mutable {
            if (iconURL.isEmpty()) {
                completionHandler(nullptr);
                return;
            }

            auto icon = [&]() -> WebCore::PlatformImagePtr {
                Locker locker { m_loadedIconsLock };
                auto it = m_loadedIcons.find(iconURL);
                if (it != m_loadedIcons.end() && it->value.first) {
                    auto icon = it->value.first;
                    it->value.second = MonotonicTime::now();
                    startClearLoadedIconsTimer();
                    return icon;
                }

                auto addResult = m_loadedIcons.set(iconURL, std::make_pair<PlatformImagePtr, MonotonicTime>(nullptr, MonotonicTime::now()));
                if (!iconData.isEmpty()) {
                    auto image = BitmapImage::create();
                    if (image->setData(SharedBuffer::create(WTFMove(iconData)), true) < EncodedDataStatus::SizeAvailable)
                        return nullptr;

                    auto nativeImage = image->nativeImageForCurrentFrame();
                    if (!nativeImage)
                        return nullptr;

                    addResult.iterator->value.first = nativeImage->platformImage();
                }

                auto icon = addResult.iterator->value.first;
                startClearLoadedIconsTimer();
                return icon;
            }();
            completionHandler(WTFMove(icon));
        });
    });
}

String IconDatabase::iconURLForPageURL(const String& pageURL)
{
    ASSERT(isMainRunLoop());

    Locker locker { m_pageURLToIconURLMapLock };
    return m_pageURLToIconURLMap.get(pageURL);
}

void IconDatabase::setIconForPageURL(const String& iconURL, const uint8_t* iconData, size_t iconDataSize, const String& pageURL, AllowDatabaseWrite allowDatabaseWrite, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    // If database write is not allowed load the icon to cache it in memory only.
    if (m_allowDatabaseWrite == AllowDatabaseWrite::No || allowDatabaseWrite == AllowDatabaseWrite::No) {
        bool result = true;
        {
            Locker locker { m_loadedIconsLock };
            auto addResult = m_loadedIcons.set(iconURL, std::make_pair<PlatformImagePtr, MonotonicTime>(nullptr, { }));
            if (iconDataSize) {
                RefPtr<NativeImage> nativeImage;
                auto image = BitmapImage::create();
                if (image->setData(SharedBuffer::create(iconData, iconDataSize), true) >= EncodedDataStatus::SizeAvailable && (nativeImage = image->nativeImageForCurrentFrame()))
                    addResult.iterator->value.first = nativeImage->platformImage();
                else
                    result = false;
            }
        }
        startClearLoadedIconsTimer();
        m_workQueue->dispatch([this, protectedThis = Ref { *this }, result, iconURL = iconURL.isolatedCopy(), pageURL = pageURL.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            {
                Locker locker { m_pageURLToIconURLMapLock };
                m_pageURLToIconURLMap.set(pageURL, iconURL);
            }
            RunLoop::main().dispatch([result, completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(result);
            });
        });
        return;
    }

    Vector<uint8_t> data { iconData, iconDataSize };
    m_workQueue->dispatch([this, protectedThis = Ref { *this }, iconURL = iconURL.isolatedCopy(), iconData = WTFMove(data), pageURL = pageURL.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
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
                    Locker locker { m_pageURLToIconURLMapLock };
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
    ASSERT(isMainRunLoop());

    {
        Locker locker { m_loadedIconsLock };
        m_loadedIcons.clear();
    }
    m_workQueue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        {
            Locker locker { m_pageURLToIconURLMapLock };
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
