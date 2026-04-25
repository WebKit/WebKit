// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_cursor_update_updates_indexes.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB: mutating records with a r/w cursor updates indexes on those records");

firstValue = evalAndLog("firstValue = 'hi';");
secondValue = evalAndLog("secondValue = 'bye';");
objectStoreInfo = evalAndLog("objectStoreInfo = [\n" +
"        { name: '1', options: {}, key: 1,\n" +
"          entry: { data: firstValue } },\n" +
"        { name: '2', options: { keyPath: 'foo' },\n" +
"          entry: { foo: 1, data: firstValue } },\n" +
"        { name: '3', options: { autoIncrement: true },\n" +
"          entry: { data: firstValue } },\n" +
"        { name: '4', options: { keyPath: 'foo', autoIncrement: true },\n" +
"          entry: { data: firstValue } },\n" +
"    ];");
i = evalAndLog("i = 0;");
debug("");

test();

function test()
{
    if (i < objectStoreInfo.length) {
        indexedDBTest(setupObjectStoreAndCreateIndexAndAdd, openCursor);
        info = evalAndLog("info = objectStoreInfo[i];");
    } else {
        finishJSTest();
    }
}

function setupObjectStoreAndCreateIndexAndAdd()
{
    debug("setupObjectStoreAndCreateIndex():");
    evalAndLog("db = event.target.result");

    objectStore = evalAndLog("objectStore = db.createObjectStore(info.name, info.options);");

    index = evalAndLog("index = objectStore.createIndex('data_index', 'data', { unique: false });");
    uniqueIndex = evalAndLog("uniqueIndex = objectStore.createIndex('unique_data_index', 'data', { unique: true });");
    if (info.key) {
        request = evalAndLog("request = objectStore.add(info.entry, info.key);");
    } else {
        request = evalAndLog("request = objectStore.add(info.entry);");
    }
    request.onerror = unexpectedErrorCallback;
}

function openCursor()
{
    evalAndLog("trans = db.transaction(info.name, 'readwrite')");
    evalAndLog("trans.oncomplete = test");
    trans.onabort = unexpectedAbortCallback;
    evalAndLog("objectStore = trans.objectStore(info.name)");
    evalAndLog("index = objectStore.index('data_index')");
    evalAndLog("uniqueIndex = objectStore.index('unique_data_index')");
    request = evalAndLog("request = objectStore.openCursor();");
    request.onsuccess = updateCursor;
    request.onerror = unexpectedErrorCallback;
}

function updateCursor()
{
    cursor = evalAndLog("cursor = request.result;");
    value = evalAndLog("value = cursor.value;");
    value.data = evalAndLog("value.data = secondValue;");
    request = evalAndLog("request = cursor.update(value);");
    request.onsuccess = getIndex1;
    request.onerror = unexpectedErrorCallback;
}

function getIndex1()
{
    request = evalAndLog("request = index.get(secondValue);");
    request.onsuccess = checkIndex1AndGetIndex2;
    request.onerror = unexpectedErrorCallback;
}

function checkIndex1AndGetIndex2()
{
    shouldBe("value.data", "event.target.result.data");
    request = evalAndLog("request = uniqueIndex.get(secondValue);");
    request.onsuccess = checkIndex2;
    request.onerror = unexpectedErrorCallback;
}

function checkIndex2()
{
    shouldBe("value.data", "event.target.result.data");
    evalAndLog("i++;");
    evalAndLog("db.close()");
}
