const dbName = "cryptoKeyWrapUnwrap";
const objstoreName = "cryptoKeys";

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

async function runTest(keys) {
    try {
        db = await openDB();
        var fetchedKeys = {};
        for (let [algorithm, value] of Object.entries(keys)) {
            await addValue(algorithm, value);
            let fetchedKey = await getValue(algorithm)
            fetchedKeys[algorithm] = fetchedKey;
        }
        return {result: true, message: "Completed fetching and getting keys", keys: fetchedKeys};
    } catch (e) {
        return {result: false, message: "Test Failed:Exception: " + e.name + " message: " + e.message + JSON.stringify(e)};
    }
}

self.addEventListener("message", (event) => {
    runTest(event.data).then((e) => {
        try {
            if (e.result) {
                event.source.postMessage({result: true, message: "runTest Passed", keys: e.keys});
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
