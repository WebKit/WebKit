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

static const char* databaseName = "CachedGeoposition.db";

int GeolocationPositionCache::s_instances = 0;
RefPtr<Geoposition>* GeolocationPositionCache::s_cachedPosition;
String* GeolocationPositionCache::s_databaseFile = 0;

GeolocationPositionCache::GeolocationPositionCache()
{
    if (!(s_instances++)) {
        s_cachedPosition = new RefPtr<Geoposition>;
        *s_cachedPosition = readFromDB();
    }
}

GeolocationPositionCache::~GeolocationPositionCache()
{
    if (!(--s_instances)) {
        if (*s_cachedPosition)
            writeToDB(s_cachedPosition->get());
        delete s_cachedPosition;
    }
}

void GeolocationPositionCache::setCachedPosition(Geoposition* cachedPosition)
{
    *s_cachedPosition = cachedPosition;
}

Geoposition* GeolocationPositionCache::cachedPosition()
{
    return s_cachedPosition->get();
}

void GeolocationPositionCache::setDatabasePath(const String& databasePath)
{
    if (!s_databaseFile)
        s_databaseFile = new String;
    *s_databaseFile = SQLiteFileSystem::appendDatabaseFileNameToPath(databasePath, databaseName);
    // If we don't have have a cached position, attempt to read one from the
    // DB at the new path.
    if (s_instances && !(*s_cachedPosition))
        *s_cachedPosition = readFromDB();
}

PassRefPtr<Geoposition> GeolocationPositionCache::readFromDB()
{
    SQLiteDatabase database;
    if (!s_databaseFile || !database.open(*s_databaseFile))
        return 0;

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
        return 0;

    SQLiteStatement statement(database, "SELECT * FROM CachedPosition");
    if (statement.prepare() != SQLResultOk)
        return 0;

    if (statement.step() != SQLResultRow)
        return 0;

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
    return Geoposition::create(coordinates.release(), statement.getColumnInt64(7)); // timestamp
}

void GeolocationPositionCache::writeToDB(const Geoposition* position)
{
    ASSERT(position);

    SQLiteDatabase database;
    if (!s_databaseFile || !database.open(*s_databaseFile))
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

    statement.bindDouble(1, position->coords()->latitude());
    statement.bindDouble(2, position->coords()->longitude());
    if (position->coords()->canProvideAltitude())
        statement.bindDouble(3, position->coords()->altitude());
    else
        statement.bindNull(3);
    statement.bindDouble(4, position->coords()->accuracy());
    if (position->coords()->canProvideAltitudeAccuracy())
        statement.bindDouble(5, position->coords()->altitudeAccuracy());
    else
        statement.bindNull(5);
    if (position->coords()->canProvideHeading())
        statement.bindDouble(6, position->coords()->heading());
    else
        statement.bindNull(6);
    if (position->coords()->canProvideSpeed())
        statement.bindDouble(7, position->coords()->speed());
    else
        statement.bindNull(7);
    statement.bindInt64(8, position->timestamp());
    if (!statement.executeCommand())
        return;

    transaction.commit();
}

} // namespace WebCore

#endif // ENABLE(GEOLOCATION)
