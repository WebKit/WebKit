if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB odd value datatypes");

function test()
{
    removeVendorPrefixes();

    testData = [{ description: 'null',               name: '\u0000' },
                { description: 'faihu',              name: '\ud800\udf46' },
                { description: 'unpaired surrogate', name: '\ud800' },
                { description: 'fffe',               name: '\ufffe' },
                { description: 'ffff',               name: '\uffff' },
                { description: 'line separator',     name: '\u2028' }
    ];
    nextToOpen = 0;
    openNextDatabase();
}

function openNextDatabase()
{
    debug("opening a database named " + testData[nextToOpen].description);
    request = evalAndLog("indexedDB.open(testData[nextToOpen].name)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = openSuccess;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");
    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = addAKey;
    request.onerror = unexpectedErrorCallback;
}

function addAKey()
{
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore(testData[nextToOpen].name);");
    key = evalAndLog("key = testData[nextToOpen].name");
    request = evalAndLog("request = objectStore.add(key, key);");
    request.onsuccess = closeDatabase;
    request.onerror = unexpectedErrorCallback;
}

function closeDatabase()
{
    evalAndLog("db.close()");
    debug("");
    if (++nextToOpen < testData.length) {
        openNextDatabase();
    } else {
        nextToOpen = 0;
        verifyNextDatabase();
    }
}

function verifyNextDatabase()
{
    debug("reopening a database named " + testData[nextToOpen].description);
    request = evalAndLog("indexedDB.open(testData[nextToOpen].name)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = openSuccess2;
}

function openSuccess2()
{
    db = evalAndLog("db = event.target.result");
    request = evalAndLog("request = db.setVersion('1')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = getAKey;
}

function getAKey()
{
    trans = evalAndLog("trans = event.target.result");
    objectStore = evalAndLog("objectStore = trans.objectStore(testData[nextToOpen].name);");
    key = evalAndLog("key = testData[nextToOpen].name");
    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = openCursorSuccess;
}

function openCursorSuccess()
{
    cursor = evalAndLog("cursor = event.target.result;");
    shouldBe("cursor.key", "testData[nextToOpen].name");
    shouldBe("cursor.value", "testData[nextToOpen].name");
    if (++nextToOpen < testData.length) {
        debug("");
        verifyNextDatabase();
    } else {
        finishJSTest();
    }
}

test();
