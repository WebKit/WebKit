if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB invalid keys");

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
    request.onsuccess = testGroup1;
    request.onerror = unexpectedErrorCallback;
}

function testGroup1()
{
    deleteAllObjectStores(db);

    objectStore = evalAndLog("db.createObjectStore('foo');");
    testInvalidKeys();
}

function testInvalidKeys()
{
    var invalidKeys = [
        "void 0", // Undefined
        "null", // Null
        "(function() { return arguments; }())", // Arguments
        "true", // Boolean
        "false", // Boolean
        "new Error", // Error
        "function () {}", // Function
        "JSON", // JSON
        "Math", // Math
        "NaN", // Number (special case)
        "new Date(NaN)", // Date (special case)
        "{}", // Object
        "/regex/", // RegExp
        "self", // global
        "self.document", // HTMLDocument
        "self.document.body" // HTMLBodyElement
    ];

    invalidKeys.forEach(function(key) {
        evalAndExpectException("request = objectStore.put('value', " + key + ")", "IDBDatabaseException.DATA_ERR", "'DataError'");
    });

    finishJSTest();
}

test();