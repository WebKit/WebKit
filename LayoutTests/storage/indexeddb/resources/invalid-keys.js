if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB invalid keys");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
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
        evalAndExpectException("request = objectStore.put('value', " + key + ")", "0", "'DataError'");
    });

    finishJSTest();
}
