if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's IDBObjectStore.count().");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('objectstore-count')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        db = evalAndLog("db = event.target.result");
        request = evalAndLog("db.setVersion('new version')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = prepareDatabase;
    };
}

function prepareDatabase()
{
    debug("");
    debug("preparing database");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");

    deleteAllObjectStores(db);

    store = evalAndLog("store = db.createObjectStore('storeName', null)");

    debug("adding 0 ... 99");
    for (var i = 0; i < 100; ++i) {
        request = store.add(i, i);
        request.onerror = unexpectedErrorCallback;
    }
    trans.oncomplete = verifyCount;
}

function verifyCount()
{
    debug("");
    debug("verifying count without range");
    trans = evalAndLog("trans = db.transaction('storeName', 'readonly')");
    shouldBeTrue("trans != null");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = verifyCountWithRange;

    store = evalAndLog("store = trans.objectStore('storeName')");
    shouldBeTrue("store != null");

    request = evalAndLog("request = store.count()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
         shouldBeTrue("typeof request.result == 'number'");
         shouldBe("request.result", "100");
         // let the transaction complete
    };
}

function verifyCountWithRange()
{
    debug("");
    debug("verifying count with range");
    trans = evalAndLog("trans = db.transaction('storeName', 'readonly')");
    shouldBeTrue("trans != null");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = verifyCountWithKey;

    store = evalAndLog("store = trans.objectStore('storeName')");
    shouldBeTrue("store != null");

    var tests = [
        { lower: 0, lowerOpen: false, upper: 99, upperOpen: false, expected: 100 },
        { lower: 0, lowerOpen: true, upper: 99, upperOpen: false, expected: 99 },
        { lower: 0, lowerOpen: false, upper: 99, upperOpen: true, expected: 99 },
        { lower: 0, lowerOpen: true, upper: 99, upperOpen: true, expected: 98 },
        { lower: 0, lowerOpen: false, upper: 100, upperOpen: false, expected: 100 },
        { lower: 0, lowerOpen: false, upper: 100, upperOpen: false, expected: 100 },
        { lower: 10, lowerOpen: false, upper: 100, upperOpen: false, expected: 90 },
        { lower: 0, lowerOpen: false, upper: 0, upperOpen: false, expected: 1 },
        { lower: 500, lowerOpen: false, upper: 500, upperOpen: false, expected: 0 }
    ];

    function nextTest() {
        debug("");
        evalAndLog("test = " + JSON.stringify(tests.shift()));
        request = evalAndLog("request = store.count(IDBKeyRange.bound(test.lower, test.upper, test.lowerOpen, test.upperOpen))");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
             shouldBeTrue("typeof request.result == 'number'");
             shouldBe("request.result", String(test.expected));

             if (tests.length) {
                 nextTest();
             }
             // otherwise let the transaction complete
        };
    }

    nextTest();
}

function verifyCountWithKey()
{
    debug("");
    debug("verifying count with key");
    trans = evalAndLog("trans = db.transaction('storeName', 'readonly')");
    shouldBeTrue("trans != null");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = finishJSTest;

    store = evalAndLog("store = trans.objectStore('storeName')");
    shouldBeTrue("store != null");

    evalAndExpectException("store.count(NaN)", "IDBDatabaseException.DATA_ERR");
    evalAndExpectException("store.count({})", "IDBDatabaseException.DATA_ERR");
    evalAndExpectException("store.count(/regex/)", "IDBDatabaseException.DATA_ERR");

    var tests = [
        { key: 0, expected: 1 },
        { key: 100, expected: 0 },
        { key: null, expected: 100 }
    ];

    function nextTest() {
        debug("");
        evalAndLog("test = " + JSON.stringify(tests.shift()));
        request = evalAndLog("request = store.count(test.key)");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            shouldBeTrue("typeof request.result == 'number'");
            shouldBe("request.result", String(test.expected));
             if (tests.length) {
                 nextTest();
             }
             // otherwise let the transaction complete
        };
    }

    nextTest();
}

test();