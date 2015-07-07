// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_objectCursors.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's index cursors");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    autoIncrement = evalAndLog("autoIncrement = false;");
    objectStore = evalAndLog("objectStore = db.createObjectStore('a', { keyPath: 'id', autoIncrement: autoIncrement });");
    indexes = [
        { name: "a", options: { } },
        { name: "b", options: { unique: true } }
    ];
    for (j in indexes) {
        evalAndLog("objectStore.createIndex(indexes[j].name, 'name', indexes[j].options);");
    }
    data = evalAndLog("data = { name: 'Ben', id: 1 };");
    request = evalAndLog("request = objectStore.add(data);");
    request.onsuccess = postAdd;
    request.onerror = unexpectedErrorCallback;
}

function postAdd()
{
    numIndexes = indexes.length;
    indexesFinished = 0;
    for (j in indexes) {
        index = evalAndLog("index = objectStore.index(indexes[j].name);");
        request = evalAndLog("request = index.openCursor();");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(event) {
            shouldBe("event.target.result.value.name", "'Ben'");
            if (++indexesFinished == numIndexes) {
                if (autoIncrement) {
                    finishJSTest();
                } else {
                    setupAutoIncrement();
                }
            }
        };
    }
}

function setupAutoIncrement()
{
    autoIncrement = evalAndLog("autoIncrement = true;");
    objectStore = evalAndLog("objectStore = db.createObjectStore('b', { keyPath: 'id', autoIncrement: autoIncrement });");
    for (j in indexes) {
        evalAndLog("objectStore.createIndex(indexes[j].name, 'name', indexes[j].options);");
    }
    data = evalAndLog("data = { name: 'Ben' };");
    request = evalAndLog("request = objectStore.add(data);");
    request.onsuccess = postAdd;
    request.onerror = unexpectedErrorCallback;
}
