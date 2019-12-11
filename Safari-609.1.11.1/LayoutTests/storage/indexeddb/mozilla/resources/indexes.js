// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_indexes.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB: iterating through index cursors with keys and key ranges");

indexedDBTest(prepareDatabase, setVersionComplete);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStoreName = "People";

    objectStoreData = [
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7737", value: { name: "Pat", height: 65, weight: 100 } }
    ];

    indexData = [
        { name: "name", keyPath: "name", options: { unique: true } },
        { name: "height", keyPath: "height", options: { } },
        { name: "weight", keyPath: "weight", options: { unique: false } }
    ];

    objectStoreDataNameSort = [
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7737", value: { name: "Pat", height: 65, weight: 100 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } }
    ];

    objectStoreDataWeightSort = [
        { key: "237-23-7737", value: { name: "Pat", height: 65, weight: 100 } },
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
    ];

    objectStoreDataHeightSort = [
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7737", value: { name: "Pat", height: 65, weight: 100 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
    ];

    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName);");

    debug("First, add all our data to the object store.");
    addedData = 0;
    for (i in objectStoreData) {
      request = evalAndLog("request = objectStore.add(objectStoreData[i].value, objectStoreData[i].key);");
      request.onerror = unexpectedErrorCallback;
      request.onsuccess = function(event) {
        if (++addedData == objectStoreData.length) {
          createIndexes();
        }
      }
    }
}

function createIndexes()
{
    debug("Now create the indexes.");
    for (i in indexData) {
      evalAndLog("objectStore.createIndex(indexData[i].name, indexData[i].keyPath, indexData[i].options);");
    }
    shouldBe("objectStore.indexNames.length", "indexData.length");
}

function setVersionComplete()
{
    objectStore = evalAndLog("objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);");

    // Check global properties to make sure they are correct.
    shouldBe("objectStore.indexNames.length", "indexData.length");
    for (i in indexData) {
        found = false;
        for (j = 0; j < objectStore.indexNames.length; j++) {
            if (objectStore.indexNames.item(j) == indexData[i].name) {
                found = true;
                break;
            }
        }
        shouldBeTrue("found");
        index = evalAndLog("index = objectStore.index(indexData[i].name);");
        shouldBe("index.name", "indexData[i].name");
        shouldBe("index.keyPath", "indexData[i].keyPath");
        shouldBe("index.unique", indexData[i].options.unique ? "true" : "false");
    }

    request = evalAndLog("request = objectStore.index('name').getKey('Bob');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = checkGetKey;
}

function checkGetKey()
{
    shouldBe("event.target.result", "'237-23-7732'");

    request = evalAndLog("request = objectStore.index('name').get('Bob');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = checkGet;
}

function checkGet()
{
    shouldBe("event.target.result.name", "'Bob'");
    shouldBe("event.target.result.height", "60");
    shouldBe("event.target.result.weight", "120");

    keyIndex = 0;

    request = evalAndLog("request = objectStore.index('name').openKeyCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBeFalse("'value' in cursor");

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBeFalse("'value' in cursor");

            evalAndLog("keyIndex++;");
        }
        else {
          shouldBe("keyIndex", "objectStoreData.length");
          testGroup3();
        }
    };
}

function testGroup3()
{
    keyIndex = 0;

    request = evalAndLog("request = objectStore.index('weight').openKeyCursor(null, 'next');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataWeightSort[keyIndex].value.weight");
            shouldBe("cursor.primaryKey", "objectStoreDataWeightSort[keyIndex].key");

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataWeightSort[keyIndex].value.weight");
            shouldBe("cursor.primaryKey", "objectStoreDataWeightSort[keyIndex].key");

            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "objectStoreData.length");
            testGroup4();
        }
    }
}

function testGroup4()
{
    objectStore = evalAndLog("objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);");
    keyIndex = evalAndLog("keyIndex = objectStoreDataNameSort.length - 1;");
    request = evalAndLog("request = objectStore.index('name').openKeyCursor(null, 'prev');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("keyIndex--;");
        }
        else {
            shouldBe("keyIndex", "-1");
            testGroup5();
        }
    }
}

function testGroup5()
{
    keyIndex = 1;
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron');");
    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "5");
            testGroup6();
        }
    }
}

function testGroup6()
{
    keyIndex = evalAndLog("keyIndex = 2;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron', true);");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "5");
            testGroup7();
        }
    }
}

