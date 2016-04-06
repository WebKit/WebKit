'use strict';

let assert = require('assert');
let http = require('http');
let https = require('https');
let querystring = require('querystring');

class RemoteAPI {
    constructor(server)
    {
        this._server = null;
        if (server)
            this.configure(server);
    }

    url(path)
    {
        let scheme = this._server.scheme;
        let port = this._server.port;
        let portSuffix = (scheme == 'http' && port == 80) || (scheme == 'https' && port == 443) ? '' : `:${port}`;
        if (path.charAt(0) != '/')
            path = '/' + path;
        return `${scheme}://${this._server.host}portSuffix${path}`;
    }

    configure(server)
    {
        assert(server.scheme === 'http' || server.scheme === 'https');
        assert.equal(typeof(server.host), 'string');
        assert(!server.port || typeof(server.port) == 'number');
        assert(!server.auth || typeof(server.auth) == 'object');
        this._server = server;
    }

    getJSON(path)
    {
        return this.sendHttpRequest(path, 'GET', null, null).then(function (result) {
            return JSON.parse(result.responseText);
        });
    }

    getJSONWithStatus(path)
    {
        return this.getJSON(path).then(function (result) {
            if (result['status'] != 'OK')
                return Promise.reject(result);
            return result;
        });
    }

    postJSON(path, data)
    {
        const contentType = 'application/json';
        const payload = JSON.stringify(data);
        return this.sendHttpRequest(path, 'POST', 'application/json', payload).then(function (result) {
            return JSON.parse(result.responseText);
        });
    }

    postFormUrlencodedData(path, data)
    {
        const contentType = 'application/x-www-form-urlencoded';
        const payload = querystring.stringify(data);
        return this.sendHttpRequest(path, 'POST', contentType, payload).then(function (result) {
            return result.responseText;
        });
    }

    sendHttpRequest(path, method, contentType, content)
    {
        let server = this._server;
        return new Promise(function (resolve, reject) {
            let options = {
                hostname: server.host,
                port: server.port || 80,
                auth: server.auth ? server.auth.username + ':' + server.auth.password : null,
                method: method,
                path: path,
            };

            let request = (server.scheme == 'http' ? http : https).request(options, function (response) {
                let responseText = '';
                response.setEncoding('utf8');
                response.on('data', function (chunk) { responseText += chunk; });
                response.on('end', function () { resolve({statusCode: response.statusCode, responseText: responseText}); });
            });

            request.on('error', reject);

            if (contentType)
                request.setHeader('Content-Type', contentType);

            if (content)
                request.write(content);

            request.end();
        });
    }
};

if (typeof module != 'undefined')
    module.exports.RemoteAPI = RemoteAPI;
