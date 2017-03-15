"use strict";

class BrowserRemoteAPI extends CommonRemoteAPI {

    sendHttpRequest(path, method, contentType, content)
    {
        console.assert(!path.startsWith('http:') && !path.startsWith('https:') && !path.startsWith('file:'));

        return new Promise((resolve, reject) => {
            Instrumentation.startMeasuringTime('Remote', 'sendHttpRequest');

            function onload() {
                Instrumentation.endMeasuringTime('Remote', 'sendHttpRequest');
                if (xhr.status != 200)
                    return resject(xhr.status);
                resolve({statusCode: xhr.status, responseText: xhr.responseText});
            };

            function onerror() {
                Instrumentation.endMeasuringTime('Remote', 'sendHttpRequest');
                reject(xhr.status);
            }

            const xhr = new XMLHttpRequest;
            xhr.onload = onload;
            xhr.onabort = onerror;
            xhr.onerror = onerror;

            xhr.open(method, path, true);
            if (contentType)
                xhr.setRequestHeader('Content-Type', contentType);
            if (content)
                xhr.send(content);
            else
                xhr.send();
        });
    }

}

const RemoteAPI = new BrowserRemoteAPI;
