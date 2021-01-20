if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
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
    request.onsuccess = ++nextToOpen < testData.length ? openNextDatabase : finishJSTest;
}

test();
