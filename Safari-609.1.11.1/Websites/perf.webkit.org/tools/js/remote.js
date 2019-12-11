'use strict';

const assert = require('assert');
const http = require('http');
const https = require('https');
const querystring = require('querystring');
const CommonRemoteAPI = require('../../public/shared/common-remote.js').CommonRemoteAPI;

global.FormData = require('form-data');

class NodeRemoteAPI extends CommonRemoteAPI {
    constructor(server)
    {
        super();
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

    postFormUrlencodedData(path, data)
    {
        const contentType = 'application/x-www-form-urlencoded';
        const payload = querystring.stringify(data);
        return this.sendHttpRequest(path, 'POST', contentType, payload).then(function (result) {
            return result.responseText;
        });
    }

    sendHttpRequest(path, method, contentType, content, requestOptions = {})
    {
        let server = this._server;
        return new Promise((resolve, reject) => {
            let options = {
                hostname: server.host,
                port: server.port,
                auth: server.auth ? server.auth.username + ':' + server.auth.password : null,
                method: method,
                path: path,
            };

            let request = (server.scheme == 'http' ? http : https).request(options, (response) => {
                let responseText = '';
                if (requestOptions.responseHandler)
                    requestOptions.responseHandler(response);
                else {
                    response.setEncoding('utf8');
                    response.on('data', (chunk) => { responseText += chunk; });
                }
                response.on('end', () => {
                    if (response.statusCode < 200 || response.statusCode >= 300)
                        return reject(response.statusCode);

                    if ('set-cookie' in response.headers) {
                        for (let cookie of response.headers['set-cookie']) {
                            const nameValue = cookie.split('=');
                            this._cookies.set(nameValue[0], nameValue[1]);
                        }
                    }
                    resolve({statusCode: response.statusCode, responseText: responseText, headers: response.headers});
                });
            });

            request.on('error', reject);

            if (contentType)
                request.setHeader('Content-Type', contentType);

            if (this._cookies.size)
                request.setHeader('Cookie', Array.from(this._cookies.keys()).map((key) => `${key}=${this._cookies.get(key)}`).join('; '));

            if (requestOptions.headers) {
                const requestHeaders = requestOptions.headers;
                for (let headerName in requestHeaders)
                    request.setHeader(headerName, requestHeaders[headerName]);
            }

            if (content instanceof Function)
                content(request);
            else {
                if (content)
                    request.write(content);
                request.end();
            }
        });
    }

    sendHttpRequestWithFormData(path, formData, options)
    {
        return this.sendHttpRequest(path, 'POST', `multipart/form-data; boundary=${formData.getBoundary()}`, (request) => {
            formData.pipe(request);
        }, options);
    }

};

if (typeof module != 'undefined')
    module.exports.RemoteAPI = NodeRemoteAPI;
