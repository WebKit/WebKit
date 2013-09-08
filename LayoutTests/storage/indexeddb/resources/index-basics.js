if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the basics of IndexedDB's webkitIDBIndex.");

indexedDBTest(prepareDatabase);
function prepareDatabase(evt)
{
    preamble(evt);
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    self.store = evalAndLog("db.createObjectStore('storeName', null)");
    self.indexObject = evalAndLog("store.createIndex('indexName', 'x')");
    self.indexObject2 = evalAndLog("store.createIndex('indexName2', 'y', {unique: false})");
    self.indexObject3 = evalAndLog("store.createIndex('zIndex', 'z', {unique: true})");
    shouldBeFalse("indexObject2.unique");
    shouldBeTrue("indexObject3.unique");
    evalAndExpectExceptionClass("store.createIndex('failureIndex', 'zzz', true)", "TypeError");
    evalAndExpectExceptionClass("store.createIndex('failureIndex', 'zzz', 'string')", "TypeError");
    addData();
}

function addData()
{
    shouldBeTrue("'name' in indexObject");
    shouldBeEqualToString("indexObject.name", "indexName");
    shouldBeTrue("'objectStore' in indexObject");
    shouldBeEqualToString("indexObject.objectStore.name", "storeName");
    shouldBeTrue("'keyPath' in indexObject");
    shouldBeEqualToString("indexObject.keyPath", "x");
    shouldBeTrue("'unique' in indexObject");
    shouldBeTrue("'multiEntry' in indexObject");
    shouldBeFalse("indexObject.unique");
    shouldBeFalse("indexObject.multiEntry");
    shouldBeTrue("'openKeyCursor' in indexObject");
    shouldBeEqualToString("typeof indexObject.openKeyCursor", "function");
    shouldBeTrue("'openCursor' in indexObject");
    shouldBeEqualToString("typeof indexObject.openCursor", "function");
    shouldBeTrue("'getKey' in indexObject");
    shouldBeEqualToString("typeof indexObject.getKey", "function");
    shouldBeTrue("'get' in indexObject");
    shouldBeEqualToString("typeof indexObject.get", "function");
    shouldBeTrue("'count' in indexObject");
    shouldBeEqualToString("typeof indexObject.count", "function");

    request = evalAndLog("store.add({x: 'value', y: 'zzz', z: 2.72}, 'key')");
    request.onsuccess = addData2;
    request.onerror = unexpectedErrorCallback;
}

function addData2(evt)
{
    event = evt;
    request = evalAndLog("event.target.source.add({x: 'value2', y: 'zzz2', z: 2.71, foobar: 12}, 'key2')");
    request.onsuccess = addData3;
    request.onerror = unexpectedErrorCallback;
    self.indexObject4 = evalAndLog("store.createIndex('indexWhileAddIsInFlight', 'x')");
    self.indexObject5 = evalAndLog("store.createIndex('indexWithWeirdKeyPath', 'foobar')");
}

function addData3(evt)
{
    event = evt;
    debug("Add data which doesn't have a key in the z index.");
    request = evalAndLog("event.target.source.add({x: 'value3', y: '456'}, 'key3')");
    request.onsuccess = addData4;
    request.onerror = unexpectedErrorCallback;
}

function addData4(evt)
{
    event = evt;
    debug("Add data which has invalid key for y index, no key for the z index.");
    request = evalAndLog("event.target.source.add({x: 'value4', y: null}, 'key4')");
    request.onsuccess = getData;
    request.onerror = unexpectedErrorCallback;
}

function getData(evt)
{
    event = evt;
    request = evalAndLog("indexObject.getKey('value')");
    request.onsuccess = getObjectData;
    request.onerror = unexpectedErrorCallback;
}

function getObjectData(evt)
{
    event = evt;
    shouldBeEqualToString("event.target.result", "key");

    request = evalAndLog("indexObject2.getKey('zzz')");
    request.onsuccess = getObjectData2;
    request.onerror = unexpectedErrorCallback;
}

function getObjectData2(evt)
{
    event = evt;
    shouldBeEqualToString("event.target.result", "key");

    request = evalAndLog("indexObject3.get(2.71)");
    request.onsuccess = getObjectData3;
    request.onerror = unexpectedErrorCallback;
}

function getObjectData3(evt)
{
    event = evt;
    shouldBeEqualToString("event.target.result.x", "value2");

    request = evalAndLog("indexObject.get('value')");
    request.onsuccess = getDataFail;
    request.onerror = unexpectedErrorCallback;
}

function getDataFail(evt)
{
    event = evt;
    shouldBeEqualToString("event.target.result.x", "value");
    shouldBeEqualToString("event.target.result.y", "zzz");

    request = evalAndLog("indexObject.getKey('does not exist')");
    request.onsuccess = getObjectDataFail;
    request.onerror = unexpectedSuccessCallback;
}

