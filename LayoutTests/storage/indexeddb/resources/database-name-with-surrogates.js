if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that IndexedDB correctly handles database names containing unpaired UTF-16 surrogates");

// Per Web Standards: DOMString = sequence of UTF-16 code units (can include unpaired surrogates).
// IndexedDB database names are DOMString, so they can contain unpaired surrogates.

const testCases = [
    { name:"Normal", databaseName:"db1" },
    { name:"Unpaired high surrogate", databaseName:"db2_" + String.fromCharCode(0xD9B2) + "test" },
    { name:"Unpaired low surrogate", databaseName:"db3_" + String.fromCharCode(0xDC00) + "test" },
    { name:"Valid surrogate pair (😀)", databaseName:"db4_" + + String.fromCharCode(0xD83D, 0xDE00) + "test" }
];
var index = 0;
var databaseName, database;

function test() {
    if (index >= testCases.length) {
        testIDBFactoryDatabases();
        return;
    }

    testCase = testCases[index++];
    debug("");
    debug("Test " + index + ": " + testCase.name);
    databaseName = testCase.databaseName;
    indexedDB.deleteDatabase(databaseName);
    request = evalAndLog("indexedDB.open(databaseName)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(event) {
        database = event.target.result;
        evalAndLog("actualName = database.name");
        shouldBeEqualToString("actualName", databaseName);
        evalAndLog("database.close()");
        request2 = evalAndLog("indexedDB.open(databaseName)");
        request2.onerror = unexpectedErrorCallback;
        request2.onsuccess = function(event) {
            database = event.target.result;
            evalAndLog("actualName = database.name");
            shouldBeEqualToString("actualName", databaseName);
            database.close();
            test();
        };
    };
}

function testIDBFactoryDatabases() {
    debug("");
    debug("Test all database names");

    promise = evalAndLog("indexedDB.databases()");
    promise.then((databases) => {
        actualDatabases = databases;
        shouldBe("actualDatabases.length", "4");
        actualDatabases.sort((a, b) => a.name > b.name);
        for (index = 0; index <actualDatabases.length; ++index)
            shouldBeEqualToString("actualDatabases["+ index +"].name", testCases[index].databaseName);
        finishJSTest();
    })
}

test();
