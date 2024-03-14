const asymmetricUsage = ['sign', 'verify'];
const symmetricUsage = ['decrypt', 'encrypt'];
const dbName = "cryptoKeyWrapUnwrap";
const objstoreName = "cryptoKeys";

async function createKeys() {
    const extractable = true;
    var keys = {
        "AES-GCM": await self.crypto.subtle.generateKey({name: 'AES-GCM', length: 256}, extractable, symmetricUsage),
        "AES-CBC": await self.crypto.subtle.generateKey({name: "AES-CBC", length: 256}, extractable, symmetricUsage),
        "ECDSA": await self.crypto.subtle.generateKey({name: "ECDSA", namedCurve: "P-384"}, extractable, asymmetricUsage),
        "HMAC": await self.crypto.subtle.generateKey({name: "HMAC", hash: {name: "SHA-512"}, }, extractable, asymmetricUsage),
    };
    return keys;
}

function openDB() {
    return new Promise(
        function (resolve, reject) {
            try {
                var request = self.indexedDB.open(dbName, 1);
                request.onsuccess = () => {resolve(request.result);}
                request.onerror = (event) => {reject(event.target.error.name);}
                request.onupgradeneeded = (event) => {event.target.result.createObjectStore(objstoreName, {keyPath: "id"});}
            } catch (error) {
                reject(error)
            }
        });
}

var db;
async function addValue(key, value) {
    return new Promise(
        function (resolve, reject) {
            try {
                var transaction = db.transaction([objstoreName], "readwrite");
                var objectStore = transaction.objectStore(objstoreName);
                var objectStoreRequest = objectStore.put({'obj': value, 'id': key});
                objectStoreRequest.onerror = function () {
                    reject(Error("ErrorCompletingPut"));
                }
                objectStoreRequest.onsuccess = function () {
                    resolve("Saved");
                }
            } catch (e) {
                reject(e);
            }
        }
    );
}

async function getValue(key) {
    return new Promise(
        function (resolve, reject) {
            try {
                var transaction = db.transaction([objstoreName], "readonly");
                var objectStore = transaction.objectStore(objstoreName);
                var objectStoreRequest = objectStore.get(key);
                objectStoreRequest.onerror = function () {
                    reject(Error("ErrorGettingValue"));
                }
                objectStoreRequest.onsuccess = function () {
                    try {
                        resolve(objectStoreRequest.result.obj);
                    } catch (e) {
                        reject(Error("Error in objectStore.get: " + JSON.stringify(e)));
                    }
                }
            } catch (e) {
                reject(Error("Error Exception Getting Key: " + JSON.stringify(e)));
            }
        }

    );

}

function checkKeys(key, algorithm, length, usages) {
    if (!key)
        return {result: false, message: 'key is ' + key, type: algorithm};
    if (algorithm === "HMAC" || algorithm === "ECDSA") {
        if (algorithm === "HMAC") {
            if (key.usages.toString() != usages)
                return {result: false, message: 'key.usages should be ' + usages, type: algorithm};
            if (key.type != "secret")
                return {result: false, message: 'key.type should be "secret"', type: algorithm};
            if (key.algorithm.hash.name != 'SHA-512')
                return {result: false, message: 'key.algorithm.hash.name should be SHA-512', type: algorithm};
            if (!key.extractable)
                return {result: false, message: 'key.extractable should be true', type: algorithm};
        }
        if (algorithm === "ECDSA") {
            if (key.privateKey.type != "private" || key.publicKey.type != "public") {
                return {result: false, message: 'key.type should either be public or private', type: algorithm};
            }
            if (key.privateKey.algorithm.namedCurve != 'P-384' || key.publicKey.algorithm.namedCurve != 'P-384')
                return {result: false, message: 'key.algorithm.name should be P-384', type: algorithm};
            if (!key.privateKey.extractable)
                return {result: false, message: 'key.extractable should be true for privateKey', type: algorithm};
            if (!key.publicKey.extractable)
                return {result: false, message: 'key.extractable should be true for publicKey', type: algorithm};
            if (key.publicKey.usages.toString() != "verify" || key.privateKey.usages.toString() != "sign")
                return {result: false, message: 'key.privateKey.usages or key.publicKey.usages is not correct', type: algorithm};

        }
        return {result: true, key: key};
    }
    else if (key.algorithm.name != algorithm)
        return {result: false, message: 'key.algorithm.name should be ' + algorithm, type: algorithm};
    else if (!key.extractable)
        return {result: false, message: 'key.extractable should be true', type: algorithm};
    else if (key.algorithm.length != length)
        return {result: false, message: 'key.algorithm.length should be 128', type: algorithm};
    else if (key.usages.toString() != usages)
        return {result: false, message: 'key.usages should be ' + usages, type: algorithm + " " + key.usages.toString()};
    else if (key.type != 'secret')
        return {result: false, message: 'key.type should be "secret"', type: algorithm};
    else
        return {result: true, key: key};
}

