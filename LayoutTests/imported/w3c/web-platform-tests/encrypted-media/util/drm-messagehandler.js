(function(){
// Expect utf8decoder and utf8decoder to be TextEncoder('utf-8') and TextDecoder('utf-8') respectively
//
// drmconfig format:
// { <keysystem> : {    "serverURL"             : <the url for the server>,
//                      "httpRequestHeaders"    : <map of HTTP request headers>,
//                      "servertype"            : "microsoft" | "drmtoday",                 // affects how request parameters are formed
//                      "certificate"           : <base64 encoded server certificate> } }
//

drmtodaysecret = Uint8Array.from( [144, 34, 109, 76, 134, 7, 97, 107, 98, 251, 140, 28, 98, 79, 153, 222, 231, 245, 154, 226, 193, 1, 213, 207, 152, 204, 144, 15, 13, 2, 37, 236] );

drmconfig = {
    "com.widevine.alpha": [ {
        "serverURL": "https://lic.staging.drmtoday.com/license-proxy-widevine/cenc/",
        "servertype" : "drmtoday",
        "merchant" : "w3c-eme-test",
        "secret" : drmtodaysecret
    } ],
    "com.microsoft.playready": [ {
        "serverURL": "http://playready-testserver.azurewebsites.net/rightsmanager.asmx",
        "servertype": "microsoft",
        "sessionTypes" : [ "persistent-usage-record" ],
        "certificate" : "Q0hBSQAAAAEAAAUEAAAAAAAAAAJDRVJUAAAAAQAAAfQAAAFkAAEAAQAAAFjt9G6KdSncCkrjbTQPN+/2AAAAAAAAAAAAAAAJIPbrW9dj0qydQFIomYFHOwbhGZVGP2ZsPwcvjh+NFkP/////AAAAAAAAAAAAAAAAAAAAAAABAAoAAABYxw6TjIuUUmvdCcl00t4RBAAAADpodHRwOi8vcGxheXJlYWR5LmRpcmVjdHRhcHMubmV0L3ByL3N2Yy9yaWdodHNtYW5hZ2VyLmFzbXgAAAAAAQAFAAAADAAAAAAAAQAGAAAAXAAAAAEAAQIAAAAAADBRmRRpqV4cfRLcWz9WoXIGZ5qzD9xxJe0CSI2mXJQdPHEFZltrTkZtdmurwVaEI2etJY0OesCeOCzCqmEtTkcAAAABAAAAAgAAAAcAAAA8AAAAAAAAAAVEVEFQAAAAAAAAABVNZXRlcmluZyBDZXJ0aWZpY2F0ZQAAAAAAAAABAAAAAAABAAgAAACQAAEAQGHic/IPbmLCKXxc/MH20X/RtjhXH4jfowBWsQE1QWgUUBPFId7HH65YuQJ5fxbQJCT6Hw0iHqKzaTkefrhIpOoAAAIAW+uRUsdaChtq/AMUI4qPlK2Bi4bwOyjJcSQWz16LAFfwibn5yHVDEgNA4cQ9lt3kS4drx7pCC+FR/YLlHBAV7ENFUlQAAAABAAAC/AAAAmwAAQABAAAAWMk5Z0ovo2X0b2C9K5PbFX8AAAAAAAAAAAAAAARTYd1EkpFovPAZUjOj2doDLnHiRSfYc89Fs7gosBfar/////8AAAAAAAAAAAAAAAAAAAAAAAEABQAAAAwAAAAAAAEABgAAAGAAAAABAAECAAAAAABb65FSx1oKG2r8AxQjio+UrYGLhvA7KMlxJBbPXosAV/CJufnIdUMSA0DhxD2W3eRLh2vHukIL4VH9guUcEBXsAAAAAgAAAAEAAAAMAAAABwAAAZgAAAAAAAAAgE1pY3Jvc29mdAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgFBsYXlSZWFkeSBTTDAgTWV0ZXJpbmcgUm9vdCBDQQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgDEuMC4wLjEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEACAAAAJAAAQBArAKJsEIDWNG5ulOgLvSUb8I2zZ0c5lZGYvpIO56Z0UNk/uC4Mq3jwXQUUN6m/48V5J/vuLDhWu740aRQc1dDDAAAAgCGTWHP8iVuQixWizwoABz7PhUnZYWEugUht5sYKNk23h2Cao/D5uf6epDVyilG8fZKLvufXc/+fkNOtEKT+sWr"
    },
    {
        "serverURL": "http://playready.directtaps.net/pr/svc/rightsmanager.asmx",
        "servertype": "microsoft",
        "sessionTypes" : [ "persistent-usage-record" ],
        "certificate" : "Q0hBSQAAAAEAAAUEAAAAAAAAAAJDRVJUAAAAAQAAAfQAAAFkAAEAAQAAAFjt9G6KdSncCkrjbTQPN+/2AAAAAAAAAAAAAAAJIPbrW9dj0qydQFIomYFHOwbhGZVGP2ZsPwcvjh+NFkP/////AAAAAAAAAAAAAAAAAAAAAAABAAoAAABYxw6TjIuUUmvdCcl00t4RBAAAADpodHRwOi8vcGxheXJlYWR5LmRpcmVjdHRhcHMubmV0L3ByL3N2Yy9yaWdodHNtYW5hZ2VyLmFzbXgAAAAAAQAFAAAADAAAAAAAAQAGAAAAXAAAAAEAAQIAAAAAADBRmRRpqV4cfRLcWz9WoXIGZ5qzD9xxJe0CSI2mXJQdPHEFZltrTkZtdmurwVaEI2etJY0OesCeOCzCqmEtTkcAAAABAAAAAgAAAAcAAAA8AAAAAAAAAAVEVEFQAAAAAAAAABVNZXRlcmluZyBDZXJ0aWZpY2F0ZQAAAAAAAAABAAAAAAABAAgAAACQAAEAQGHic/IPbmLCKXxc/MH20X/RtjhXH4jfowBWsQE1QWgUUBPFId7HH65YuQJ5fxbQJCT6Hw0iHqKzaTkefrhIpOoAAAIAW+uRUsdaChtq/AMUI4qPlK2Bi4bwOyjJcSQWz16LAFfwibn5yHVDEgNA4cQ9lt3kS4drx7pCC+FR/YLlHBAV7ENFUlQAAAABAAAC/AAAAmwAAQABAAAAWMk5Z0ovo2X0b2C9K5PbFX8AAAAAAAAAAAAAAARTYd1EkpFovPAZUjOj2doDLnHiRSfYc89Fs7gosBfar/////8AAAAAAAAAAAAAAAAAAAAAAAEABQAAAAwAAAAAAAEABgAAAGAAAAABAAECAAAAAABb65FSx1oKG2r8AxQjio+UrYGLhvA7KMlxJBbPXosAV/CJufnIdUMSA0DhxD2W3eRLh2vHukIL4VH9guUcEBXsAAAAAgAAAAEAAAAMAAAABwAAAZgAAAAAAAAAgE1pY3Jvc29mdAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgFBsYXlSZWFkeSBTTDAgTWV0ZXJpbmcgUm9vdCBDQQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgDEuMC4wLjEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEACAAAAJAAAQBArAKJsEIDWNG5ulOgLvSUb8I2zZ0c5lZGYvpIO56Z0UNk/uC4Mq3jwXQUUN6m/48V5J/vuLDhWu740aRQc1dDDAAAAgCGTWHP8iVuQixWizwoABz7PhUnZYWEugUht5sYKNk23h2Cao/D5uf6epDVyilG8fZKLvufXc/+fkNOtEKT+sWr"
    },
    {
        "serverURL": "https://lic.staging.drmtoday.com/license-proxy-headerauth/drmtoday/RightsManager.asmx",
        "servertype" : "drmtoday",
        "sessionTypes" : [ "temporary", "persistent-usage-record", "persistent-license" ],
        "merchant" : "w3c-eme-test",
        "secret" : drmtodaysecret
    } ],
    "com.apple.fps": [ {
        "serverURL": (location.protocol === "https:" ? "https://127.0.0.1:8443" : "http://127.0.0.1:8000") + "/media/fairplay/resources/index.py",
        "servertype" : "fps",
        "sessionTypes" : [ "temporary", "persistent-usage-record", "persistent-license" ],
        "certificate" : "MIIEzzCCA7egAwIBAgIIAbMPMUN04ogwDQYJKoZIhvcNAQEFBQAwfzELMAkGA1UEBhMCVVMxEzARBgNVBAoMCkFwcGxlIEluYy4xJjAkBgNVBAsMHUFwcGxlIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MTMwMQYDVQQDDCpBcHBsZSBLZXkgU2VydmljZXMgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMjEwOTEzMjAzMzU1WhcNMjMwOTE0MjAzMzU0WjB4MQswCQYDVQQGEwJVUzEfMB0GA1UECgwWQXBwbGUgSW5jLiAtIENvcmVNZWRpYTETMBEGA1UECwwKR0NUNVk2OUJVVDEzMDEGA1UEAwwqRmFpclBsYXkgU3RyZWFtaW5nOiBBcHBsZSBJbmMuIC0gQ29yZU1lZGlhMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDKOSWKpii2sqlJeg0TRHZJF3aocb4j8K7RDmmii2vwrcv2ECiRTnGZp3/giNyj74Ty0/hjf71ck7ldJryHi7iO5dUVjatjmF5giRdyjAPp9dVxj2nMDyQORcCPzGgDTJ8BNt2oM1tOant+/3J9jDcWw3vjWeZURX7QH5+gpQuC0QIDAQABo4IB2DCCAdQwDAYDVR0TAQH/BAIwADAfBgNVHSMEGDAWgBRj5EdUy4VxWUYsg6zMRDFkZwMsvjCB4gYDVR0gBIHaMIHXMIHUBgkqhkiG92NkBQEwgcYwgcMGCCsGAQUFBwICMIG2DIGzUmVsaWFuY2Ugb24gdGhpcyBjZXJ0aWZpY2F0ZSBieSBhbnkgcGFydHkgYXNzdW1lcyBhY2NlcHRhbmNlIG9mIHRoZSB0aGVuIGFwcGxpY2FibGUgc3RhbmRhcmQgdGVybXMgYW5kIGNvbmRpdGlvbnMgb2YgdXNlLCBjZXJ0aWZpY2F0ZSBwb2xpY3kgYW5kIGNlcnRpZmljYXRpb24gcHJhY3RpY2Ugc3RhdGVtZW50cy4wNQYDVR0fBC4wLDAqoCigJoYkaHR0cDovL2NybC5hcHBsZS5jb20va2V5c2VydmljZXMuY3JsMB0GA1UdDgQWBBRH6yDSZZCJE1gcuNnWiRBLVZmypzAOBgNVHQ8BAf8EBAMCBSAwKQYLKoZIhvdjZAYNAQMBAf8EFwFxbG9kZHF0ZnFmb3hoZGQ2c2RwY2p3MC0GCyqGSIb3Y2QGDQEEAQH/BBsBamNsaGdiZ3Jjbm52aHdycXlxa20zZWpxZmswDQYJKoZIhvcNAQEFBQADggEBALd/pptUeufI5wFe1kYbQ790XL3LXWJN77n3kEEFBVZAtyrLtsGsE7JvB3X+6CL3JkneaCL5XRr2lbCp+vw0mFtwTxrRlnsOAhLyESffGfrO+0UXJM/v3mB3dQrE0tzGVoXZdXQO3ldWd/sYWYJxRLLc3wKJNMzbbIttaBLEsOWYzPP5tekoGCGz9WpsKBK6cm3jVac0BW13SbmF13Q8rwFebgmZ2buUVwDYiJNk8WhquNYk+Jw98O3u0ttalcFtirAwlDfxIlYApI1Ag+J5dy+iMi0Ho7K5Vf90Dkgb+M2Lb6RDrbGjN41RozTloHWgPptiQJChK9xRiTpkj6jDBjM="
    } ]
};


var keySystemWrappers = {
    // Key System wrappers map messages and pass to a handler, then map the response and return to caller
    //
    // function wrapper(handler, messageType, message, params)
    //
    // where:
    //      Promise<response> handler(messageType, message, responseType, headers, params);
    //

    'com.widevine.alpha': function(handler, messageType, message, params) {
        return handler.call(this, messageType, new Uint8Array(message), 'json', null, params).then(function(response){
            return base64DecodeToUnit8Array(response.license);
        });
    },

    'com.microsoft.playready': function(handler, messageType, message, params) {
        var msg, xmlDoc;
        var licenseRequest = null;
        var headers = {};
        var parser = new DOMParser();
        var dataview = new Uint16Array(message);

        msg = String.fromCharCode.apply(null, dataview);
        xmlDoc = parser.parseFromString(msg, 'application/xml');

        if (xmlDoc.getElementsByTagName('Challenge')[0]) {
            var challenge = xmlDoc.getElementsByTagName('Challenge')[0].childNodes[0].nodeValue;
            if (challenge) {
                licenseRequest = atob(challenge);
            }
        }

        var headerNameList = xmlDoc.getElementsByTagName('name');
        var headerValueList = xmlDoc.getElementsByTagName('value');
        for (var i = 0; i < headerNameList.length; i++) {
            headers[headerNameList[i].childNodes[0].nodeValue] = headerValueList[i].childNodes[0].nodeValue;
        }
        // some versions of the PlayReady CDM return 'Content' instead of 'Content-Type',
        // but the license server expects 'Content-Type', so we fix it up here.
        if (headers.hasOwnProperty('Content')) {
            headers['Content-Type'] = headers.Content;
            delete headers.Content;
        }

        return handler.call(this, messageType, licenseRequest, 'arraybuffer', headers, params).catch(function(response){
            return response.text().then( function( error ) { throw error; } );
        });
    },

    'com.apple.fps': function(handler, messageType, message, params) {
        // FairPlay has two initData types: `sinf` and `cenc`, but the initDataType
        // is not passed into this function. `cenc` initData is an encoded JSON object
        // while `sinf` is a binary `sinf` atom. Support both by attempting to JSON.parse
        // for the `cenc` case and falling back to `sinf` if that method throws.
        let payload;
        let keyIDs
        try {
            let messageString = utf8decoder.decode(message);
            let messageObject = JSON.parse(messageString);
            payload = messageObject.map(item => item.payload)
            keyIDs = messageObject.map(item => item.keyID)

            return handler.call(this, messageType, payload, 'json', { 'Content-type': 'application/x-www-form-urlencoded' }, params).then(function(response){
                let responses = response["fairplay-streaming-response"]["streaming-keys"].map((item, i) => ({ keyID: keyIDs[i], payload: item.ckc }));
                return stringToUint8Array(JSON.stringify(responses));
            });

        } catch(e) {
            payload = [base64Encode(new Uint8Array(message))];

            return handler.call(this, messageType, payload, 'json', { 'Content-type': 'application/x-www-form-urlencoded' }, params).then(function(response){
                return base64DecodeToUnit8Array(response["fairplay-streaming-response"]["streaming-keys"][0].ckc);
            });
        }
    },
};

const requestConstructors = {
    // Server request construction functions
    //
    // Promise<request> constructRequest(config, sessionType, content, messageType, message, params)
    //
    // request = { url: ..., headers: ..., body: ... }
    //
    // content = { assetId: ..., variantId: ..., key: ... }
    // params = { expiration: ... }

    'drmtoday': function(config, sessionType, content, messageType, message, headers, params) {
        var optData = JSON.stringify({merchant: config.merchant, userId:"12345", sessionId:""});
        var crt = {};
        if (messageType === 'license-request') {
            crt = {assetId: content.assetId,
                    outputProtection: {digital : false, analogue: false, enforce: false},
                    storeLicense: (sessionType === 'persistent-license')};

            if (!params || (params.expiration === undefined && params.playDuration === undefined)) {
                crt.profile = {purchase: {}};
            } else {
                var expiration = params.expiration || (Date.now().valueOf() + 3600000),
                    playDuration = params.playDuration || 3600000;

                crt.profile = {rental: {absoluteExpiration: (new Date(expiration)).toISOString(),
                                        playDuration: playDuration } };
            }

            if (content.variantId !== undefined) {
                crt.variantId = content.variantId;
            }
        }

        return JWT.encode("HS256", {optData: optData, crt: JSON.stringify([crt])}, config.secret).then(function(jwt){
            headers = headers || {};
            headers['x-dt-auth-token'] = jwt;
            return {url: config.serverURL, headers: headers, body: message};
        });
    },

    'microsoft': function(config, sessionType, content, messageType, message, headers, params) {
        var url = config.serverURL;
        if (messageType === 'license-request') {
            url += "?";
            if (sessionType === 'temporary' || sessionType === 'persistent-usage-record') {
                url += "UseSimpleNonPersistentLicense=1&";
            }
            if (sessionType === 'persistent-usage-record') {
                url += "SecureStop=1&";
            }
            url += "PlayEnablers=B621D91F-EDCC-4035-8D4B-DC71760D43E9&";    // disable output protection
            url += "ContentKey=" + btoa(String.fromCharCode.apply(null, content.key));
            return url;
        }

        // TODO: Include expiration time in URL
        return Promise.resolve({url: url, headers: headers, body: message});
    },

    'fps': function(config, sessionType, content, messageType, message, headers, params) {
        let url = config.serverURL;
        let keys = message.map(item => { return {
            "id" : 1,
            "uri" : `skd://${content.assetId}`,
            "spc" : item,
        }});
        let request = {
            "fairplay-streaming-request" : {
                "version" : 1,
                "streaming-keys" : keys,
            }
        };
        if (params?.playDuration > 0)
            request["fairplay-streaming-request"]["streaming-keys"][0]["playbackDuration"] = params.playDuration / 1000;
        let body = JSON.stringify(request);
        return Promise.resolve({url: url, headers: headers, body: body});
    }
};

MessageHandler = function(keysystem, content, sessionType) {
    sessionType = sessionType || "temporary";

    this._keysystem = keysystem;
    this._content = content;
    this._sessionType = sessionType;
    try {
        this._drmconfig = drmconfig[this._keysystem].filter(function(drmconfig) {
            return drmconfig.sessionTypes === undefined || (drmconfig.sessionTypes.indexOf(sessionType) !== -1);
        })[0];
        this._requestConstructor = requestConstructors[this._drmconfig.servertype];

        this.messagehandler = keySystemWrappers[keysystem].bind(this, MessageHandler.prototype.messagehandler);

        if (this._drmconfig && this._drmconfig.certificate) {
            this.servercertificate = stringToUint8Array(atob(this._drmconfig.certificate));
        }
    } catch(e) {
        return null;
    }
}

MessageHandler.prototype.messagehandler = function messagehandler(messageType, message, responseType, headers, params) {

    var variantId = params ? params.variantId : undefined;
    var key, kid;
    if( variantId ) {
        var keys = this._content.keys.filter(function(k){return k.variantId === variantId;});
        if (keys[0]) { key = keys[0].key; kid = keys[0].kid; }
    }
    if (!key) {
        key = this._content.keys[0].key;
        kid = this._content.keys[0].kid;
    }

    var content = {assetId:    this._content.assetId,
                    variantId:  variantId,
                    key:        key,
                    kid:        kid};

    return this._requestConstructor(this._drmconfig, this._sessionType, content, messageType, message, headers, params).then(function(request){
        return fetch(request.url, {
                        method:     'POST',
                        headers:    request.headers,
                        body:       request.body    });
    }).then(function(fetchresponse){
        if(fetchresponse.status !== 200) {
            throw fetchresponse;
        }

        if(responseType === 'json') {
            return fetchresponse.json();
        } else if(responseType === 'arraybuffer') {
            return fetchresponse.arrayBuffer();
        }
    });
}

})();

