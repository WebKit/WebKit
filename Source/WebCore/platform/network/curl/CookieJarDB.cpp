/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CookieJarDB.h"
#include "cookieJar.h"

#include "CookieUtil.h"
#include "Logging.h"
#include "PublicSuffix.h"
#include "RegistrableDomain.h"
#include "SQLiteFileSystem.h"
#include <wtf/DateMath.h>
#include <wtf/FileSystem.h>
#include <wtf/MonotonicTime.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

#define CORRUPT_MARKER_SUFFIX "-corrupted"

// At least 50 cookies per domain (RFC6265 6.1. Limits)
#define MAX_COOKIE_PER_DOMAIN 80

#define CREATE_COOKIE_TABLE_SQL \
    "CREATE TABLE IF NOT EXISTS Cookie ("\
    "  name TEXT NOT NULL,"\
    "  value TEXT,"\
    "  domain TEXT NOT NULL,"\
    "  path TEXT NOT NULL,"\
    "  expires INTEGER NOT NULL,"\
    "  size INTEGER NOT NULL,"\
    "  session INTEGER NOT NULL,"\
    "  httponly INTEGER NOT NULL DEFAULT 0,"\
    "  secure INTEGER NOT NULL DEFAULT 0,"\
    "  lastupdated INTEGER NOT NULL DEFAULT CURRENT_TIMESTAMP, "\
    "  UNIQUE(name, domain, path));"_s
#define CREATE_DOMAIN_INDEX_SQL \
    "CREATE INDEX IF NOT EXISTS domain_index ON Cookie(domain);"_s
#define CREATE_PATH_INDEX_SQL \
    "CREATE INDEX IF NOT EXISTS path_index ON Cookie(path);"_s
#define SELECT_ALL_DOMAINS_SQL \
    "SELECT DISTINCT domain FROM Cookie;"_s
#define CHECK_EXISTS_COOKIE_SQL \
    "SELECT domain FROM Cookie WHERE ((domain = ?) OR (domain GLOB ?));"_s
#define CHECK_EXISTS_HTTPONLY_COOKIE_SQL \
    "SELECT name FROM Cookie WHERE (name = ?) AND (domain = ?) AND (path = ?) AND (httponly = 1);"_s
#define SET_COOKIE_SQL \
    "INSERT OR REPLACE INTO Cookie (name, value, domain, path, expires, size, session, httponly, secure) "\
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);"_s
#define DELETE_COOKIE_BY_NAME_DOMAIN_PATH_SQL \
    "DELETE FROM Cookie WHERE name = ? AND domain = ? AND path = ?;"_s
#define DELETE_COOKIE_BY_NAME_DOMAIN_SQL \
    "DELETE FROM Cookie WHERE name = ? AND domain = ?;"_s
#define DELETE_ALL_SESSION_COOKIE_SQL \
    "DELETE FROM Cookie WHERE session = 1;"_s
#define DELETE_COOKIES_BY_DOMAIN_SQL \
    "DELETE FROM Cookie WHERE domain = ? ;"_s
#define DELETE_COOKIES_BY_DOMAIN_EXCEPT_HTTP_ONLY_SQL \
    "DELETE FROM Cookie WHERE (domain = ?) AND (httponly = 0);"_s
#define DELETE_ALL_COOKIE_SQL \
    "DELETE FROM Cookie;"_s


// If the database schema is updated:
// - Increment schemaVersion
// - Add upgrade logic in verifySchemaVersion to migrate databases from the previous schema version
static constexpr int schemaVersion = 1;


CookieJarDB::CookieJarDB(const String& databasePath)
    : m_databasePath(databasePath)
{
}

CookieJarDB::~CookieJarDB()
{
    closeDatabase();
}

void CookieJarDB::open()
{
    if (!m_database.isOpen()) {
        checkDatabaseCorruptionAndRemoveIfNeeded();
        openDatabase();
    }
}

