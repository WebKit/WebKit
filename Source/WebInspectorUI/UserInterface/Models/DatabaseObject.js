/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DatabaseObject = class DatabaseObject extends WebInspector.Object
{
    constructor(id, host, name, version)
    {
        super();

        this._id = id;
        this._host = host ? host : WebInspector.UIString("Local File");
        this._name = name;
        this._version = version;
    }

    // Public

    get id() { return this._id; }
    get host() { return this._host; }
    get name() { return this._name; }
    get version() { return this._version; }

    saveIdentityToCookie(cookie)
    {
        cookie[WebInspector.DatabaseObject.HostCookieKey] = this.host;
        cookie[WebInspector.DatabaseObject.NameCookieKey] = this.name;
    }

    getTableNames(callback)
    {
        function sortingCallback(error, names)
        {
            if (!error)
                callback(names.sort());
        }

        DatabaseAgent.getDatabaseTableNames(this._id, sortingCallback);
    }

    executeSQL(query, successCallback, errorCallback)
    {
        function queryCallback(error, columnNames, values, sqlError)
        {
            if (error) {
                errorCallback(WebInspector.UIString("An unexpected error occurred."));
                return;
            }

            if (sqlError) {
                switch (sqlError.code) {
                case SQLException.VERSION_ERR:
                    errorCallback(WebInspector.UIString("Database no longer has expected version."));
                    break;
                case SQLException.TOO_LARGE_ERR:
                    errorCallback(WebInspector.UIString("Data returned from the database is too large."));
                    break;
                default:
                    errorCallback(WebInspector.UIString("An unexpected error occurred."));
                    break;
                }
                return;
            }

            successCallback(columnNames, values);
        }

        DatabaseAgent.executeSQL(this._id, query, queryCallback);
    }
};

WebInspector.DatabaseObject.TypeIdentifier = "database";
WebInspector.DatabaseObject.HostCookieKey = "database-object-host";
WebInspector.DatabaseObject.NameCookieKey = "database-object-name";
