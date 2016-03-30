'use strict';

let assert = require('assert');
let http = require('http');
let https = require('https');

let RemoteAPI = new (class RemoteAPI {
    constructor()
    {
        this._server = {
            scheme: 'http',
            host: 'localhost',
        }
    }

    configure(server)
    {
        assert(server.scheme === 'http' || server.scheme === 'https');
        assert.equal(typeof(server.host), 'string');
        assert(!server.port || typeof(server.port) == 'number');
        assert(!server.auth || typeof(server.auth) == 'object');
        this._server = server;
    }

    getJSON(path, data)
    {
        let contentType = null;
        if (data) {
            contentType = 'application/json';
            data = JSON.stringify(data);
        }
        return this.sendHttpRequest(path, 'GET', contentType, data).then(function (result) {
            return JSON.parse(result.responseText);
        });
    }

    getJSONWithStatus(path, data)
    {
        return this.getJSON(path, data).then(function (result) {
            if (result['status'] != 'OK')
                return Promise.reject(result);
            return result;
        });
    }

    sendHttpRequest(path, method, contentType, content)
    {
        let server = this._server;
        return new Promise(function (resolve, reject) {
            let options = {
                hostname: server.host,
                port: server.port || 80,
                auth: server.auth,
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
})

if (typeof module != 'undefined')
    module.exports.RemoteAPI = RemoteAPI;
