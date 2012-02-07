if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the basics of IndexedDB's webkitIDBIndex.");

function test()
{
    request = evalAndLog("webkitIndexedDB.open('index-basics')");
    request.onsuccess = setVersion;
    request.onerror = unexpectedErrorCallback;
}

function setVersion(evt)
{
    event = evt;
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = deleteExisting;
    request.onerror = unexpectedErrorCallback;
}

function deleteExisting(evt)
{
    event = evt;
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;

    deleteAllObjectStores(db);

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
    // Add data which doesn't have a key in the zIndex.
    request = evalAndLog("event.target.source.add({x: 'value3', y: '456'}, 'key3')");
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

    self.request = evalAndLog("indexObject.openKeyCursor()");
    request.onsuccess = cursor1Continue;
    request.onerror = unexpectedErrorCallback;
}

function cursor1Continue(evt)
{
    event = evt;
    shouldBe("event.target.source", "indexObject");
    shouldBeFalse("event.target.result === null");
    shouldBeEqualToString("event.target.result.key", "value");
    shouldBeEqualToString("event.target.result.primaryKey", "key");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = cursor1Continue2;
}

function cursor1Continue2(evt)
{
    event = evt;
    shouldBeFalse("event.target.result === null");
    shouldBeEqualToString("event.target.result.key", "value2");
    shouldBeEqualToString("event.target.result.primaryKey", "key2");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = cursor1Continue3;
}

function cursor1Continue3(evt)
{
    event = evt;
    shouldBeFalse("event.target.result === null");
    shouldBeEqualToString("event.target.result.key", "value3");
    shouldBeEqualToString("event.target.result.primaryKey", "key3");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = openObjectCursor;
}

function openObjectCursor(evt)
{
    event = evt;
    shouldBeTrue("event.target.result === null");

    self.request = evalAndLog("indexObject.openCursor()");
    request.onsuccess = cursor2Continue;
    request.onerror = unexpectedErrorCallback;
}

function cursor2Continue(evt)
{
    event = evt;
    shouldBe("event.target.source", "indexObject");
    shouldBeFalse("event.target.result === null");
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
    shouldBeFalse("event.target.result === null");
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
    shouldBeFalse("event.target.result === null");
    shouldBeEqualToString("event.target.result.key", "value3");
    shouldBeEqualToString("event.target.result.value.x", "value3");
    shouldBeEqualToString("event.target.result.value.y", "456");

    // We re-use the last request object.
    evalAndLog("event.target.result.continue()");
    self.request.onsuccess = last;
}

function last(evt)
{
    event = evt;
    shouldBeTrue("event.target.result === null");

    try {
        debug("Passing an invalid key into indexObject.get({}).");
        indexObject.get({});
        testFailed("No exception thrown");
    } catch (e) {
        testPassed("Caught exception: " + e.toString());
    }

    try {
        debug("Passing an invalid key into indexObject.getKey({}).");
        indexObject.getKey({});
        testFailed("No exception thrown");
    } catch (e) {
        testPassed("Caught exception: " + e.toString());
    }
    finishJSTest();
}

var jsTestIsAsync = true;
test();
