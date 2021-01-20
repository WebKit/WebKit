"use strict";

class PrivilegedAPI {

    static sendRequest(path, data, options = {useFormData: false})
    {
        const clonedData = {};
        for (let key in data)
            clonedData[key] = data[key];

        const fullPath = '/privileged-api/' + path;
        const post = options.useFormData
            ? () => RemoteAPI.postFormDataWithStatus(fullPath, clonedData, options)
            : () => RemoteAPI.postJSONWithStatus(fullPath, clonedData, options);

        return this.requestCSRFToken().then((token) => {
            clonedData['token'] = token;
            return post().catch((status) => {
                if (status != 'InvalidToken')
                    return Promise.reject(status);
                this._token = null;
                return this.requestCSRFToken().then((token) => {
                    clonedData['token'] = token;
                    return post();
                });
            });
        });
    }

    static requestCSRFToken()
    {
        const maxNetworkLatency = 3 * 60 * 1000; /* 3 minutes */
        if (this._token && this._expiration > Date.now() + maxNetworkLatency)
            return Promise.resolve(this._token);

        return RemoteAPI.postJSONWithStatus('/privileged-api/generate-csrf-token').then((result) => {
            this._token = result['token'];
            this._expiration = new Date(result['expiration']);
            return this._token;
        });
    }

}

PrivilegedAPI._token = null;
PrivilegedAPI._expiration = null;

if (typeof module != 'undefined')
    module.exports.PrivilegedAPI = PrivilegedAPI;