(function() {

    var subtlecrypto = window.crypto.subtle;

    // Encoding / decoding utilities
    function b64pad(b64)        { return b64+"==".substr(0,(b64.length%4)?(4-b64.length%4):0); }
    function str2b64url(str)    { return btoa(str).replace(/=+$/g, '').replace(/\+/g, "-").replace(/\//g, "_"); }
    function b64url2str(b64)    { return atob(b64pad(b64.replace(/\-/g, "+").replace(/\_/g, "/"))); }
    function str2ab(str)        { return Uint8Array.from( str.split(''), function(s){return s.charCodeAt(0)} ); }
    function ab2str(ab)         { return String.fromCharCode.apply(null, new Uint8Array(ab)); }

    function jwt2webcrypto(alg) {
        if (alg === "HS256") return {name: "HMAC", hash: "SHA-256", length: 256};
        else if (alg === "HS384") return { name: "HMAC", hash: "SHA-384", length: 384};
        else if (alg === "HS512") return { name: "HMAC", hash: "SHA-512", length: 512};
        else throw new Error("Unrecognized JWT algorithm: " + alg);
    }

    JWT = {
        encode: function encode(alg, claims, secret) {
            var algorithm = jwt2webcrypto(alg);
            if (secret.byteLength !== algorithm.length / 8) throw new Error("Unexpected secret length: " + secret.byteLength);

            if (!claims.iat) claims.iat = ((Date.now() / 1000) | 0) - 60;
            if (!claims.jti) {
                var nonce = new Uint8Array(16);
                window.crypto.getRandomValues(nonce);
                claims.jti = str2b64url( ab2str(nonce) );
            }

            var header = {typ: "JWT", alg: alg};
            var plaintext = str2b64url(JSON.stringify(header)) + '.' + str2b64url(JSON.stringify(claims));
            return subtlecrypto.importKey("raw", secret, algorithm, false, [ "sign" ]).then( function(key) {
                return subtlecrypto.sign(algorithm, key, str2ab(plaintext));
            }).then(function(hmac){
                return plaintext + '.' + str2b64url(ab2str(hmac));
            });
        },

        decode: function decode(jwt, secret) {
            var jwtparts = jwt.split('.');
            var header = JSON.parse( b64url2str(jwtparts[0]));
            var claims = JSON.parse( b64url2str(jwtparts[1]));
            var hmac = str2ab(b64url2str(jwtparts[2]));
            var algorithm = jwt2webcrypto(header.alg);
            if (secret.byteLength !== algorithm.length / 8) throw new Error("Unexpected secret length: " + secret.byteLength);

            return subtlecrypto.importKey("raw", secret, algorithm, false, ["sign", "verify"]).then(function(key) {
                return subtlecrypto.verify(algorithm, key, hmac, str2ab(jwtparts[0] + '.' + jwtparts[1]));
            }).then(function(success){
                if (!success) throw new Error("Invalid signature");
                return claims;
            });
        }
    };
})();