bool CookieJarDB::openDatabase()
{
    if (m_database.isOpen())
        return true;

    bool existsDatabaseFile = false;
    if (!isOnMemory())
        existsDatabaseFile = SQLiteFileSystem::ensureDatabaseFileExists(m_databasePath, false);

    if (existsDatabaseFile) {
        if (m_database.open(m_databasePath)) {
            if (checkDatabaseValidity())
                executeSQLStatement(m_database.prepareStatement(DELETE_ALL_SESSION_COOKIE_SQL));
            else {
                // delete database and try to re-create again
                LOG_ERROR("Cookie database validity check failed, attempting to recreate the database");
                m_database.close();
                deleteAllDatabaseFiles();
                existsDatabaseFile = false;
            }
        } else {
            LOG_ERROR("Failed to open cookie database: %s, attempting to recreate the database", m_databasePath.utf8().data());
            deleteAllDatabaseFiles();
            existsDatabaseFile = false;
        }
    }

    if (!existsDatabaseFile) {
        if (!FileSystem::makeAllDirectories(FileSystem::parentPath(m_databasePath)))
            LOG_ERROR("Unable to create the Cookie Database path %s", m_databasePath.utf8().data());

        m_database.open(m_databasePath);
    }

    if (!m_database.isOpen())
        return false;

    if (!isOnMemory() && !m_database.turnOnIncrementalAutoVacuum())
        LOG_ERROR("Unable to turn on incremental auto-vacuum (%d %s)", m_database.lastError(), m_database.lastErrorMsg());

    verifySchemaVersion();

    if (!existsDatabaseFile || !m_database.tableExists("Cookie"_s)) {
        bool ok = executeSQLStatement(m_database.prepareStatement(CREATE_COOKIE_TABLE_SQL))
            && executeSQLStatement(m_database.prepareStatement(CREATE_DOMAIN_INDEX_SQL))
            && executeSQLStatement(m_database.prepareStatement(CREATE_PATH_INDEX_SQL));

        if (!ok) {
            // give up create database at this time (all cookies on request/response are ignored)
            m_database.close();
            deleteAllDatabaseFiles();
            return false;
        }
    }

    m_database.setSynchronous(SQLiteDatabase::SyncNormal);

    // create prepared statements
    createPrepareStatement(SET_COOKIE_SQL);
    createPrepareStatement(CHECK_EXISTS_COOKIE_SQL);
    createPrepareStatement(CHECK_EXISTS_HTTPONLY_COOKIE_SQL);
    createPrepareStatement(DELETE_COOKIE_BY_NAME_DOMAIN_PATH_SQL);
    createPrepareStatement(DELETE_COOKIE_BY_NAME_DOMAIN_SQL);
    createPrepareStatement(DELETE_COOKIES_BY_DOMAIN_SQL);
    createPrepareStatement(DELETE_COOKIES_BY_DOMAIN_EXCEPT_HTTP_ONLY_SQL);

    return true;
}

void CookieJarDB::closeDatabase()
{
    if (m_database.isOpen()) {
        m_statements.clear();
        m_database.close();
    }
}

void CookieJarDB::verifySchemaVersion()
{
    if (isOnMemory())
        return;

    auto statement = m_database.prepareStatement("PRAGMA user_version"_s);
    int version = statement ? statement->columnInt(0) : 0;
    if (version == schemaVersion)
        return;

    switch (version) {
        // Placeholder for schema version upgrade logic
        // Ensure cases fall through to the next version's upgrade logic

    case 0:
        deleteAllTables();
        break;
    default:
        // This case can be reached when downgrading versions
        LOG_ERROR("Unknown cookie database version: %d", version);
        deleteAllTables();
        break;
    }

    // Update version
    executeSQLStatement(m_database.prepareStatementSlow(makeString("PRAGMA user_version=", schemaVersion)));
}

void CookieJarDB::deleteAllTables()
{
    if (!m_database.isOpen())
        return;

    m_database.clearAllTables();
}

String CookieJarDB::getCorruptionMarkerPath() const
{
    ASSERT(!isOnMemory());

    return m_databasePath + CORRUPT_MARKER_SUFFIX;
}

