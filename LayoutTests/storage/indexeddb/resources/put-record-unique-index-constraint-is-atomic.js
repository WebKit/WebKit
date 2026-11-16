if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("A put() that fails a unique index constraint must be atomic: the record it would have overwritten must remain in the object store unchanged.");

indexedDBTest(prepareDatabase, onOpen);

var db;
var errorName;
var record;
var username;
var count;

function prepareDatabase(event)
{
    db = event.target.result;
    var store = evalAndLog("store = db.createObjectStore('users', { keyPath: 'id' })");
    evalAndLog("store.createIndex('username', 'username', { unique: true })");
    evalAndLog("store.add({ id: 1, username: 'foo' })");
    evalAndLog("store.add({ id: 2, username: 'bar' })");
}

function onOpen(event)
{
    db = event.target.result;
    debug("");
    debug("Put {id:1, username:'bar'}: this overwrites id:1 but its username collides with id:2's unique index entry.");
    var transaction = evalAndLog("transaction = db.transaction('users', 'readwrite')");
    var store = evalAndLog("store = transaction.objectStore('users')");
    var request = evalAndLog("request = store.put({ id: 1, username: 'bar' })");
    request.onsuccess = function() {
        testFailed("put() unexpectedly succeeded; it should have failed with a ConstraintError.");
    };
    request.onerror = function(event) {
        errorName = request.error.name;
        shouldBeEqualToString("errorName", "ConstraintError");
        debug("Preventing default so the transaction commits instead of aborting.");
        event.preventDefault();
    };
    transaction.oncomplete = function() {
        debug("Transaction committed.");
        verifyStoreUnchanged();
    };
    transaction.onabort = function() {
        testFailed("Transaction was aborted: " + (transaction.error && transaction.error.name));
        finishJSTest();
    };
}

function verifyStoreUnchanged()
{
    debug("");
    debug("The failed put() must have left the store untouched.");
    var transaction = evalAndLog("transaction = db.transaction('users', 'readonly')");
    var store = evalAndLog("store = transaction.objectStore('users')");

    var getRequest = evalAndLog("getRequest = store.get(1)");
    getRequest.onsuccess = function() {
        record = getRequest.result;
        shouldBeNonNull("record");
        if (record) {
            username = record.username;
            shouldBeEqualToString("username", "foo");
        }
    };

    var countRequest = evalAndLog("countRequest = store.count()");
    countRequest.onsuccess = function() {
        count = countRequest.result;
        shouldBe("count", "2");
    };

    transaction.oncomplete = finishJSTest;
}
