// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_put_get_values.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's putting and getting values in an object store");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    testString = evalAndLog("testString = { key: 0, value: 'testString' };");
    testInt = evalAndLog("testInt = { key: 1, value: 1002 };");
    objectStore = evalAndLog("objectStore = db.createObjectStore('Objects', { autoIncrement: false });");
    request = evalAndLog("request = objectStore.add(testString.value, testString.key);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postAdd;
}

function postAdd()
{
    shouldBe("event.target.result", "testString.key");
    request = evalAndLog("request = objectStore.get(testString.key);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postGet;
}

function postGet()
{
    shouldBe("event.target.result", "testString.value");
    request = evalAndLog("request = objectStore.add(testInt.value, testInt.key);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postAddInt;
}

function postAddInt()
{
    shouldBe("event.target.result", "testInt.key");
    request = evalAndLog("request = objectStore.get(testInt.key);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postGetInt;
}

function postGetInt()
{
    shouldBe("event.target.result", "testInt.value");
    objectStoreAutoIncrement = evalAndLog("objectStoreAutoIncrement = db.createObjectStore('AutoIncremented Objects', { autoIncrement: true });");
    request = evalAndLog("request = objectStoreAutoIncrement.add(testString.value);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postAddAutoIncrement;
}

function postAddAutoIncrement()
{
    testString.key = evalAndLog("testString.key = event.target.result;");
    request = evalAndLog("request = objectStoreAutoIncrement.get(testString.key);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postGetAutoIncrement;
}

function postGetAutoIncrement()
{
    shouldBe("event.target.result", "testString.value");
    request = evalAndLog("request = objectStoreAutoIncrement.add(testInt.value);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postAddIntAutoIncrement;
}

function postAddIntAutoIncrement()
{
    testInt.key = evalAndLog("testInt.key = event.target.result;");
    request = evalAndLog("request = objectStoreAutoIncrement.get(testInt.key);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postGetIntAutoIncrement;
}

function postGetIntAutoIncrement()
{
    shouldBe("event.target.result", "testInt.value");
    finishJSTest();
}