void CookieJarDB::flagDatabaseCorruption()
{
    if (isOnMemory())
        return;

    auto handle = FileSystem::openFile(getCorruptionMarkerPath(), FileSystem::FileOpenMode::Write);
    if (FileSystem::isHandleValid(handle))
        FileSystem::closeFile(handle);
}

bool CookieJarDB::checkDatabaseCorruptionAndRemoveIfNeeded()
{
    if (!isOnMemory() && FileSystem::fileExists(getCorruptionMarkerPath())) {
        LOG_ERROR("Detected cookie database corruption, attempting to recreate the database");
        deleteAllDatabaseFiles();
        return true;
    }

    return false;
}

bool CookieJarDB::checkSQLiteReturnCode(int code)
{
    if (!m_detectedDatabaseCorruption) {
        switch (code) {
        case SQLITE_CORRUPT:
        case SQLITE_SCHEMA:
        case SQLITE_FORMAT:
        case SQLITE_NOTADB:
            flagDatabaseCorruption();
            m_detectedDatabaseCorruption = true;
        }
    }
    return code == SQLITE_OK || code == SQLITE_DONE || code == SQLITE_ROW;
}

bool CookieJarDB::checkDatabaseValidity()
{
    ASSERT(m_database.isOpen());

    if (!m_database.tableExists("Cookie"_s))
        return false;

    auto integrity = m_database.prepareStatement("PRAGMA quick_check;"_s);
    if (!integrity) {
        LOG_ERROR("Failed to execute database integrity check");
        return false;
    }

    int resultCode = integrity->step();
    if (resultCode != SQLITE_ROW) {
        LOG_ERROR("Integrity quick_check step returned %d", resultCode);
        return false;
    }

    int columns = integrity->columnCount();
    if (columns != 1) {
        LOG_ERROR("Received %i columns performing integrity check, should be 1", columns);
        return false;
    }

    String resultText = integrity->columnText(0);

    if (resultText != "ok"_s) {
        LOG_ERROR("Cookie database integrity check failed - %s", resultText.ascii().data());
        return false;
    }

    return true;
}

void CookieJarDB::deleteAllDatabaseFiles()
{
    closeDatabase();
    if (isOnMemory())
        return;

    FileSystem::deleteFile(m_databasePath);
    FileSystem::deleteFile(getCorruptionMarkerPath());
    FileSystem::deleteFile(m_databasePath + "-shm");
    FileSystem::deleteFile(m_databasePath + "-wal");
}

bool CookieJarDB::isEnabled() const
{
    if (m_databasePath.isEmpty())
        return false;

    return (m_acceptPolicy == CookieAcceptPolicy::Always || m_acceptPolicy == CookieAcceptPolicy::OnlyFromMainDocumentDomain || m_acceptPolicy == CookieAcceptPolicy::ExclusivelyFromMainDocumentDomain);
}

bool CookieJarDB::checkCookieAcceptPolicy(const URL& firstParty, const URL& url)
{
    if (m_acceptPolicy == CookieAcceptPolicy::Always)
        return true;

    // See https://bugs.webkit.org/show_bug.cgi?id=193458#c0
    if (m_acceptPolicy != CookieAcceptPolicy::OnlyFromMainDocumentDomain && m_acceptPolicy != CookieAcceptPolicy::ExclusivelyFromMainDocumentDomain)
        return false;

    if (firstParty.host() == url.host())
        return true;

    if (RegistrableDomain(firstParty).matches(url))
        return true;

    // third-party resources can read or write cookies if they have pre-existing cookies.
    if (m_acceptPolicy == CookieAcceptPolicy::OnlyFromMainDocumentDomain && hasCookies(url))
        return true;

    return false;
}

bool CookieJarDB::hasCookies(const URL& url)
{
    String host = url.host().convertToASCIILowercase();
    if (host.isEmpty())
        return false;

#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (isPublicSuffix(host))
        return false;
#endif

    RegistrableDomain registrableDomain { url };
    auto& statement = preparedStatement(CHECK_EXISTS_COOKIE_SQL);

    if (CookieUtil::isIPAddress(host) || !host.contains('.') || registrableDomain.isEmpty()) {
        statement.bindText(1, host);
        statement.bindNull(2);
    } else {
        statement.bindText(1, registrableDomain.string());
        statement.bindText(2, makeString("*.", registrableDomain.string()));
    }

    return statement.step() == SQLITE_ROW;
}

