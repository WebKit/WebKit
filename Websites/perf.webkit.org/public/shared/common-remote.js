"use strict";

class CommonRemoteAPI {
    postJSON(path, data, options)
    {
        return this._asJSON(this.sendHttpRequest(path, 'POST', 'application/json', JSON.stringify(data || {}), options));
    }

    postJSONWithStatus(path, data, options)
    {
        return this._checkStatus(this.postJSON(path, data, options));
    }

    postFormData(path, data, options)
    {
        const formData = new FormData();
        for (let key in data)
            formData.append(key, data[key]);
        return this._asJSON(this.sendHttpRequestWithFormData(path, formData, options));
    }

    postFormDataWithStatus(path, data, options)
    {
        return this._checkStatus(this.postFormData(path, data, options));
    }

    getJSON(path)
    {
        return this._asJSON(this.sendHttpRequest(path, 'GET', null, null));
    }

    getJSONWithStatus(path)
    {
        return this._checkStatus(this.getJSON(path));
    }

    sendHttpRequest(path, method, contentType, content, options = {})
    {
        throw 'NotImplemented';
    }

    sendHttpRequestWithFormData(path, formData, options = {})
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
                throw `{result.statusCode}: ${error}`;
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
