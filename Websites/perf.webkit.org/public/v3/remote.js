"use strict";

class BrowserRemoteAPI extends CommonRemoteAPI {

    url(path)
    {
        if (path.charAt(0) != '/')
            path = '/' + path;
        return `${location.protocol}//${location.host}${path}`;
    }

    sendHttpRequest(path, method, contentType, content, options = {})
    {
        console.assert(!path.startsWith('http:') && !path.startsWith('https:') && !path.startsWith('file:'));

        return new Promise((resolve, reject) => {
            Instrumentation.startMeasuringTime('Remote', 'sendHttpRequest');

            function onload() {
                Instrumentation.endMeasuringTime('Remote', 'sendHttpRequest');
                if (xhr.status != 200)
                    return reject(xhr.status);
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

            if (content && options.uploadProgressCallback) {
                xhr.upload.onprogress = (event) => {
                    options.uploadProgressCallback(event.lengthComputable ? {total: event.total, loaded: event.loaded} : null);
                }
            }

            xhr.open(method, path, true);
            if (contentType)
                xhr.setRequestHeader('Content-Type', contentType);
            if (content)
                xhr.send(content);
            else
                xhr.send();
        });
    }

    sendHttpRequestWithFormData(path, formData, options)
    {
        return this.sendHttpRequest(path, 'POST', null, formData, options); // Content-type is set by the browser.
    }

}

const RemoteAPI = new BrowserRemoteAPI;