std::optional<Vector<Cookie>> CookieJarDB::searchCookies(const URL& firstParty, const URL& requestUrl, const std::optional<bool>& httpOnly, const std::optional<bool>& secure, const std::optional<bool>& session)
{
    if (!isEnabled() || !m_database.isOpen())
        return std::nullopt;

    String requestHost = requestUrl.host().convertToASCIILowercase();
    if (requestHost.isEmpty())
        return std::nullopt;

    if (!checkCookieAcceptPolicy(firstParty, requestUrl))
        return std::nullopt;

    String requestPath = requestUrl.path().toString();
    if (requestPath.isEmpty())
        requestPath = "/"_s;

    RegistrableDomain registrableDomain { requestUrl };

    auto pstmt = m_database.prepareStatement("SELECT name, value, domain, path, expires, httponly, secure, session FROM Cookie WHERE "\
        "(NOT ((session = 0) AND (expires < ?)))"
        "AND (httponly = COALESCE(NULLIF(?, -1), httponly)) "\
        "AND (secure = COALESCE(NULLIF(?, -1), secure)) "\
        "AND (session = COALESCE(NULLIF(?, -1), session)) "\
        "AND ((domain = ?) OR (domain GLOB ?)) "\
        "ORDER BY length(path) DESC, lastupdated"_s);
    if (!pstmt)
        return std::nullopt;

    pstmt->bindInt64(1, WallTime::now().secondsSinceEpoch().milliseconds());
    pstmt->bindInt(2, httpOnly ? *httpOnly : -1);
    pstmt->bindInt(3, secure ? *secure : -1);
    pstmt->bindInt(4, session ? *session : -1);
    pstmt->bindText(5, requestHost);

    if (CookieUtil::isIPAddress(requestHost) || !requestHost.contains('.') || registrableDomain.isEmpty())
        pstmt->bindNull(6);
    else
        pstmt->bindText(6, makeString("*.", registrableDomain.string()));

    Vector<Cookie> results;

    while (pstmt->step() == SQLITE_ROW) {

        if (results.size() > MAX_COOKIE_PER_DOMAIN)
            break;

        String cookieName = pstmt->columnText(0);
        String cookieValue = pstmt->columnText(1);
        String cookieDomain = pstmt->columnText(2).convertToASCIILowercase();
        String cookiePath = pstmt->columnText(3);
        double cookieExpires = (double)pstmt->columnInt64(4);
        bool cookieHttpOnly = (pstmt->columnInt(5) == 1);
        bool cookieSecure = (pstmt->columnInt(6) == 1);
        bool cookieSession = (pstmt->columnInt(7) == 1);

        if (!CookieUtil::domainMatch(cookieDomain, requestHost))
            continue;

        // https://tools.ietf.org/html/rfc6265#section-5.1.4 "Paths and Path-Match"
        bool isPathMatched = cookiePath == requestPath
            || (requestPath.startsWith(cookiePath) && cookiePath.endsWith('/'))
            || (requestPath.startsWith(cookiePath) && (requestPath.characterAt(cookiePath.length()) == '/'));

        if (!isPathMatched)
            continue;

        Cookie cookie;
        cookie.name = cookieName;
        cookie.value = cookieValue;
        cookie.domain = cookieDomain;
        cookie.path = cookiePath;
        if (cookieExpires)
            cookie.expires = cookieExpires;
        cookie.httpOnly = cookieHttpOnly;
        cookie.secure = cookieSecure;
        cookie.session = cookieSession;
        results.append(WTFMove(cookie));
    }

    return results;
}

