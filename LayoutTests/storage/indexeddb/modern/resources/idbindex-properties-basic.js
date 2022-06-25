description("This tests getting basic properties on an IDBIndex.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

function gol(message)
{
    debug(message);
}

function logIndex(index)
{
    debug(index.name);
    debug(index.objectStore);
    debug(index.objectStore.name);
    debug(index.keyPath);
    debug(index.multiEntry);
    debug(index.unique);
}

var database;

var indexes = new Array();

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    
    indexes.push(objectStore.createIndex("TestIndex1", "foo"));
    indexes.push(objectStore.createIndex("TestIndex2", "foo", { unique: false, multiEntry: false }));
    indexes.push(objectStore.createIndex("TestIndex3", "foo", { unique: true, multiEntry: false }));
    indexes.push(objectStore.createIndex("TestIndex4", "foo", { unique: false, multiEntry: true }));
    indexes.push(objectStore.createIndex("TestIndex5", "foo", { unique: true, multiEntry: true }));
    indexes.push(objectStore.createIndex("TestIndex6", [ "foo" ]));
    indexes.push(objectStore.createIndex("TestIndex7", [ "foo", "bar" ]));

    for (index in indexes)
        logIndex(indexes[index]);

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function continueTest1()
{
    var transaction = database.transaction("TestObjectStore", "readonly");
    var objectStore = transaction.objectStore("TestObjectStore");

    logIndex(objectStore.index("TestIndex1"));
    logIndex(objectStore.index("TestIndex2"));
    logIndex(objectStore.index("TestIndex3"));
    logIndex(objectStore.index("TestIndex4"));
    logIndex(objectStore.index("TestIndex5"));
    logIndex(objectStore.index("TestIndex6"));
    logIndex(objectStore.index("TestIndex7"));

    transaction.onabort = function(event) {
        debug("readonly transaction unexpected abort" + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readonly transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readonly transaction unexpected error" + event);
        done();
    }
}
