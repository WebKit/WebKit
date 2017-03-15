"use strict";

class CommonRemoteAPI {
    postJSON(path, data)
    {
        return this._asJSON(this.sendHttpRequest(path, 'POST', 'application/json', JSON.stringify(data || {})));
    }

    postJSONWithStatus(path, data)
    {
        return this._checkStatus(this.postJSON(path, data));
    }

    getJSON(path)
    {
        return this._asJSON(this.sendHttpRequest(path, 'GET', null, null));
    }

    getJSONWithStatus(path)
    {
        return this._checkStatus(this.getJSON(path));
    }

    sendHttpRequest(path, method, contentType, content)
    {
        throw 'NotImplemented';
    }

    _asJSON(promise)
    {
        return promise.then((result) => {
            try {
                return JSON.parse(result.responseText);
            } catch (error) {
                console.error(result.responseText);
                reject(result.statusCode + ', ' + error);
            }
        });
    }

    _checkStatus(promise)
    {
        return promise.then(function (content) {
            if (content['status'] != 'OK')
                throw content['status'];
            return content;
        });
    }
}

if (typeof module != 'undefined')
    module.exports.CommonRemoteAPI = CommonRemoteAPI;
