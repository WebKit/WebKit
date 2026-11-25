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

#pragma once

#include <WebCore/NativeImage.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class IconDatabase : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<IconDatabase> {
public:
    enum class AllowDatabaseWrite : bool { No, Yes };

    static Ref<IconDatabase> create(StringImpl* path, AllowDatabaseWrite allowDatabaseWrite)
    {
        return adoptRef(*new IconDatabase(path, allowDatabaseWrite));
    }

    ~IconDatabase();

    void invalidate();

    void checkIconURLAndSetPageURLIfNeeded(StringImpl* iconURL, StringImpl* pageURL, AllowDatabaseWrite, CompletionHandler<void(bool, bool)>&&);
    void loadIconsForPageURL(StringImpl*, AllowDatabaseWrite, CompletionHandler<void(Vector<WebCore::PlatformImagePtr>&&)>&&);
    ListHashSet<RefPtr<StringImpl>> iconURLsForPageURL(StringImpl*);
    void setIconForPageURL(StringImpl* iconURL, std::span<const uint8_t>, StringImpl* pageURL, AllowDatabaseWrite, CompletionHandler<void(bool)>&&);
    void clear(CompletionHandler<void()>&&);

private:
    IconDatabase(StringImpl*, AllowDatabaseWrite);

    bool createTablesIfNeeded();
    void populatePageURLToIconURLsMap();
    void pruneTimerFired();
    void startPruneTimer();
    void clearStatements();
    void clearLoadedIconsTimerFired();
    void startClearLoadedIconsTimer();
    std::optional<int64_t> iconIDForIconURL(StringImpl*, bool& expired);
    bool setIconIDForPageURL(int64_t, StringImpl*);
    Vector<uint8_t> iconData(int64_t);
    std::optional<int64_t> addIcon(StringImpl*, const Vector<uint8_t>&);
    void updateIconTimestamp(int64_t iconID, int64_t timestamp);
    void deleteIcon(int64_t);

    const Ref<WorkQueue> m_workQueue;
    AllowDatabaseWrite m_allowDatabaseWrite { AllowDatabaseWrite::Yes };
    const UniqueRef<WebCore::SQLiteDatabase> m_db;
    HashMap<RefPtr<StringImpl>, ListHashSet<RefPtr<StringImpl>>> m_pageURLToIconURLMap;
    Lock m_pageURLToIconURLMapLock;
    HashMap<RefPtr<StringImpl>, std::pair<WebCore::PlatformImagePtr, MonotonicTime>> m_loadedIcons WTF_GUARDED_BY_LOCK(m_loadedIconsLock);
    Lock m_loadedIconsLock;

    std::unique_ptr<WebCore::SQLiteStatement> m_iconIDForIconURLStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_setIconIDForPageURLStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_iconDataStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_addIconStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_addIconDataStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_updateIconTimestampStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_deletePageURLsForIconStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_deleteIconDataStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_deleteIconStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_pruneIconsStatement;

    std::unique_ptr<RunLoop::Timer> m_pruneTimer;
    RunLoop::Timer m_clearLoadedIconsTimer;
};

} // namespace WebKit