async function testFetchedKeys(algorithm, key) {
    try {
        let data = self.crypto.getRandomValues(new Uint8Array(100));
        let iv = self.crypto.getRandomValues(new Uint8Array(16));
        switch (algorithm) {
            case "AES-GCM":
                var ciphertext = new Uint8Array(await self.crypto.subtle.encrypt({name: algorithm, iv: iv}, key, data));
                if (!ciphertext.length) {
                    return {result: false, message: 'Cannot Encrypt with ' + key};
                }
                return checkKeys(key, algorithm, 256, symmetricUsage.toString())
            case "AES-CBC":
                var ciphertext = new Uint8Array(await self.crypto.subtle.encrypt({name: algorithm, iv: iv}, key, data));
                if (!ciphertext.length) {
                    return {result: false, message: 'Cannot Encrypt with ' + key};
                }
                return checkKeys(key, algorithm, 256, symmetricUsage.toString())
            case "ECDSA":
                var signature = new Uint8Array(await self.crypto.subtle.sign({name: algorithm, hash: "SHA-384"}, key.privateKey, data));
                if (!signature.length) {
                    return {result: false, message: 'Cannot sign with ' + key};
                }
                return checkKeys(key, algorithm, 0, asymmetricUsage.toString())
            case "HMAC":
                signature = new Uint8Array(await self.crypto.subtle.sign(algorithm, key, data));
                if (!signature.length) {
                    return {result: false, message: 'Cannot sign with ' + key};
                }
                return checkKeys(key, algorithm, 512, asymmetricUsage.toString())
        }
    } catch (e) {
        return {result: false, message: "Exception: " + e};
    }
    return {result: true, message: "Key verified"};
}

async function runTest() {
    try {
        let keys = await createKeys();
        db = await openDB();
        for (let [algorithm, value] of Object.entries(keys)) {
            await addValue(algorithm, value);
            let fetchedKey = await getValue(algorithm)
            let check = await testFetchedKeys(algorithm, fetchedKey)
            if (!check.result)
                return check
        }
        return {result: true, message: "Keys verified"};
    } catch (e) {
        return {result: false, message: "Test Failed:Exception: " + e.name + " message: " + e.message + JSON.stringify(e)};
    }
}

self.addEventListener("message", (event) => {
    runTest().then((e) => {
        try {
            if (e.result) {
                event.source.postMessage({result: true, message: "runTest Passed"});
            } else {
                if (e.type)
                    event.source.postMessage({result: false, message: "runTest failed: " + e.message + " key type: " + e.type});
                else
                    event.source.postMessage({result: false, message: "runTest failed: " + e.message});

            }
        } catch (ee) {
            event.source.postMessage({result: false, message: "runTest failed: Exception" + JSON.stringify(ee)});
        }
    }).catch((err) => {
        event.source.postMessage({result: false, message: "runTest failed: " + JSON.stringify(err)});
    });
});
