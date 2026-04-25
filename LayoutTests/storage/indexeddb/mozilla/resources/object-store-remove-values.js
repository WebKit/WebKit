// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_objectStore_remove_values.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB: adds, gets, deletes, and re-gets a record in a variety of different object store configurations");

data = evalAndLog("data = [\n" +
"        { name: 'inline key; key generator',\n" +
"          autoIncrement: true,\n" +
"          storedObject: {name: 'Lincoln'},\n" +
"          keyName: 'id',\n" +
"          keyValue: undefined,\n" +
"        },\n" +
"        { name: 'inline key; no key generator',\n" +
"          autoIncrement: false,\n" +
"          storedObject: {id: 1, name: 'Lincoln'},\n" +
"          keyName: 'id',\n" +
"          keyValue: undefined,\n" +
"        },\n" +
"        { name: 'out of line key; key generator',\n" +
"          autoIncrement: true,\n" +
"          storedObject: {name: 'Lincoln'},\n" +
"          keyName: undefined,\n" +
"          keyValue: undefined,\n" +
"        },\n" +
"        { name: 'out of line key; no key generator',\n" +
"          autoIncrement: false,\n" +
"          storedObject: {name: 'Lincoln'},\n" +
"          keyName: undefined,\n" +
"          keyValue: 1,\n" +
"        }\n" +
"    ];");

i = evalAndLog("i = 0;");
var db = null;
debug("");

runATest();

function runATest()
{
    if (db)
        db.close();
    if (i < data.length) {
        test = data[i];
        indexedDBTest(createObjectStore, runATest);
    } else {
        finishJSTest();
    }
}

function createObjectStore()
{
    db = event.target.result;
    if (test.keyName) {
        objectStore = evalAndLog("objectStore = db.createObjectStore(test.name, { keyPath: test.keyName, autoIncrement: test.autoIncrement });");
    } else {
        objectStore = evalAndLog("objectStore = db.createObjectStore(test.name, { autoIncrement: test.autoIncrement });");
    }
    if (test.keyValue) {
        request = evalAndLog("request = objectStore.add(test.storedObject, test.keyValue);");
    } else {
        request = evalAndLog("request = objectStore.add(test.storedObject);");
    }
    request.onsuccess = getRecord;
    request.onerror = unexpectedErrorCallback;
}

function getRecord()
{
    id = evalAndLog("id = event.target.result;");
    request = evalAndLog("request = objectStore.get(id);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = deleteRecord;
}

function deleteRecord()
{
    shouldBe("test.storedObject.name", "event.target.result.name");

    request = evalAndLog("request = objectStore.delete(id);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = getRecordAgain;
}

function getRecordAgain()
{
    request = evalAndLog("request = objectStore.get(id);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = finalCheck;
}

function finalCheck()
{
    shouldBeTrue("event.target.result === undefined");
    i++;
}
