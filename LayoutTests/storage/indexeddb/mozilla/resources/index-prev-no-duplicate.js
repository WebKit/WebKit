//  original test: http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_indexes.html?force=1
//  license of original test:
//    " Any copyright is dedicated to the Public Domain.
//      http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB: iterating backwards through an index, skipping duplicates");

indexedDBTest(prepareDatabase, testPrev);
function prepareDatabase()
{
    db = event.target.result;
    trans = event.target.transaction;

    objectStoreName = "People";

    objectStoreData = [
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7737", value: { name: "Pat", height: 65, weight: 100 } },
        { key: "237-23-7738", value: { name: "Leo", height: 65, weight: 180 } },
        { key: "237-23-7739", value: { name: "Jef", height: 65, weight: 120 } },
        { key: "237-23-7740", value: { name: "Sam", height: 73, weight: 110 } },
    ];

    indexData = [
        { name: "name", keyPath: "name", options: { unique: true } },
        { name: "height", keyPath: "height", options: { } },
        { name: "weight", keyPath: "weight", options: { unique: false } }
    ];

    objectStoreDataHeightSort = [
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7737", value: { name: "Pat", height: 65, weight: 100 } },
        { key: "237-23-7738", value: { name: "Leo", height: 65, weight: 180 } },
        { key: "237-23-7739", value: { name: "Jef", height: 65, weight: 120 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
        { key: "237-23-7740", value: { name: "Sam", height: 73, weight: 110 } },
    ];

    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName);");
    debug("First, add all our data to the object store.");
    addedData = 0;
    for (i in objectStoreData) {
      request = evalAndLog("request = objectStore.add(objectStoreData[i].value, objectStoreData[i].key);");
      request.onerror = unexpectedErrorCallback;
      request.onsuccess = function(event)
      {
        if (++addedData == objectStoreData.length) {
          createIndexes();
        }
      }
    }
    trans.oncomplete = function() {
        evalAndLog("trans = db.transaction(objectStoreName, 'readwrite')");
        evalAndLog("objectStore = trans.objectStore(objectStoreName)");
    };
}

function createIndexes()
{
    debug("Now create the indexes.");
    for (i in indexData) {
      evalAndLog("objectStore.createIndex(indexData[i].name, indexData[i].keyPath, indexData[i].options);");
    }
}

function testPrev()
{
    debug("testPrev()");
    trans = evalAndLog("trans = db.transaction(objectStoreName)");
    objectStore = evalAndLog("objectStore = trans.objectStore(objectStoreName);");
    keyIndex = evalAndLog("keyIndex = 8;");
    // first try with just PREV
    request = evalAndLog("request = objectStore.index('height').openCursor(null, 'prev');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(event)
    {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "" + objectStoreDataHeightSort[keyIndex].value.height);
            shouldBe("cursor.primaryKey", "'" + objectStoreDataHeightSort[keyIndex].key + "'");
            shouldBe("cursor.value.name", "'" + objectStoreDataHeightSort[keyIndex].value.name + "'");
            shouldBe("cursor.value.height", "" + objectStoreDataHeightSort[keyIndex].value.height);
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "" + objectStoreDataHeightSort[keyIndex].value.weight);
            }

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex--;");
            debug("  => " + keyIndex);
        }
        else {
            debug("No cursor: " + cursor);
            shouldBe("keyIndex", "-1");
        }
    };
    trans.oncomplete = testPrevNoDuplicate;
}

function testPrevNoDuplicate()
{
    debug("testPrevNoDuplicate()");
    objectStore = evalAndLog("objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);");
    keyIndex = evalAndLog("keyIndex = 8;");
    request = evalAndLog("request = objectStore.index('height').openCursor(null, 'prevunique');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event)
    {
        cursor = evalAndLog("cursor = event.target.result;");
            if (keyIndex == 8) {
                evalAndLog("keyIndex -= 1");
            }
            if (keyIndex == 6) {
                evalAndLog("keyIndex -= 3;");
            }
        debug("  => Entering with keyIndex = " + keyIndex);
        if (cursor) {
            shouldBe("cursor.key", "" + objectStoreDataHeightSort[keyIndex].value.height);
            shouldBe("cursor.primaryKey", "'" + objectStoreDataHeightSort[keyIndex].key + "'");
            shouldBe("cursor.value.name", "'" + objectStoreDataHeightSort[keyIndex].value.name + "'");
            shouldBe("cursor.value.height", "" + objectStoreDataHeightSort[keyIndex].value.height);
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "" + objectStoreDataHeightSort[keyIndex].value.weight);
            }

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex--;");
        }
        else {
            debug("No cursor: " + cursor);
            shouldBe("keyIndex", "-1");
            finishJSTest();
        }
    }
}

var successfullyParsed = true;
