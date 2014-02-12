if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB deleteIndex method");

indexedDBTest(prepareDatabase, postTwiddling);
function prepareDatabase()
{
    db = event.target.result;
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    evalAndExpectException("objectStore.deleteIndex('first')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    shouldThrow("objectStore.deleteIndex()"); // TypeError: not enough arguments.
    index = evalAndLog("index = objectStore.createIndex('first', 'first');");
    evalAndExpectException("objectStore.deleteIndex('FIRST')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    index = evalAndLog("index = objectStore.createIndex('second', 'second');");
    returnValue = evalAndLog("returnValue = objectStore.deleteIndex('first');");
    shouldBe("returnValue", "undefined");
}

function postTwiddling()
{
    evalAndExpectException("db.createObjectStore('bar');", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("objectStore.deleteIndex('second')", "0", "'TransactionInactiveError'");
    finishJSTest();
}
