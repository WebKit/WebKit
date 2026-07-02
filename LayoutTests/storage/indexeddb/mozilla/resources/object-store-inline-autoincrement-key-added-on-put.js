// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_objectStore_inline_autoincrement_key_added_on_put.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB adding an autoincremented key and retrieving it successfully");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    test = evalAndLog("test = {\n" +
"        name: 'inline key; key generator',\n" +
"        autoIncrement: true,\n" +
"        storedObject: { name: 'Lincoln' },\n" +
"        keyName: 'id',\n" +
"    };");
    objectStore = evalAndLog("objectStore = db.createObjectStore(test.name, { keyPath: test.keyName, autoIncrement: test.autoIncrement });");
    request = evalAndLog("request = objectStore.add(test.storedObject);");
    request.onsuccess = addSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addSuccess()
{
    id = evalAndLog("id = event.target.result;");
    request = evalAndLog("request = objectStore.get(id);");
    request.onsuccess = getSuccess;
    request.onerror = unexpectedErrorCallback;
}

function getSuccess()
{
    shouldBe("event.target.result.name", "test.storedObject.name");
    shouldBe("event.target.result.id", "id");
    finishJSTest();
}
