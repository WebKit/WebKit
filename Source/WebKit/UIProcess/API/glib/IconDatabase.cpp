/*
 * Copyright (C) 2019, 2025 Igalia S.L.
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
    : m_workQueue(WorkQueue::create("org.webkit.IconDatabase"_s))
    , m_allowDatabaseWrite(allowDatabaseWrite)
    , m_db(makeUniqueRef<SQLiteDatabase>())
    , m_clearLoadedIconsTimer(RunLoop::mainSingleton(), "IconDatabase::ClearLoadedIconsTimer"_s, this, &IconDatabase::clearLoadedIconsTimerFired)
{
    ASSERT(isMainRunLoop());
    m_clearLoadedIconsTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);

    // We initialize the database synchronously, it's hopefully fast enough because it makes
    // the implementation a lot simpler.
    m_workQueue->dispatchSync([&] {
        if (allowDatabaseWrite == AllowDatabaseWrite::No && (path.isNull() || !FileSystem::fileExists(path)))
            return;

        auto databaseDirectory = FileSystem::parentPath(path);
        FileSystem::makeAllDirectories(databaseDirectory);
        if (!m_db->open(path)) {
            RELEASE_LOG_ERROR(IconDatabase, "Unable to open favicon database '%s' (%i) - %s", path.utf8().data(), m_db->lastError(), m_db->lastErrorMsg());
            return;
        }

        if (m_db->tableExists("IconDatabaseInfo"_s)) {
            auto versionStatement = m_db->prepareStatement("SELECT value FROM IconDatabaseInfo WHERE key = 'Version';"_s);
            if (!versionStatement) {
                RELEASE_LOG_ERROR(IconDatabase, "Unable to prepare version statement (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
                return;
            }

            auto databaseVersionNumber = versionStatement ? versionStatement->columnInt(0) : 0;
            if (databaseVersionNumber > currentDatabaseVersion) {
                LOG(IconDatabase, "Database version number %d is greater than our current version number %d - closing the database to prevent overwriting newer versions",
                    databaseVersionNumber, currentDatabaseVersion);
                m_db->close();
                return;
            }

            if (databaseVersionNumber < currentDatabaseVersion) {
                if (m_allowDatabaseWrite == AllowDatabaseWrite::No) {
                    m_db->close();
                    return;
                }

                m_db->clearAllTables();
            }
        }

        // Reduce sqlite RAM cache size from default 2000 pages (~1.5kB per page). 3MB of cache for icon database is overkill.
        m_db->executeCommand("PRAGMA cache_size = 200;"_s);

        if (allowDatabaseWrite == AllowDatabaseWrite::Yes) {
            m_pruneTimer = makeUnique<RunLoop::Timer>(RunLoop::currentSingleton(), "IconDatabase::PruneTimer"_s, this, &IconDatabase::pruneTimerFired);
            m_pruneTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
        }

        if (!createTablesIfNeeded())
            populatePageURLToIconURLsMap();
    });
}

IconDatabase::~IconDatabase()
{
    ASSERT(isMainRunLoop());

    m_workQueue->dispatchSync([&] {
        if (m_db->isOpen()) {
            m_pruneTimer = nullptr;
            clearStatements();
            m_db->close();
        }
    });
}

bool IconDatabase::createTablesIfNeeded()
{
    ASSERT(!isMainRunLoop());

    if (m_db->tableExists("IconInfo"_s) && m_db->tableExists("IconData"_s) && m_db->tableExists("PageURL"_s) && m_db->tableExists("IconDatabaseInfo"_s))
        return false;

    if (m_allowDatabaseWrite == AllowDatabaseWrite::No) {
        m_db->close();
        return false;
    }

    m_db->clearAllTables();

    if (!m_db->executeCommand("CREATE TABLE PageURL (url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,iconID INTEGER NOT NULL ON CONFLICT FAIL);"_s)) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not create PageURL table in database (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        m_db->close();
        return false;
    }
    if (!m_db->executeCommand("CREATE INDEX PageURLIndex ON PageURL (url);"_s)) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not create PageURL index in database (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        m_db->close();
        return false;
    }
    if (!m_db->executeCommand("CREATE TABLE IconInfo (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, url TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT FAIL, stamp INTEGER);"_s)) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not create IconInfo table in database (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        m_db->close();
        return false;
    }
    if (!m_db->executeCommand("CREATE INDEX IconInfoIndex ON IconInfo (url, iconID);"_s)) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not create PageURL index in database (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        m_db->close();
        return false;
    }
    if (!m_db->executeCommand("CREATE TABLE IconData (iconID INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE ON CONFLICT REPLACE, data BLOB);"_s)) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not create IconData table in database (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        m_db->close();
        return false;
    }
    if (!m_db->executeCommand("CREATE INDEX IconDataIndex ON IconData (iconID);"_s)) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not create PageURL index in database (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        m_db->close();
        return false;
    }
    if (!m_db->executeCommand("CREATE TABLE IconDatabaseInfo (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);"_s)) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not create IconDatabaseInfo table in database (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        m_db->close();
        return false;
    }
    auto statement = m_db->prepareStatement("INSERT INTO IconDatabaseInfo VALUES ('Version', ?);"_s);
    if (!statement || statement->bindInt(1, currentDatabaseVersion) != SQLITE_OK || !statement->executeCommand()) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not insert icon database version into IconDatabaseInfo table (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        m_db->close();
        return false;
    }

    return true;
}

void IconDatabase::populatePageURLToIconURLsMap()
{
    ASSERT(!isMainRunLoop());

    if (!m_db->isOpen())
        return;

    auto query = m_db->prepareStatement("SELECT PageURL.url, IconInfo.url, IconInfo.stamp FROM PageURL INNER JOIN IconInfo ON PageURL.iconID=IconInfo.iconID WHERE IconInfo.stamp > (?);"_s);
    if (!query) {
        RELEASE_LOG_ERROR(IconDatabase, "Unable to prepare icon URL import query (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        return;
    }

    if (query->bindInt64(1, floor((WallTime::now() - notUsedIconExpirationTime).secondsSinceEpoch().seconds())) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind timestamp (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        return;
    }

    auto result = query->step();
    Locker locker { m_pageURLToIconURLMapLock };
    while (result == SQLITE_ROW) {
        auto iconURLs = m_pageURLToIconURLMap.ensure(query->columnText(0), []() {
            return ListHashSet<String> { };
        });
        iconURLs.iterator->value.add(query->columnText(1));
        result = query->step();
    }

    startPruneTimer();
}

void IconDatabase::clearStatements()
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db->isOpen());

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
    ASSERT(m_db->isOpen());

    if (!m_pruneIconsStatement) {
        m_pruneIconsStatement = m_db->prepareStatement("DELETE FROM IconInfo WHERE stamp <= (?);"_s);
        if (!m_pruneIconsStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement pruneIcons failed: %s", m_db->lastErrorMsg());
            return;
        }
    }

    if (m_pruneIconsStatement->bindInt64(1, floor((WallTime::now() - notUsedIconExpirationTime).secondsSinceEpoch().seconds())) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind timestamp: %s", m_db->lastErrorMsg());
        return;
    }

    SQLiteTransaction transaction(m_db);
    transaction.begin();
    if (m_pruneIconsStatement->step() == SQLITE_DONE) {
        m_db->executeCommand("DELETE FROM IconData WHERE iconID NOT IN (SELECT iconID FROM IconInfo);"_s);
        m_db->executeCommand("DELETE FROM PageURL WHERE iconID NOT IN (SELECT iconID FROM IconInfo);"_s);
    }
    m_pruneIconsStatement->reset();

    transaction.commit();
}

void IconDatabase::startPruneTimer()
{
    ASSERT(!isMainRunLoop());

    if (!m_pruneTimer || !m_db->isOpen())
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
    ASSERT(m_db->isOpen());

    if (!m_iconIDForIconURLStatement) {
        m_iconIDForIconURLStatement = m_db->prepareStatement("SELECT IconInfo.iconID, IconInfo.stamp FROM IconInfo WHERE IconInfo.url = (?);"_s);
        if (!m_iconIDForIconURLStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement iconIDForIconURL failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return std::nullopt;
        }
    }

    if (m_iconIDForIconURLStatement->bindText(1, iconURL) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind iconURL (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
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
    ASSERT(m_db->isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_setIconIDForPageURLStatement) {
        m_setIconIDForPageURLStatement = m_db->prepareStatement("INSERT INTO PageURL (url, iconID) VALUES ((?), ?);"_s);
        if (!m_setIconIDForPageURLStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement setIconIDForPageURL failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return false;
        }
    }

    if (m_setIconIDForPageURLStatement->bindText(1, pageURL) != SQLITE_OK
        || m_setIconIDForPageURLStatement->bindInt64(2, iconID) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind pageURL or iconID (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
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
    ASSERT(m_db->isOpen());

    if (!m_iconDataStatement) {
        m_iconDataStatement = m_db->prepareStatement("SELECT IconData.data FROM IconData WHERE IconData.iconID = (?);"_s);
        if (!m_iconDataStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement iconData failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return { };
        }
    }

    if (m_iconDataStatement->bindInt64(1, iconID) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind iconID (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        return { };
    }

    auto result = m_iconDataStatement->columnBlob(0);
    m_iconDataStatement->reset();
    return result;
}

std::optional<int64_t> IconDatabase::addIcon(const String& iconURL, const Vector<uint8_t>& iconData)
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db->isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_addIconStatement) {
        m_addIconStatement = m_db->prepareStatement("INSERT INTO IconInfo (url, stamp) VALUES (?, 0);"_s);
        if (!m_addIconStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement addIcon failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return std::nullopt;
        }
    }
    if (!m_addIconDataStatement) {
        m_addIconDataStatement = m_db->prepareStatement("INSERT INTO IconData (iconID, data) VALUES (?, ?);"_s);
        if (!m_addIconDataStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement addIconData failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return std::nullopt;
        }
    }

    if (m_addIconStatement->bindText(1, iconURL) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind iconURL (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        return std::nullopt;
    }

    m_addIconStatement->step();
    m_addIconStatement->reset();

    auto iconID = m_db->lastInsertRowID();
    if (m_addIconDataStatement->bindInt64(1, iconID) != SQLITE_OK || m_addIconDataStatement->bindBlob(2, iconData) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind iconID (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        return std::nullopt;
    }

    m_addIconDataStatement->step();
    m_addIconDataStatement->reset();

    return iconID;
}

void IconDatabase::updateIconTimestamp(int64_t iconID, int64_t timestamp)
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db->isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_updateIconTimestampStatement) {
        m_updateIconTimestampStatement = m_db->prepareStatement("UPDATE IconInfo SET stamp = ? WHERE iconID = ?;"_s);
        if (!m_updateIconTimestampStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement updateIconTimestamp failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return;
        }
    }

    if (m_updateIconTimestampStatement->bindInt64(1, timestamp) != SQLITE_OK || m_updateIconTimestampStatement->bindInt64(2, iconID) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind timestamp or iconID (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
        return;
    }

    m_updateIconTimestampStatement->step();
    m_updateIconTimestampStatement->reset();
}

void IconDatabase::deleteIcon(int64_t iconID)
{
    ASSERT(!isMainRunLoop());
    ASSERT(m_db->isOpen());
    ASSERT(m_allowDatabaseWrite == AllowDatabaseWrite::Yes);

    if (!m_deletePageURLsForIconStatement) {
        m_deletePageURLsForIconStatement = m_db->prepareStatement("DELETE FROM PageURL WHERE PageURL.iconID = (?);"_s);
        if (!m_deletePageURLsForIconStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement deletePageURLsForIcon failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return;
        }
    }
    if (!m_deleteIconDataStatement) {
        m_deleteIconDataStatement = m_db->prepareStatement("DELETE FROM IconData WHERE IconData.iconID = (?);"_s);
        if (!m_deleteIconDataStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement deleteIcon failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return;
        }
    }
    if (!m_deleteIconStatement) {
        m_deleteIconStatement = m_db->prepareStatement("DELETE FROM IconInfo WHERE IconInfo.iconID = (?);"_s);
        if (!m_deleteIconStatement) {
            RELEASE_LOG_ERROR(IconDatabase, "Preparing statement deleteIcon failed (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
            return;
        }
    }

    if (m_deletePageURLsForIconStatement->bindInt64(1, iconID) != SQLITE_OK
        || m_deleteIconDataStatement->bindInt64(1, iconID) != SQLITE_OK
        || m_deleteIconStatement->bindInt64(1, iconID) != SQLITE_OK) {
        RELEASE_LOG_ERROR(IconDatabase, "Could not bind iconID (%i) - %s", m_db->lastError(), m_db->lastErrorMsg());
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

    m_workQueue->dispatch([this, protectedThis = Ref { *this }, iconURL = iconURL.isolatedCopy(), pageURL = pageURL.isolatedCopy(), allowDatabaseWrite, completionHandler = WTF::move(completionHandler)]() mutable {
        bool result = false;
        bool changed = false;
        if (m_db->isOpen()) {
            bool canWriteToDatabase = m_allowDatabaseWrite == AllowDatabaseWrite::Yes && allowDatabaseWrite == AllowDatabaseWrite::Yes;
            bool expired = false;
            ListHashSet<String> cachedIconURLs;
            {
                Locker locker { m_pageURLToIconURLMapLock };
                cachedIconURLs = m_pageURLToIconURLMap.get(pageURL);
            }
            if (cachedIconURLs.contains(iconURL))
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
                        auto iconURLs = m_pageURLToIconURLMap.ensure(pageURL, []() {
                            return ListHashSet<String> { };
                        });
                        iconURLs.iterator->value.add(iconURL);
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
                    auto iconURLs = m_pageURLToIconURLMap.ensure(pageURL, []() {
                        return ListHashSet<String> { };
                    });
                    iconURLs.iterator->value.add(iconURL);
                    changed = true;
                }
            }
        }
        startPruneTimer();
        RunLoop::mainSingleton().dispatch([result, changed, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(result, changed);
        });
    });
}

void IconDatabase::loadIconsForPageURL(const String& pageURL, AllowDatabaseWrite allowDatabaseWrite, CompletionHandler<void(Vector<PlatformImagePtr>&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    m_workQueue->dispatch([this, protectedThis = Ref { *this }, pageURL = pageURL.isolatedCopy(), allowDatabaseWrite, timestamp = WallTime::now().secondsSinceEpoch(), completionHandler = WTF::move(completionHandler)]() mutable {
        ListHashSet<String> iconURLs;
        {
            Locker locker { m_pageURLToIconURLMapLock };
            iconURLs = m_pageURLToIconURLMap.get(pageURL);
        }

        Vector<Vector<uint8_t>> iconDatas(iconURLs.size());

        if (m_db->isOpen() && !iconURLs.isEmpty()) {
            unsigned iconDataIndex = 0;
            for (const auto& iconURL : iconURLs) {
                bool expired;
                std::optional<int64_t> iconID = iconIDForIconURL(iconURL, expired);
                if (iconID) {
                    Locker locker { m_loadedIconsLock };
                    if (!m_loadedIcons.contains(iconURL)) {
                        iconDatas[iconDataIndex] = this->iconData(iconID.value());
                        m_loadedIcons.set(iconURL, std::make_pair<PlatformImagePtr, MonotonicTime>(nullptr, { }));
                    }
                }
                bool canWriteToDatabase = m_allowDatabaseWrite == AllowDatabaseWrite::Yes && allowDatabaseWrite == AllowDatabaseWrite::Yes;
                if (iconID && canWriteToDatabase)
                    updateIconTimestamp(iconID.value(), timestamp.secondsAs<int64_t>());
                ++iconDataIndex;
            }
        }
        RELEASE_ASSERT(iconDatas.size() == iconURLs.size());

        startPruneTimer();
        RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, iconURLs = WTF::move(iconURLs), iconDatas = WTF::move(iconDatas), completionHandler = WTF::move(completionHandler)]() mutable {
            RELEASE_ASSERT(iconURLs.size() == iconDatas.size());

            Vector<PlatformImagePtr> icons;
            icons.reserveCapacity(iconURLs.size());
            unsigned iconDataIndex = 0;
            for (const auto& iconURL : iconURLs) {
                auto iconData = std::exchange(iconDatas[iconDataIndex++], { });

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
                        if (image->setData(SharedBuffer::create(WTF::move(iconData)), true) < EncodedDataStatus::SizeAvailable)
                            return nullptr;

                        auto nativeImage = image->currentNativeImage();
                        if (!nativeImage)
                            return nullptr;

                        addResult.iterator->value.first = nativeImage->platformImage();
                    }

                    auto icon = addResult.iterator->value.first;
                    startClearLoadedIconsTimer();
                    return icon;
                }();
                if (icon)
                    icons.append(WTF::move(icon));
            }
            completionHandler(WTF::move(icons));
        });
    });
}

ListHashSet<String> IconDatabase::iconURLsForPageURL(const String& pageURL)
{
    ASSERT(isMainRunLoop());

    Locker locker { m_pageURLToIconURLMapLock };
    return m_pageURLToIconURLMap.get(pageURL);
}

void IconDatabase::setIconForPageURL(const String& iconURL, std::span<const uint8_t> iconData, const String& pageURL, AllowDatabaseWrite allowDatabaseWrite, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    // If database write is not allowed load the icon to cache it in memory only.
    if (m_allowDatabaseWrite == AllowDatabaseWrite::No || allowDatabaseWrite == AllowDatabaseWrite::No) {
        bool result = true;
        {
            Locker locker { m_loadedIconsLock };
            auto addResult = m_loadedIcons.set(iconURL, std::make_pair<PlatformImagePtr, MonotonicTime>(nullptr, { }));
            if (iconData.size()) {
                RefPtr<NativeImage> nativeImage;
                Ref image = BitmapImage::create();
                if (image->setData(SharedBuffer::create(iconData), true) >= EncodedDataStatus::SizeAvailable && (nativeImage = image->currentNativeImage()))
                    addResult.iterator->value.first = nativeImage->platformImage();
                else
                    result = false;
            }
        }
        startClearLoadedIconsTimer();
        m_workQueue->dispatch([this, protectedThis = Ref { *this }, result, iconURL = iconURL.isolatedCopy(), pageURL = pageURL.isolatedCopy(), completionHandler = WTF::move(completionHandler)]() mutable {
            {
                Locker locker { m_pageURLToIconURLMapLock };
                auto iconURLs = m_pageURLToIconURLMap.ensure(pageURL, []() {
                    return ListHashSet<String> { };
                });
                iconURLs.iterator->value.add(iconURL);
            }
            RunLoop::mainSingleton().dispatch([result, completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(result);
            });
        });
        return;
    }

    m_workQueue->dispatch([this, protectedThis = Ref { *this }, iconURL = iconURL.isolatedCopy(), iconData = Vector(iconData), pageURL = pageURL.isolatedCopy(), completionHandler = WTF::move(completionHandler)]() mutable {
        bool result = false;
        if (m_db->isOpen()) {
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
                    auto iconURLs = m_pageURLToIconURLMap.ensure(pageURL, []() {
                        return ListHashSet<String> { };
                    });
                    iconURLs.iterator->value.add(iconURL);
                }
            }

            transaction.commit();
        }
        startPruneTimer();
        RunLoop::mainSingleton().dispatch([result, completionHandler = WTF::move(completionHandler)]() mutable {
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
    m_workQueue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        {
            Locker locker { m_pageURLToIconURLMapLock };
            m_pageURLToIconURLMap.clear();
        }

        if (m_db->isOpen() && m_allowDatabaseWrite == AllowDatabaseWrite::Yes) {
            clearStatements();
            m_db->clearAllTables();
            m_db->runVacuumCommand();
            createTablesIfNeeded();
        }

        RunLoop::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

} // namespace WebKit
