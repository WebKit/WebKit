if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test features of IndexedDB's unique indices.");

indexedDBTest(prepareDatabase, setVersionCompleted);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    self.store = evalAndLog("db.createObjectStore('store')");
    self.indexObject = evalAndLog("store.createIndex('index', 'x', {unique: true})");
}

function setVersionCompleted()
{
    debug("setVersionCompleted():");
    self.transaction = evalAndLog("transaction = db.transaction(['store'], 'readwrite')");

    request = evalAndLog("transaction.objectStore('store').put({x: 1}, 'foo')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = addMoreData;
}

function addMoreData()
{
    debug("addMoreData():");

    // The x value violates the uniqueness constraint of the index.
    request = evalAndLog("transaction.objectStore('store').put({x: 1}, 'bar')");
    request.onerror = addMoreDataFailed;
    request.onsuccess = unexpectedSuccessCallback;
}

function addMoreDataFailed()
{
    debug("addMoreDataFailed():");

    // Don't abort the transaction.
    evalAndLog("event.preventDefault()");

    shouldBe("event.target.error.name", "'ConstraintError'");

    // Update the 'foo' entry in object store, changing the value of x.
    request = evalAndLog("transaction.objectStore('store').put({x: 0}, 'foo')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = changeDataSuccess;
}

function changeDataSuccess()
{
    debug("changeDataSuccess():");

    // An index cursor starting at 1 should not find anything.
    var request = evalAndLog("transaction.objectStore('store').index('index').openCursor(IDBKeyRange.lowerBound(1))");
    request.onsuccess = cursorSuccess;
    request.onerror = unexpectedErrorCallback;
}

function cursorSuccess()
{
    debug("cursorSuccess():");
    shouldBeNull("event.target.result");

    // A key cursor starting at 1 should not find anything.
    var request = evalAndLog("transaction.objectStore('store').index('index').openKeyCursor(IDBKeyRange.lowerBound(1))");
    request.onsuccess = keyCursorSuccess;
    request.onerror = unexpectedErrorCallback;
}

function keyCursorSuccess()
{
    debug("keyCursorSuccess():");
    shouldBeNull("event.target.result");

    // Now we should be able to add a value with x: 1.
    request = evalAndLog("transaction.objectStore('store').put({x: 1}, 'bar')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = addMoreDataSucces;
}

function addMoreDataSucces()
{
    debug("addMoreDataSucces():");

    request = evalAndLog("transaction.objectStore('store').delete('bar')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = deleteSuccess;
}

function deleteSuccess()
{
    debug("deleteSuccess():");

    // Now we should be able to add a value with x: 1 again.
    request = evalAndLog("transaction.objectStore('store').put({x: 1}, 'baz')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = finalAddSuccess;
}

function finalAddSuccess() {
    debug("finalAddSuccess():");

    // An overwrite should be ok.
    request = evalAndLog("transaction.objectStore('store').put({x: 1}, 'baz')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = finishJSTest;
}
