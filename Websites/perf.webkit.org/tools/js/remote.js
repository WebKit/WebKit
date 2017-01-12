'use strict';

let assert = require('assert');
let http = require('http');
let https = require('https');
let querystring = require('querystring');

class RemoteAPI {
    constructor(server)
    {
        this._server = null;
        this._cookies = new Map;
        if (server)
            this.configure(server);
    }

    url(path)
    {
        let scheme = this._server.scheme;
        let port = this._server.port;
        let portSuffix = this._server.port == this._server.defaultPort ? '' : `:${port}`;
        if (path.charAt(0) != '/')
            path = '/' + path;
        return `${scheme}://${this._server.host}${portSuffix}${path}`;
    }

    configure(server)
    {
        assert(server.scheme === 'http' || server.scheme === 'https');
        assert.equal(typeof(server.host), 'string', 'host should be a string');
        assert(!server.port || typeof(server.port) == 'number', 'port should be a number');

        let auth = null;
        if (server.auth) {
            assert.equal(typeof(server.auth), 'object', 'auth should be a dictionary with username and password as keys');
            assert.equal(typeof(server.auth.username), 'string', 'auth should contain a string username');
            assert.equal(typeof(server.auth.password), 'string', 'auth should contain a string password');
            auth = {
                username: server.auth.username,
                password: server.auth.password,
            };
        }

        const defaultPort = server.scheme == 'http' ? 80 : 443;
        this._server = {
            scheme: server.scheme,
            host: server.host,
            port: server.port || defaultPort,
            defaultPort: defaultPort,
            auth: auth,
        };
    }

    clearCookies() { this._cookies = new Map; }

    getJSON(path)
    {
        return this.sendHttpRequest(path, 'GET', null, null).then(function (result) {
            try {
                return JSON.parse(result.responseText);
            } catch (error) {
                console.error(result.responseText);
                throw error;
            }
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
        const payload = JSON.stringify(data || {});
        return this.sendHttpRequest(path, 'POST', 'application/json', payload).then(function (result) {
            try {
                return JSON.parse(result.responseText);
            } catch (error) {
                console.error(result.responseText);
                throw error;
            }
        });
    }

    postJSONWithStatus(path, data)
    {
        return this.postJSON(path, data).then(function (result) {
            if (result['status'] != 'OK')
                return Promise.reject(result);
            return result;
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
        const self = this;
        return new Promise(function (resolve, reject) {
            let options = {
                hostname: server.host,
                port: server.port,
                auth: server.auth ? server.auth.username + ':' + server.auth.password : null,
                method: method,
                path: path,
            };

            let request = (server.scheme == 'http' ? http : https).request(options, function (response) {
                let responseText = '';
                response.setEncoding('utf8');
                response.on('data', function (chunk) { responseText += chunk; });
                response.on('end', function () {
                    if (response.statusCode != 200) {
                        reject(response.statusCode);
                        return;
                    }

                    if ('set-cookie' in response.headers) {
                        for (const cookie of response.headers['set-cookie']) {
                            var nameValue = cookie.split('=')
                            self._cookies.set(nameValue[0], nameValue[1]);
                        }
                    }
                    resolve({statusCode: response.statusCode, responseText: responseText});
                });
            });

            request.on('error', reject);

            if (contentType)
                request.setHeader('Content-Type', contentType);

            if (self._cookies.size) {
                request.setHeader('Cookie', Array.from(self._cookies.keys()).map(function (key) {
                    return `${key}=${self._cookies.get(key)}`;
                }).join('; '));
            }

            if (content)
                request.write(content);

            request.end();
        });
    }
};

if (typeof module != 'undefined')
    module.exports.RemoteAPI = RemoteAPI;
