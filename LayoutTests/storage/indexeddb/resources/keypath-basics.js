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
    debug("");
    debug("testValidKeyPaths():");
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
    debug("");
    debug("testInvalidKeyPaths():");
    deleteAllObjectStores(db);

    debug("");
    debug("Object store key path may not be empty or an array if autoIncrement is true");
    testKeyPaths = ["''", "[]", "['a']", "['']"];
    testKeyPaths.forEach(function (keyPath) {
        store = evalAndExpectException("store = db.createObjectStore('storeName', {autoIncrement: true, keyPath: " + keyPath + "})", "DOMException.INVALID_ACCESS_ERR");
        deleteAllObjectStores(db);
    });

    debug("");
    debug("Key paths which are never valid:");
    testKeyPaths = ["' '", "'foo '", "'foo bar'", "'foo. bar'", "'foo .bar'", "'foo..bar'", "'+foo'", "'foo%'", "'1'", "'1.0'"];
    testKeyPaths.forEach(function (keyPath) {
        evalAndExpectException("db.createObjectStore('name', {keyPath: " + keyPath + "})", "DOMException.SYNTAX_ERR");
        evalAndExpectException("db.createObjectStore('name').createIndex('name', " + keyPath + ")", "DOMException.SYNTAX_ERR");
        deleteAllObjectStores(db);
    });

    finishJSTest();
}

test();
