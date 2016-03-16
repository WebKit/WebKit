
function getJSON(path, data)
{
    console.assert(!path.startsWith('http:') && !path.startsWith('https:') && !path.startsWith('file:'));

    return new Promise(function (resolve, reject) {
        Instrumentation.startMeasuringTime('Remote', 'getJSON');

        var xhr = new XMLHttpRequest;
        xhr.onload = function () {
            Instrumentation.endMeasuringTime('Remote', 'getJSON');

            if (xhr.status != 200) {
                reject(xhr.status);
                return;
            }

            try {
                var parsed = JSON.parse(xhr.responseText);
                resolve(parsed);
            } catch (error) {
                console.error(xhr.responseText);
                reject(xhr.status + ', ' + error);
            }
        };

        function onerror() {
            Instrumentation.endMeasuringTime('Remote', 'getJSON');
            reject(xhr.status);
        }

        xhr.onabort = onerror;
        xhr.onerror = onerror;

        if (data) {
            xhr.open('POST', path, true);
            xhr.setRequestHeader('Content-Type', 'application/json');
            xhr.send(JSON.stringify(data));
        } else {
            xhr.open('GET', path, true);
            xhr.send();
        }
    });
}

function getJSONWithStatus(path, data)
{
    return getJSON(path, data).then(function (content) {
        if (content['status'] != 'OK')
            return Promise.reject(content['status']);
        return content;
    });
}

// FIXME: Use real class syntax once the dependency on data.js has been removed.
PrivilegedAPI = class {

    static sendRequest(path, data)
    {
        return this.requestCSRFToken().then(function (token) {
            var clonedData = {};
            for (var key in data)
                clonedData[key] = data[key];
            clonedData['token'] = token;
            return getJSONWithStatus('../privileged-api/' + path, clonedData);
        });
    }

    static requestCSRFToken()
    {
        var maxNetworkLatency = 3 * 60 * 1000; /* 3 minutes */
        if (this._token && this._expiration > Date.now() + maxNetworkLatency)
            return Promise.resolve(this._token);

        return getJSONWithStatus('../privileged-api/generate-csrf-token', {}).then(function (result) {
            PrivilegedAPI._token = result['token'];
            PrivilegedAPI._expiration = new Date(result['expiration']);
            return PrivilegedAPI._token;
        });
    }

}

PrivilegedAPI._token = null;
PrivilegedAPI._expiration = null;
