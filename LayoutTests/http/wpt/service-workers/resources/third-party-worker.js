addEventListener('activate', async (e) => {
    await self.clients.claim();
});
addEventListener('message', async (e) => {
    if (e.data === 'write') {
        await writeDB();
        await self.caches.open(e.data);
        e.source.postMessage('written');
        return;
    }
    if (e.data === 'read') {
        var keys = await self.caches.keys();
        var db = await readDB();
        if (!db)
            db = null;
        var result = { cache : keys, db : db };
        e.source.postMessage(JSON.stringify(result));
        return;
    }
    e.source.postMessage('error');
});

function readDB() {
    return new Promise(function(resolve, reject) {
        var openRequest = indexedDB.open('db');

        openRequest.onerror = reject;
        openRequest.onupgradeneeded = function() {
            var db = openRequest.result;
            db.createObjectStore('store');
        };

        openRequest.onsuccess = function() {
            var db = openRequest.result;
            var tx = db.transaction('store');
            var store = tx.objectStore('store');
            var getRequest = store.get('key');

            getRequest.onerror = function() {
                db.close();
                reject(getRequest.error);
            };
            getRequest.onsuccess = function() {
                db.close();
                resolve(getRequest.result);
            };
        };
    });
}

function writeDB() {
    return new Promise(function(resolve, reject) {
        var openRequest = indexedDB.open('db');

        openRequest.onerror = reject;
        openRequest.onupgradeneeded = function() {
            var db = openRequest.result;
            db.createObjectStore('store');
        };
        openRequest.onsuccess = function() {
            var db = openRequest.result;
            var tx = db.transaction('store', 'readwrite');
            var store = tx.objectStore('store');
            store.put('value', 'key');

            tx.onerror = function() {
                db.close();
                reject(tx.error);
            };
            tx.oncomplete = function() {
                db.close();
                resolve();
            };
        };
    });
}
