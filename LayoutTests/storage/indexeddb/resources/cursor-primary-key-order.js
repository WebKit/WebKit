if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB primary key ordering and readback from cursors.");

function test()
{
    removeVendorPrefixes();

    prepareDatabase();
}

function prepareDatabase()
{
    debug("");
    evalAndLog("openRequest = indexedDB.open('cursor-primary-key-order')");
    openRequest.onerror = unexpectedErrorCallback;
    openRequest.onsuccess = function() {
        evalAndLog("db = openRequest.result");
        evalAndLog("versionChangeRequest = db.setVersion('1')");
        versionChangeRequest.onerror = unexpectedErrorCallback;
        versionChangeRequest.onsuccess = function() {
            evalAndLog("store = db.createObjectStore('store')");
            evalAndLog("index = store.createIndex('index', 'indexKey')");

            versionChangeRequest.result.oncomplete = populateStore;
        };
    };
}

self.keys = [
    "-Infinity",
    "-2",
    "-1",
    "0",
    "1",
    "2",
    "Infinity",

    "'0'",
    "'1'",
    "'2'",
    "'A'",
    "'B'",
    "'C'",
    "'a'",
    "'b'",
    "'c'"
];

function populateStore()
{
    debug("");
    debug("populating store...");
    evalAndLog("trans = db.transaction('store', IDBTransaction.READ_WRITE)");
    evalAndLog("store = trans.objectStore('store');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    var count = 0;
    var indexKey = 0;
    var keys = self.keys.slice();
    keys.reverse();
    keys.forEach(function(key) {
        var value = { indexKey: indexKey, count: count++ };
        evalAndLog("store.put(" + JSON.stringify(value) + ", " + key + ")");
    });
    trans.oncomplete = checkStore;
}

function checkStore()
{
    debug("");
    debug("iterating cursor...");
    evalAndLog("trans = db.transaction('store', IDBTransaction.READ_ONLY)");
    evalAndLog("store = trans.objectStore('store');");
    evalAndLog("index = store.index('index');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    cursorRequest = evalAndLog("cursorRequest = index.openCursor()");
    evalAndLog("count = 0");
    var indexKey = 0;
    cursorRequest.onerror = unexpectedErrorCallback;
    cursorRequest.onsuccess = function() {
        if (cursorRequest.result) {
            evalAndLog("cursor = cursorRequest.result");
            shouldBe("cursor.key", String(indexKey));
            shouldBe("cursor.primaryKey", self.keys[count++]);
            cursor.continue();
        } else {
            shouldBeTrue("count === keys.length");
            finishJSTest();
        }
    };
}

test();