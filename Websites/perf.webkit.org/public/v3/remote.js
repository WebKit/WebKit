"use strict";

var RemoteAPI = {};

RemoteAPI.postJSON = function (path, data)
{
    return this.getJSON(path, data || {});
}

RemoteAPI.postJSONWithStatus = function (path, data)
{
    console.log(document.cookie);
    return this.getJSONWithStatus(path, data || {});
}

RemoteAPI.getJSON = function(path, data)
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

RemoteAPI.getJSONWithStatus = function(path, data)
{
    return this.getJSON(path, data).then(function (content) {
        if (content['status'] != 'OK')
            return Promise.reject(content['status']);
        return content;
    });
}
