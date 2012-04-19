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

    testKeyPaths = [null, undefined, ''];
    testKeyPaths.forEach(function (keyPath) {
        globalKeyPath = keyPath;
        debug("globalKeyPath = '" + globalKeyPath + "'");
        evalAndLog("db.createObjectStore('name', {keyPath: globalKeyPath})");
        deleteAllObjectStores(db);
    });

    testKeyPaths = ['foo', 'foo.bar.baz'];
    testKeyPaths.forEach(function (keyPath) {
        globalKeyPath = keyPath;
        debug("globalKeyPath = '" + globalKeyPath + "'");
        evalAndLog("db.createObjectStore('name', {keyPath: globalKeyPath})");
        deleteAllObjectStores(db);
    });

    testKeyPaths = [null, undefined, '', 'foo', 'foo.bar.baz'];
    testKeyPaths.forEach(function (keyPath) {
        globalKeyPath = keyPath;
        debug("globalKeyPath = '" + globalKeyPath + "'");
        store = evalAndLog("store = db.createObjectStore('storeName')");
        evalAndLog("store.createIndex('name', globalKeyPath)");
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