function testGroup7()
{
    keyIndex = evalAndLog("keyIndex = 1;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron', false, true);");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "4");
            testGroup8();
        }
    }
}

function testGroup8()
{
    keyIndex = evalAndLog("keyIndex = 2;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron', true, true);");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "4");
            testGroup9();
        }
    }
}

function testGroup9()
{
    keyIndex = evalAndLog("keyIndex = 1;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.lowerBound('Bob');");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "objectStoreDataNameSort.length");
            testGroup10();
        }
    }
}

function testGroup10()
{
    keyIndex = evalAndLog("keyIndex = 2;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.lowerBound('Bob', true);");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "objectStoreDataNameSort.length");
            testGroup11();
        }
    }
}

function testGroup11()
{
    keyIndex = evalAndLog("keyIndex = 0;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.upperBound('Joe');");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
          shouldBe("keyIndex", "3");
          testGroup12();
        }
    }
}

function testGroup12()
{
    keyIndex = evalAndLog("keyIndex = 0;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.upperBound('Joe', true);");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "2");
            testGroup13();
        }
    }
}

function testGroup13()
{
    keyIndex = evalAndLog("keyIndex = 3;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.only('Pat');");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "4");
            testGroup14();
        }
    }
}

function testGroup14()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    request = evalAndLog("request = objectStore.index('name').openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "objectStoreDataNameSort.length");
            testGroup15();
        }
    }
}

function testGroup15()
{
    keyIndex = evalAndLog("keyIndex = objectStoreDataNameSort.length - 1;");

    request = evalAndLog("request = objectStore.index('name').openCursor(null, 'prev');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            keyIndex--;
        }
        else {
            shouldBe("keyIndex", "-1");
            testGroup16();
        }
    }
}

function testGroup16()
{
    keyIndex = evalAndLog("keyIndex = 1;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron');");

    request = evalAndLog("request = objectStore.index('name').openCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "5");
            testGroup17();
        }
    }
}

function testGroup17()
{
    keyIndex = evalAndLog("keyIndex = 2;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron', true);");

    request = evalAndLog("request = objectStore.index('name').openCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "5");
            testGroup18();
        }
    }
}

function testGroup18()
{
    keyIndex = evalAndLog("keyIndex = 1;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron', false, true);");

    request = evalAndLog("request = objectStore.index('name').openCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "4");
            testGroup19();
        }
    }
}

function testGroup19()
{
    keyIndex = evalAndLog("keyIndex = 2;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron', true, true);");

    request = evalAndLog("request = objectStore.index('name').openCursor(keyRange);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "4");
            testGroup20();
        }
    }
}

function testGroup20()
{
    keyIndex = evalAndLog("keyIndex = 4;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.bound('Bob', 'Ron');");

    request = evalAndLog("request = objectStore.index('name').openCursor(keyRange, 'prev');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            evalAndLog("keyIndex--;");
        }
        else {
            shouldBe("keyIndex", "0");
            testGroup21();
        }
    }
}

function testGroup21()
{
    keyIndex = evalAndLog("keyIndex = 3;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.only(65);");

    request = evalAndLog("request = objectStore.index('height').openKeyCursor(keyRange, 'next');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataHeightSort[keyIndex].value.height");
            shouldBe("cursor.primaryKey", "objectStoreDataHeightSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "5");
            testGroup22();
        }
    }
}

function testGroup22()
{
    keyIndex = evalAndLog("keyIndex = 3;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.only(65);");

    request = evalAndLog("request = objectStore.index('height').openKeyCursor(keyRange, 'nextunique');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataHeightSort[keyIndex].value.height");
            shouldBe("cursor.primaryKey", "objectStoreDataHeightSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "4");
            testGroup24();
        }
    }
}

/* this was split out into a separate test
function testGroup23()
{
    keyIndex = evalAndLog("keyIndex = 5;");

    request = evalAndLog("request = objectStore.index('height').openKeyCursor(null, 'prevunique');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataHeightSort[keyIndex].value.height");
            shouldBe("cursor.primaryKey", "objectStoreDataHeightSort[keyIndex].key");

            evalAndLog("cursor.continue();");
            if (keyIndex == 5) {
                evalAndLog("keyIndex--;");
            }
            evalAndLog("keyIndex--;");
        }
        else {
            shouldBe("keyIndex", "-1");
            testGroup24();
        }
    }
}
*/

