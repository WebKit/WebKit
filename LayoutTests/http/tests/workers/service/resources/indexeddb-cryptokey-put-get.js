let createdKeys;
window.jsTestAsync = true;

async function test() {
    try {
        var registeration = await registerAndWaitForActive("resources/indexeddb-cryptokey-put-get-worker.js", "/workers/service/resources/");
        let worker = registeration.active;
        let keys = await createKeys();
        createdKeys = keys;
        worker.postMessage(keys);
    } catch (e) {
        log("Got exception: " + e);
        finishSWTest();
    }
}

async function checkKey(key, algorithm, length, usages) {
    if (!key)
        return {result: false, message: 'key is ' + key, type: algorithm};
    if (algorithm === "ECDSA") {
        exportedKey = await self.crypto.subtle.exportKey("raw", key.publicKey);
        originalKey = await self.crypto.subtle.exportKey("raw", createdKeys[algorithm].publicKey);
        if (bytesToHexString(exportedKey) === bytesToHexString(originalKey)) {
            log("ECDSA public key matches original");
        } else {
            return {result: false, message: "exportedKey and originalKey are not the same", type: algorithm};
        }
    } else {

        let exportedKey = await self.crypto.subtle.exportKey("raw", key);
        let originalKey = await self.crypto.subtle.exportKey("raw", createdKeys[algorithm]);
        if (bytesToHexString(exportedKey) === bytesToHexString(originalKey)) {
            log(algorithm + " exported key matches original.");
        } else {
            return {result: false, message: "exportedKey and originalKey are not the same", type: algorithm};
        }
    }
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

const asymmetricUsage = ['sign', 'verify'];
const symmetricUsage = ['decrypt', 'encrypt'];


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
async function testKey(algorithm, key) {
    try {
        switch (algorithm) {
            case "AES-GCM":
                return await checkKey(key, algorithm, 256, symmetricUsage.toString())
            case "AES-CBC":
                return await checkKey(key, algorithm, 256, symmetricUsage.toString())
            case "ECDSA":
                return await checkKey(key, algorithm, 0, asymmetricUsage.toString())
            case "HMAC":
                return await checkKey(key, algorithm, 512, asymmetricUsage.toString())
        }
    } catch (e) {
        return {result: false, message: "Exception: " + e};
    }
    return {result: true, message: "Key verified"};
}


navigator.serviceWorker.addEventListener("message", async (event) => {
    try {
        if (event.data.result) {
            let fetchedKeys = event.data.keys;
            for (const [algorithm, key] of Object.entries(fetchedKeys)) {
                let r = await testKey(algorithm, key);
                if (!r.result) {
                    log("Test Failed: key verification failed for " + algorithm + " " + r.message);
                    finishSWTest();
                }
            }
            log("Test Passed");
            finishSWTest();
        } else {
            log("Test Failed: " + event.data.message);
            finishSWTest();
        }
    } catch (err) {
        log("Test Failed. Exception: " + JSON.stringify(err));
        finishSWTest();
    }
});

test();