Vector<Cookie> CookieJarDB::getAllCookies()
{
    Vector<Cookie> result;
    if (!isEnabled() || !m_database.isOpen())
        return result;

    auto pstmt = m_database.prepareStatement("SELECT name, value, domain, path, expires, httponly, secure, session FROM Cookie"_s);
    if (!pstmt)
        return result;

    while (pstmt->step() == SQLITE_ROW) {
        Cookie cookie;
        cookie.name = pstmt->columnText(0);
        cookie.value = pstmt->columnText(1);
        cookie.domain = pstmt->columnText(2).convertToASCIILowercase();
        cookie.path = pstmt->columnText(3);
        double cookieExpires = (double)pstmt->columnInt64(4);
        if (cookieExpires)
            cookie.expires = cookieExpires;
        cookie.httpOnly = (pstmt->columnInt(5) == 1);
        cookie.secure = (pstmt->columnInt(6) == 1);
        cookie.session = (pstmt->columnInt(7) == 1);
        result.append(WTFMove(cookie));
    }
    return result;
}

bool CookieJarDB::hasHttpOnlyCookie(const String& name, const String& domain, const String& path)
{
    auto& statement = preparedStatement(CHECK_EXISTS_HTTPONLY_COOKIE_SQL);

    statement.bindText(1, name);
    statement.bindText(2, domain);
    statement.bindText(3, path);

    return statement.step() == SQLITE_ROW;
}

static bool checkSecureCookie(const Cookie& cookie)
{
    if (cookie.name.startsWith("__Secure-"_s) && !cookie.secure)
        return false;

    // Cookies for __Host must have the Secure attribute, path explicitly set to "/", and no domain attribute
    if (cookie.name.startsWith("__Host-"_s) && (!cookie.secure || cookie.path != "/"_s || !cookie.domain.isEmpty()))
        return false;

    return true;
}

bool CookieJarDB::canAcceptCookie(const Cookie& cookie, const URL& firstParty, const URL& url, CookieJarDB::Source source)
{
#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (isPublicSuffix(cookie.domain))
        return false;
#endif

    bool fromJavaScript = source == CookieJarDB::Source::Script;
    if (fromJavaScript && (cookie.httpOnly || hasHttpOnlyCookie(cookie.name, cookie.domain, cookie.path)))
        return false;

    if (!CookieUtil::domainMatch(cookie.domain, url.host().convertToASCIILowercase()))
        return false;

    if (!checkCookieAcceptPolicy(firstParty, url))
        return false;

    return true;
}

bool CookieJarDB::setCookie(const Cookie& cookie)
{
    auto expires = cookie.expires.value_or(0.0);
    if (!cookie.session && MonotonicTime::fromRawSeconds(expires / msPerSecond) <= MonotonicTime::now())
        return deleteCookieInternal(cookie.name, cookie.domain, cookie.path);

    auto& statement = preparedStatement(SET_COOKIE_SQL);

    // FIXME: We should have some eviction policy when a domain goes over MAX_COOKIE_PER_DOMAIN
    statement.bindText(1, cookie.name);
    statement.bindText(2, cookie.value);
    statement.bindText(3, cookie.domain);
    statement.bindText(4, cookie.path);
    statement.bindInt64(5, cookie.session ? 0 : static_cast<int64_t>(expires));
    statement.bindInt(6, cookie.value.length());
    statement.bindInt(7, cookie.session ? 1 : 0);
    statement.bindInt(8, cookie.httpOnly ? 1 : 0);
    statement.bindInt(9, cookie.secure ? 1 : 0);
    return checkSQLiteReturnCode(statement.step());
}