function getObjectDataFail(evt)
{
    event = evt;
    shouldBe("event.target.result", "undefined");

    request = evalAndLog("indexObject.get('does not exist')");
    request.onsuccess = getObjectData4;
    request.onerror = unexpectedSuccessCallback;
}

function getObjectData4(evt)
{
    event = evt;
    shouldBe("event.target.result", "undefined");

    request = evalAndLog("indexObject4.getKey('value2')");
    request.onsuccess = openKeyCursor;
    request.onerror =  unexpectedErrorCallback;
}

function openKeyCursor(evt)
{
    event = evt;
    shouldBeEqualToString("event.target.result", "key2");

    debug("");
    debug("Verify that specifying an invalid direction raises an exception:");
    evalAndExpectExceptionClass("indexObject.openKeyCursor(0, 'invalid-direction')", "TypeError");
    debug("");

    self.request = evalAndLog("indexObject.openKeyCursor()");
    request.onsuccess = cursor1Continue;
    request.onerror = unexpectedErrorCallback;
}

function cursor1Continue(evt)
{
    event = evt;
    shouldBe("event.target.source", "indexObject");
    shouldBeNonNull("event.target.result");
    shouldBeEqualToString("event.target.result.key", "value");
    shouldBeEqualToString("event.target.result.primaryKey", "key");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = cursor1Continue2;
}

function cursor1Continue2(evt)
{
    event = evt;
    shouldBeNonNull("event.target.result");
    shouldBeEqualToString("event.target.result.key", "value2");
    shouldBeEqualToString("event.target.result.primaryKey", "key2");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = cursor1Continue3;
}

function cursor1Continue3(evt)
{
    event = evt;
    shouldBeNonNull("event.target.result");
    shouldBeEqualToString("event.target.result.key", "value3");
    shouldBeEqualToString("event.target.result.primaryKey", "key3");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = cursor1Continue4;
}

function cursor1Continue4(evt)
{
    event = evt;
    shouldBeNonNull("event.target.result");
    shouldBeEqualToString("event.target.result.key", "value4");
    shouldBeEqualToString("event.target.result.primaryKey", "key4");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = openObjectCursor;
}

function openObjectCursor(evt)
{
    event = evt;
    shouldBeNull("event.target.result");

    debug("");
    debug("Verify that specifying an invalid direction raises an exception:");
    evalAndExpectExceptionClass("indexObject.openCursor(0, 'invalid-direction')", "TypeError");
    debug("");

    self.request = evalAndLog("indexObject.openCursor()");
    request.onsuccess = cursor2Continue;
    request.onerror = unexpectedErrorCallback;
}

function cursor2Continue(evt)
{
    event = evt;
    shouldBe("event.target.source", "indexObject");
    shouldBeNonNull("event.target.result");
    shouldBeEqualToString("event.target.result.key", "value");
    shouldBeEqualToString("event.target.result.value.x", "value");
    shouldBeEqualToString("event.target.result.value.y", "zzz");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = cursor2Continue2;
}

function cursor2Continue2(evt)
{
    event = evt;
    shouldBeNonNull("event.target.result");
    shouldBeEqualToString("event.target.result.key", "value2");
    shouldBeEqualToString("event.target.result.value.x", "value2");
    shouldBeEqualToString("event.target.result.value.y", "zzz2");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = cursor2Continue3;
}

function cursor2Continue3(evt)
{
    event = evt;
    shouldBeNonNull("event.target.result");
    shouldBeEqualToString("event.target.result.key", "value3");
    shouldBeEqualToString("event.target.result.value.x", "value3");
    shouldBeEqualToString("event.target.result.value.y", "456");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = cursor2Continue4;
}

function cursor2Continue4(evt)
{
    event = evt;
    shouldBeNonNull("event.target.result");
    shouldBeEqualToString("event.target.result.key", "value4");
    shouldBeEqualToString("event.target.result.value.x", "value4");
    shouldBe("event.target.result.value.y", "null");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = last;
}

function last(evt)
{
    event = evt;
    shouldBeNull("event.target.result");

    evalAndLog("request = indexObject.count()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = index1Count;
}

function index1Count(evt)
{
    event = evt;
    shouldBe("event.target.result", "4");

    evalAndLog("request = indexObject2.count()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = index2Count;
}

function index2Count(evt)
{
    event = evt;
    shouldBe("event.target.result", "3");

    evalAndLog("request = indexObject3.count()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = index3Count;
}

function index3Count(evt)
{
    event = evt;
    shouldBe("event.target.result", "2");

    debug("Passing an invalid key into indexObject.get({}).");
    evalAndExpectException("indexObject.get({})", "0", "'DataError'");

    debug("Passing an invalid key into indexObject.getKey({}).");
    evalAndExpectException("indexObject.getKey({})", "0", "'DataError'");

    finishJSTest();
}
