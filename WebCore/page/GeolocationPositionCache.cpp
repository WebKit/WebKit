/*
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeolocationPositionCache.h"

#if ENABLE(GEOLOCATION)

#include "Geoposition.h"
#include "SQLValue.h"
#include "SQLiteDatabase.h"
#include "SQLiteFileSystem.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"

namespace WebCore {

static int numUsers = 0;

GeolocationPositionCache* GeolocationPositionCache::instance()
{
    DEFINE_STATIC_LOCAL(GeolocationPositionCache*, instance, (0));
    if (!instance)
        instance = new GeolocationPositionCache();
    return instance;
}

GeolocationPositionCache::GeolocationPositionCache()
    : m_haveReadFromDatabase(false)
{
}

void GeolocationPositionCache::addUser()
{
    ASSERT(numUsers >= 0);
    ++numUsers;
}

void GeolocationPositionCache::removeUser()
{
    if (!(--numUsers) && m_cachedPosition)
        writeToDatabase();
    ASSERT(numUsers >= 0);
}

void GeolocationPositionCache::setDatabasePath(const String& path)
{
    static const char* databaseName = "CachedGeoposition.db";
    String newFile = SQLiteFileSystem::appendDatabaseFileNameToPath(path, databaseName);
    if (m_databaseFile != newFile) {
        m_databaseFile = newFile;
        m_haveReadFromDatabase = false;
    }
}

void GeolocationPositionCache::setCachedPosition(Geoposition* cachedPosition)
{
    m_cachedPosition = cachedPosition;
}

Geoposition* GeolocationPositionCache::cachedPosition()
{
    if (!m_haveReadFromDatabase && !m_cachedPosition)
        readFromDatabase();
    return m_cachedPosition.get();
}

void GeolocationPositionCache::readFromDatabase()
{
    ASSERT(!m_haveReadFromDatabase);
    ASSERT(!m_cachedPosition);

    m_haveReadFromDatabase = true;

    SQLiteDatabase database;
    if (!database.open(m_databaseFile))
        return;

    // Create the table here, such that even if we've just created the
    // DB, the commands below should succeed.
    if (!database.executeCommand("CREATE TABLE IF NOT EXISTS CachedPosition ("
            "latitude REAL NOT NULL, "
            "longitude REAL NOT NULL, "
            "altitude REAL, "
            "accuracy REAL NOT NULL, "
            "altitudeAccuracy REAL, "
            "heading REAL, "
            "speed REAL, "
            "timestamp INTEGER NOT NULL)"))
        return;

    SQLiteStatement statement(database, "SELECT * FROM CachedPosition");
    if (statement.prepare() != SQLResultOk)
        return;

    if (statement.step() != SQLResultRow)
        return;

    bool providesAltitude = statement.getColumnValue(2).type() != SQLValue::NullValue;
    bool providesAltitudeAccuracy = statement.getColumnValue(4).type() != SQLValue::NullValue;
    bool providesHeading = statement.getColumnValue(5).type() != SQLValue::NullValue;
    bool providesSpeed = statement.getColumnValue(6).type() != SQLValue::NullValue;
    RefPtr<Coordinates> coordinates = Coordinates::create(statement.getColumnDouble(0), // latitude
                                                          statement.getColumnDouble(1), // longitude
                                                          providesAltitude, statement.getColumnDouble(2), // altitude
                                                          statement.getColumnDouble(3), // accuracy
                                                          providesAltitudeAccuracy, statement.getColumnDouble(4), // altitudeAccuracy
                                                          providesHeading, statement.getColumnDouble(5), // heading
                                                          providesSpeed, statement.getColumnDouble(6)); // speed
    m_cachedPosition = Geoposition::create(coordinates.release(), statement.getColumnInt64(7)); // timestamp
}

void GeolocationPositionCache::writeToDatabase()
{
    ASSERT(m_cachedPosition);

    SQLiteDatabase database;
    if (!database.open(m_databaseFile))
        return;

    SQLiteTransaction transaction(database);

    if (!database.executeCommand("DELETE FROM CachedPosition"))
        return;

    SQLiteStatement statement(database, "INSERT INTO CachedPosition ("
        "latitude, "
        "longitude, "
        "altitude, "
        "accuracy, "
        "altitudeAccuracy, "
        "heading, "
        "speed, "
        "timestamp) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    if (statement.prepare() != SQLResultOk)
        return;

    statement.bindDouble(1, m_cachedPosition->coords()->latitude());
    statement.bindDouble(2, m_cachedPosition->coords()->longitude());
    if (m_cachedPosition->coords()->canProvideAltitude())
        statement.bindDouble(3, m_cachedPosition->coords()->altitude());
    else
        statement.bindNull(3);
    statement.bindDouble(4, m_cachedPosition->coords()->accuracy());
    if (m_cachedPosition->coords()->canProvideAltitudeAccuracy())
        statement.bindDouble(5, m_cachedPosition->coords()->altitudeAccuracy());
    else
        statement.bindNull(5);
    if (m_cachedPosition->coords()->canProvideHeading())
        statement.bindDouble(6, m_cachedPosition->coords()->heading());
    else
        statement.bindNull(6);
    if (m_cachedPosition->coords()->canProvideSpeed())
        statement.bindDouble(7, m_cachedPosition->coords()->speed());
    else
        statement.bindNull(7);
    statement.bindInt64(8, m_cachedPosition->timestamp());
    if (!statement.executeCommand())
        return;

    transaction.commit();
}

} // namespace WebCore

#endif // ENABLE(GEOLOCATION)
