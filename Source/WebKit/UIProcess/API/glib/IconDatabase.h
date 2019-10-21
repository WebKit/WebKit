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
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class IconDatabase : public ThreadSafeRefCounted<IconDatabase> {
public:
    enum class AllowDatabaseWrite { No, Yes };

    static Ref<IconDatabase> create(const String& path, AllowDatabaseWrite allowDatabaseWrite)
    {
        return adoptRef(*new IconDatabase(path, allowDatabaseWrite));
    }

    ~IconDatabase();

    void invalidate();

    void checkIconURLAndSetPageURLIfNeeded(const String& iconURL, const String& pageURL, AllowDatabaseWrite, CompletionHandler<void(bool, bool)>&&);
    void loadIconForPageURL(const String&, AllowDatabaseWrite, CompletionHandler<void(WebCore::NativeImagePtr&&)>&&);
    String iconURLForPageURL(const String&);
    void setIconForPageURL(const String& iconURL, const unsigned char*, size_t, const String& pageURL, AllowDatabaseWrite, CompletionHandler<void(bool)>&&);
    void clear(CompletionHandler<void()>&&);

private:
    IconDatabase(const String&, AllowDatabaseWrite);

    bool createTablesIfNeeded();
    void populatePageURLToIconURLMap();
    void pruneTimerFired();
    void startPruneTimer();
    void clearStatements();
    void clearLoadedIconsTimerFired();
    void startClearLoadedIconsTimer();
    Optional<int64_t> iconIDForIconURL(const String&, bool& expired);
    bool setIconIDForPageURL(int64_t, const String&);
    Vector<char> iconData(int64_t);
    Optional<int64_t> addIcon(const String&, const Vector<char>&);
    void updateIconTimestamp(int64_t iconID, int64_t timestamp);
    void deleteIcon(int64_t);

    Ref<WorkQueue> m_workQueue;
    AllowDatabaseWrite m_allowDatabaseWrite { AllowDatabaseWrite::Yes };
    WebCore::SQLiteDatabase m_db;
    HashMap<String, String> m_pageURLToIconURLMap;
    Lock m_pageURLToIconURLMapLock;
    HashMap<String, std::pair<WebCore::NativeImagePtr, MonotonicTime>> m_loadedIcons;
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

    std::unique_ptr<RunLoop::Timer<IconDatabase>> m_pruneTimer;
    RunLoop::Timer<IconDatabase> m_clearLoadedIconsTimer;
};

} // namespace WebKit
