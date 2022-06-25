if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure all databases can be retrieved");

var databases;

function test() {
    indexedDB.deleteDatabase('getdatabase-first');
    indexedDB.deleteDatabase('getdatabase-second');

    request = evalAndLog("indexedDB.open('getdatabase-first')");
    request.onupgradeneeded = () => {
        indexedDB.databases().then((result)=> {
            databases = result;
            shouldBeEqualToNumber("databases.length", 0);
            continueTest();
        });
    }
}

function continueTest() {
    request = evalAndLog("indexedDB.open('getdatabase-second', 2)");
    request.onupgradeneeded = () => {
        indexedDB.databases().then((result) => {
            databases = result;
            shouldBeEqualToNumber("databases.length", 1);
            shouldBeEqualToNumber("databases[0].version", 1)
            shouldBeEqualToString("databases[0].name", "getdatabase-first");
        });
    }
    request.onsuccess = () => {
        indexedDB.databases().then((result) => {
            databases = result;
            shouldBeEqualToNumber("databases.length", 2);
            shouldBeEqualToNumber("databases[0].version", 2);
            shouldBeEqualToString("databases[0].name", "getdatabase-second");
            finishJSTest();
        });
    }
}

test();