function testGroup24()
{
    keyIndex = evalAndLog("keyIndex = 3;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.only(65);");

    request = evalAndLog("request = objectStore.index('height').openCursor(keyRange, 'next');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataHeightSort[keyIndex].value.height");
            shouldBe("cursor.primaryKey", "objectStoreDataHeightSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataHeightSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataHeightSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "objectStoreDataHeightSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "5");
            testGroup25();
        }
    }
}

function testGroup25()
{
    keyIndex = evalAndLog("keyIndex = 3;");
    keyRange = evalAndLog("keyRange = IDBKeyRange.only(65);");

    request = evalAndLog("request = objectStore.index('height').openCursor(keyRange, 'nextunique');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataHeightSort[keyIndex].value.height");
            shouldBe("cursor.primaryKey", "objectStoreDataHeightSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataHeightSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataHeightSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "objectStoreDataHeightSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");
            evalAndLog("keyIndex++;");
        }
        else {
            shouldBe("keyIndex", "4");
            testGroup27();
        }
    }
}

/* this was split out into a separate test
function testGroup26()
{
    keyIndex = evalAndLog("keyIndex = 5;");

    request = evalAndLog("request = objectStore.index('height').openCursor(null, 'prevunique');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataHeightSort[keyIndex].value.height");
            shouldBe("cursor.primaryKey", "objectStoreDataHeightSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataHeightSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataHeightSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                shouldBe("cursor.value.weight", "objectStoreDataHeightSort[keyIndex].value.weight");
            }

            evalAndLog("cursor.continue();");
            if (keyIndex == 5) {
                evalAndLog("keyIndex--;");
            }
            evalAndLog("keyIndex--;");
        }
        else {
            shouldBe("keyIndex", "-1");
            testGroup27();
        }
    }
}
*/

function testGroup27()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            nextKey = evalAndLog("nextKey = !keyIndex ? 'Pat' : undefined;");
            if (nextKey) {
                evalAndLog("cursor.continue(nextKey);");
            } else {
                evalAndLog("cursor.continue();");
            }

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            if (!keyIndex) {
                keyIndex = evalAndLog("keyIndex = 3;");
            }
            else {
                evalAndLog("keyIndex++;");
            }
        }
        else {
            shouldBe("keyIndex", "objectStoreData.length");
            testGroup28();
        }
    }
}

function testGroup28()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    request = evalAndLog("request = objectStore.index('name').openKeyCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            nextKey = evalAndLog("nextKey = !keyIndex ? 'Flo' : undefined;");
            if (nextKey) {
                evalAndLog("cursor.continue(nextKey);");
            } else {
                evalAndLog("cursor.continue();");
            }

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");

            keyIndex += keyIndex ? 1 : 2;
        }
        else {
            shouldBe("keyIndex", "objectStoreData.length");
            testGroup29();
        }
    }
}

function testGroup29()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    request = evalAndLog("request = objectStore.index('name').openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            nextKey = evalAndLog("nextKey = !keyIndex ? 'Pat' : undefined;");
            if (nextKey) {
                evalAndLog("cursor.continue(nextKey);");
            } else {
                evalAndLog("cursor.continue();");
            }

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            if (!keyIndex) {
                keyIndex = evalAndLog("keyIndex = 3;");
            }
            else {
                evalAndLog("keyIndex++;");
            }
        }
        else {
            shouldBe("keyIndex", "objectStoreDataNameSort.length");
            testGroup30();
        }
    }
}

function testGroup30()
{
    keyIndex = evalAndLog("keyIndex = 0;");

    request = evalAndLog("request = objectStore.index('name').openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (event) {
        cursor = evalAndLog("cursor = event.target.result;");
        if (cursor) {
            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            nextKey = evalAndLog("nextKey = !keyIndex ? 'Flo' : undefined;");
            if (nextKey) {
                evalAndLog("cursor.continue(nextKey);");
            } else {
                evalAndLog("cursor.continue();");
            }

            shouldBe("cursor.key", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.primaryKey", "objectStoreDataNameSort[keyIndex].key");
            shouldBe("cursor.value.name", "objectStoreDataNameSort[keyIndex].value.name");
            shouldBe("cursor.value.height", "objectStoreDataNameSort[keyIndex].value.height");
            if ('weight' in cursor.value) {
                  shouldBe("cursor.value.weight", "objectStoreDataNameSort[keyIndex].value.weight");
            }

            keyIndex += keyIndex ? 1 : 2;
        }
        else {
            shouldBe("keyIndex", "objectStoreDataNameSort.length");
            finishJSTest();
        }
    }
}
