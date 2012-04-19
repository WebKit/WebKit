if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's transaction and objectStore calls");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open('transaction-and-objectstore-calls')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    self.db = evalAndLog("db = event.target.result");
    request = evalAndLog("result = db.setVersion('version 1')");
    request.onsuccess = cleanDatabase;
    request.onerror = unexpectedErrorCallback;
}

function cleanDatabase()
{
    trans = evalAndLog("trans = event.target.result");
    deleteAllObjectStores(db);

    evalAndLog("db.createObjectStore('a')");
    evalAndLog("db.createObjectStore('b')");
    evalAndLog("db.createObjectStore('store').createIndex('index', 'some_path')");
    evalAndLog("trans.addEventListener('complete', created, true)");
    debug("");
}

function created()
{
    trans = evalAndLog("trans = db.transaction(['a'])");
    evalAndLog("trans.objectStore('a')");
    evalAndExpectException("trans.objectStore('b')", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("trans.objectStore('x')", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    trans = evalAndLog("trans = db.transaction(['a'])");
    evalAndLog("trans.objectStore('a')");
    evalAndExpectException("trans.objectStore('b')", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("trans.objectStore('x')", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    trans = evalAndLog("trans = db.transaction(['b'])");
    evalAndLog("trans.objectStore('b')");
    evalAndExpectException("trans.objectStore('a')", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("trans.objectStore('x')", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    trans = evalAndLog("trans = db.transaction(['a', 'b'])");
    evalAndLog("trans.objectStore('a')");
    evalAndLog("trans.objectStore('b')");
    evalAndExpectException("trans.objectStore('x')", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    trans = evalAndLog("trans = db.transaction(['b', 'a'])");
    evalAndLog("trans.objectStore('a')");
    evalAndLog("trans.objectStore('b')");
    evalAndExpectException("trans.objectStore('x')", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    debug("Passing a string as the first argument is a shortcut for just one object store:");
    trans = evalAndLog("trans = db.transaction('a')");
    evalAndLog("trans.objectStore('a')");
    evalAndExpectException("trans.objectStore('b')", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("trans.objectStore('x')", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    shouldThrow("trans = db.transaction()");
    debug("");

    evalAndExpectException("db.transaction(['x'])", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("db.transaction(['x'])", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("db.transaction(['a', 'x'])", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("db.transaction(['x', 'x'])", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("db.transaction(['a', 'x', 'b'])", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    debug("Exception thrown when no stores specified:");
    evalAndExpectException("db.transaction([])", "DOMException.INVALID_ACCESS_ERR");
    debug("");

    debug("{} coerces to a string - so no match, but not a type error:");
    evalAndExpectException("db.transaction({})", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("db.transaction({mode:0})", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    debug("Overriding the default string coercion makes these work:");
    evalAndLog("db.transaction({toString:function(){return 'a';}})");
    evalAndLog("db.transaction([{toString:function(){return 'a';}}])");
    debug("... but you still need to specify a real store:");
    evalAndExpectException("db.transaction([{toString:function(){return 'x';}}])", "IDBDatabaseException.NOT_FOUND_ERR");
    evalAndExpectException("db.transaction([{toString:function(){return 'x';}}])", "IDBDatabaseException.NOT_FOUND_ERR");
    debug("");

    trans = evalAndLog("trans = db.transaction(['store'])");
    shouldBeTrue("trans != null");
    trans.onabort = unexpectedAbortCallback;
    trans.onerror = unexpectedErrorCallback;
    trans.oncomplete = afterComplete;
    evalAndLog("store = trans.objectStore('store')");
    shouldBeTrue("store != null");
    evalAndLog("store.get('some_key')");
}

function afterComplete()
{
    debug("transaction complete, ensuring methods fail");
    shouldBeTrue("trans != null");
    shouldBeTrue("store != null");
    evalAndExpectException("trans.objectStore('store')", "IDBDatabaseException.NOT_ALLOWED_ERR");
    evalAndExpectException("store.index('index')", "IDBDatabaseException.NOT_ALLOWED_ERR");

    finishJSTest();
}

test();
