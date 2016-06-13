"use strict";

// FIXME: Use real class syntax once the dependency on data.js has been removed.
var PrivilegedAPI = class {

    static sendRequest(path, data)
    {
        var clonedData = {};
        for (var key in data)
            clonedData[key] = data[key];

        return this.requestCSRFToken().then(function (token) {
            clonedData['token'] = token;
            return RemoteAPI.postJSONWithStatus('/privileged-api/' + path, clonedData).catch(function (status) {
                if (status != 'InvalidToken')
                    return Promise.reject(status);
                PrivilegedAPI._token = null;
                return PrivilegedAPI.requestCSRFToken().then(function (token) {
                    clonedData['token'] = token;
                    return RemoteAPI.postJSONWithStatus('/privileged-api/' + path, clonedData);
                });
            });
        });
    }

    static requestCSRFToken()
    {
        var maxNetworkLatency = 3 * 60 * 1000; /* 3 minutes */
        if (this._token && this._expiration > Date.now() + maxNetworkLatency)
            return Promise.resolve(this._token);

        return RemoteAPI.postJSONWithStatus('/privileged-api/generate-csrf-token').then(function (result) {
            PrivilegedAPI._token = result['token'];
            PrivilegedAPI._expiration = new Date(result['expiration']);
            return PrivilegedAPI._token;
        });
    }

}

PrivilegedAPI._token = null;
PrivilegedAPI._expiration = null;

if (typeof module != 'undefined')
    module.exports.PrivilegedAPI = PrivilegedAPI;