bool CookieJarDB::setCookie(const URL& firstParty, const URL& url, const String& body, CookieJarDB::Source source, std::optional<Seconds> cappedLifetime)
{
    if (!isEnabled() || !m_database.isOpen())
        return false;

    if (url.isEmpty() || body.isEmpty())
        return false;

    auto cookie = CookieUtil::parseCookieHeader(body);
    if (!cookie || (cookie->name.isEmpty() && cookie->value.isEmpty()))
        return false;

    if (!checkSecureCookie(*cookie))
        return false;

    if (cookie->domain.isEmpty())
        cookie->domain = url.host().convertToASCIILowercase();

    if (cookie->path.isEmpty())
        cookie->path = CookieUtil::defaultPathForURL(url);

    if (!canAcceptCookie(*cookie, firstParty, url, source))
        return false;

    if (cappedLifetime && cookie->expires) {
        ASSERT(*cappedLifetime >= 0_s);
        auto cappedExpires = WallTime::now() + *cappedLifetime;
        if (cappedExpires < WallTime::fromRawSeconds(*cookie->expires / msPerSecond))
            cookie->expires = cappedExpires.secondsSinceEpoch().milliseconds();
    }

    return setCookie(*cookie);
}

HashSet<String> CookieJarDB::allDomains()
{
    auto statement = m_database.prepareStatement(SELECT_ALL_DOMAINS_SQL);
    if (!statement)
        return { };

    HashSet<String> domains;
    while (statement->step() == SQLITE_ROW) {
        auto domain = statement->columnText(0);
        domains.add(domain);
    }
    return domains;
}

bool CookieJarDB::deleteCookie(const String& url, const String& name)
{
    if (!isEnabled() || !m_database.isOpen())
        return false;

    String urlCopied = String(url);
    if (urlCopied.startsWith('.'))
        urlCopied = urlCopied.substring(1);

    URL urlObj({ }, urlCopied);
    if (urlObj.isValid()) {
        String hostStr(urlObj.host().toString());
        String pathStr(urlObj.path().toString());
        return deleteCookieInternal(name, hostStr, pathStr);
    }

    return false;
}

bool CookieJarDB::deleteCookieInternal(const String& name, const String& domain, const String& path)
{
    auto& statement = preparedStatement(path.isEmpty() ? DELETE_COOKIE_BY_NAME_DOMAIN_SQL : DELETE_COOKIE_BY_NAME_DOMAIN_PATH_SQL);
    statement.bindText(1, name);
    statement.bindText(2, domain);
    if (!path.isEmpty())
        statement.bindText(3, path);
    return checkSQLiteReturnCode(statement.step());
}

bool CookieJarDB::deleteCookies(const String&)
{
    // NOT IMPLEMENTED
    // TODO: this function will be called if application calls WKCookieManagerDeleteCookiesForHostname() in WKCookieManager.h.
    return false;
}

bool CookieJarDB::deleteCookiesForHostname(const String& hostname, IncludeHttpOnlyCookies includeHttpOnlyCookies)
{
    auto& statement = preparedStatement(includeHttpOnlyCookies == IncludeHttpOnlyCookies::Yes ? DELETE_COOKIES_BY_DOMAIN_SQL : DELETE_COOKIES_BY_DOMAIN_EXCEPT_HTTP_ONLY_SQL);
    statement.bindText(1, hostname);
    return checkSQLiteReturnCode(statement.step());
}

bool CookieJarDB::deleteAllCookies()
{
    if (!isEnabled() || !m_database.isOpen())
        return false;

    return executeSQLStatement(m_database.prepareStatement(DELETE_ALL_COOKIE_SQL));
}

void CookieJarDB::createPrepareStatement(ASCIILiteral sql)
{
    auto statement = m_database.prepareHeapStatement(sql);
    ASSERT(statement);
    m_statements.add(sql, statement.value().moveToUniquePtr());
}

SQLiteStatement& CookieJarDB::preparedStatement(const String& sql)
{
    const auto& statement = m_statements.get(sql);
    ASSERT(statement);
    statement->reset();
    return *statement;
}

bool CookieJarDB::executeSQLStatement(Expected<SQLiteStatement, int>&& statement)
{
    if (!statement && !checkSQLiteReturnCode(statement.error())) {
        LOG_ERROR("Failed to prepare sql statement with error: %s", m_database.lastErrorMsg());
        return false;
    }

    int ret = statement->step();
    if (!checkSQLiteReturnCode(ret)) {
        LOG_ERROR("Failed to execute SQL error: %s", m_database.lastErrorMsg());
        return false;
    }

    return true;
}

} // namespace WebCore
