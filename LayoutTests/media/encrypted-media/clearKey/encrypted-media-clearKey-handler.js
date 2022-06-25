function Base64ToHex(str)
{
    let bin = window.atob(str.replace(/-/g, "+").replace(/_/g, "/"));
    let res = "";
    for (let i = 0; i < bin.length; i++) {
        res += ("0" + bin.charCodeAt(i).toString(16)).substr(-2);
    }
    return res;
}

function HexToBase64(hex)
{
    let bin = "";
    for (let i = 0; i < hex.length; i += 2) {
        bin += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
    }
    return window.btoa(bin).replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}

function stringToArray(s)
{
    let array = new Uint8Array(s.length);
    for (let i = 0; i < s.length; i++) {
        array[i] = s.charCodeAt(i);
    }
    return array;
}

function EncryptedMediaHandler(video, videoConf, audioConf)
{
    if (!navigator.requestMediaKeySystemAccess) {
        logResult(false, "EME API is not supported");
        return;
    } else {
        logResult(true, "EME API is supported");
    }

    this.video = video;
    this.keys = videoConf.keys;
    this.audioConf = null;
    if (audioConf) {
        for (let attrname in audioConf.keys) {
            this.keys[attrname] =  audioConf.keys[attrname];
        }
        this.audioConf = audioConf;
    }
    this.videoConf = videoConf;
    this.sessions = [];
    this.setMediaKeyPromise;
    waitForEventOn(video, "encrypted", this.onEncrypted.bind(this));
    return this;
};

EncryptedMediaHandler.prototype = {

    onEncrypted : function(event)
    {
        let self = this;
        let initData = event.initData;
        let initDataType = event.initDataType;
        let eventVideo = event.target;

        if (!this.setMediaKeyPromise) {
            let options = [
                {    initDataTypes: [self.videoConf.initDataType],
                     videoCapabilities: [{contentType : self.videoConf.mimeType}] }
            ];

            if (self.audioConf) {
                options.audioCapabilities = [{contentType : self.audioConf.mimeType}];
            }

            this.setMediaKeyPromise = navigator.requestMediaKeySystemAccess("org.w3.clearkey", options).then(function(keySystemAccess) {
                return keySystemAccess.createMediaKeys();
            }).then(function(mediaKeys) {
                logResult(true, "MediaKeys is created");
                return eventVideo.setMediaKeys(mediaKeys);
            });
        }

        this.setMediaKeyPromise.then(function() {
            let session = eventVideo.mediaKeys.createSession();
            self.sessions.push(session);
            waitForEventOn(session, "message", self.onMessage.bind(self));
            waitForEventOn(session, "keystatuseschange", self.onKeyStatusChange.bind(self));
            session.generateRequest(initDataType, initData);
        });

        this.setMediaKeyPromise.catch(function(error){
            logResult(false, "setMediaKeys is failed");
        });
    },

    onMessage : function(event)
    {
        let session = event.target;
        let msgStr = String.fromCharCode.apply(String, new Uint8Array(event.message));
        let msg = JSON.parse(msgStr);
        let outKeys = [];
        let keys = this.keys;

        for (let i = 0; i < msg.kids.length; i++) {
            let id64 = msg.kids[i];
            let idHex = Base64ToHex(msg.kids[i]).toLowerCase();
            let key = keys[idHex];
            if (key) {
                outKeys.push({
                    "kty":"oct",
                    "alg":"A128KW",
                    "kid":id64,
                    "k":HexToBase64(key)
                });
            }
        }
        let update = JSON.stringify({
                        "keys" : outKeys,
                        "type" : msg.type
                     });

        session.update(stringToArray(update).buffer);
    },

    onKeyStatusChange : function(event)
    {
        let session = event.target;
        let keysStatus = session.keyStatuses;
        for (let key of keysStatus.entries()) {
            let keyId = key[0];
            let status = key[1];
            let base64KId = Base64ToHex(window.btoa(String.fromCharCode.apply(String,new Uint8Array(keyId))));
            logResult(true,"Session:" + " keyId=" + base64KId + " status=" + status);
        }
    }
};
