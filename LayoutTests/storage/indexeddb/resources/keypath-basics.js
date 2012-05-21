if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test for valid and invalid keypaths");

function test()
{
    removeVendorPrefixes();
    name = self.location.pathname;
    request = evalAndLog("indexedDB.open(name)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = testValidKeyPaths;
    request.onerror = unexpectedErrorCallback;
}

function testValidKeyPaths()
{
    deleteAllObjectStores(db);

    evalAndLog("store = db.createObjectStore('name')");
    shouldBeNull("store.keyPath");
    deleteAllObjectStores(db);

    testKeyPaths = [
        { keyPath: "null", storeExpected: "null", indexExpected: "'null'" },
        { keyPath: "undefined", storeExpected: "null", indexExpected: "'undefined'" },
        { keyPath: "''", storeExpected: "''", indexExpected: "''" },
        { keyPath: "'foo'", storeExpected: "'foo'", indexExpected: "'foo'" },
        { keyPath: "'foo.bar.baz'", storeExpected: "'foo.bar.baz'", indexExpected: "'foo.bar.baz'" }
    ];

    testKeyPaths.forEach(function (testCase) {
        evalAndLog("store = db.createObjectStore('name', {keyPath: " + testCase.keyPath + "})");
        shouldBe("store.keyPath", testCase.storeExpected);
        evalAndLog("index = store.createIndex('name', " + testCase.keyPath + ")");
        shouldBe("index.keyPath", testCase.indexExpected);
        deleteAllObjectStores(db);
    });

    testInvalidKeyPaths();
}

function testInvalidKeyPaths()
{
    deleteAllObjectStores(db);

    testKeyPaths = ['[]', '["foo"]', '["foo", "bar"]', '["", ""]', '[1.0, 2.0]', '[["foo"]]', '["foo", ["bar"]]'];
    testKeyPaths.forEach(function (keyPath) {
        globalKeyPath = keyPath;
        debug("globalKeyPath = '" + globalKeyPath + "'");
        evalAndExpectException("db.createObjectStore('name', {keyPath: globalKeyPath})", "IDBDatabaseException.NON_TRANSIENT_ERR");
        deleteAllObjectStores(db);
    });

    testKeyPaths = [' ', 'foo ', 'foo bar', 'foo. bar', 'foo .bar', 'foo..bar', '+foo', 'foo%'];
    testKeyPaths.forEach(function (keyPath) {
        globalKeyPath = keyPath;
        debug("globalKeyPath = '" + globalKeyPath + "'");
        evalAndExpectException("db.createObjectStore('name', {keyPath: globalKeyPath})", "IDBDatabaseException.NON_TRANSIENT_ERR");
        deleteAllObjectStores(db);
    });

    testKeyPaths = [' ', 'foo ', 'foo bar', 'foo. bar', 'foo .bar', 'foo..bar', '+foo', 'foo%'];
    testKeyPaths.forEach(function (keyPath) {
        globalKeyPath = keyPath;
        debug("globalKeyPath = '" + globalKeyPath + "'");
        store = evalAndLog("store = db.createObjectStore('storeName')");
        evalAndExpectException("store.createIndex('name', globalKeyPath)", "IDBDatabaseException.NON_TRANSIENT_ERR");
        deleteAllObjectStores(db);
    });

    finishJSTest();
}

test();
