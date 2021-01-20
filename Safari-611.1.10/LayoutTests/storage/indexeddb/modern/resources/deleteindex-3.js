description("This tests creating an index, deleting it, creating a new one with the same name, and making sure those two indexes aren't equal.");

indexedDBTest(prepareDatabase);

function prepareDatabase(event)
{
    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    objectStore.put({ foo: "a", bar: "A" }, 1);
    objectStore.put({ foo: "b", bar: "B" }, 2);

    var index1 = objectStore.createIndex("FooIndex", "foo");
    objectStore.deleteIndex("FooIndex");
    var index2 = objectStore.createIndex("FooIndex", "bar", { unique: true });

    debug("First and Second indexes are the same: " + (index1 == index2));
    debug("First index keyPath: " + index1.keyPath);
    debug("Second index keyPath: " + index2.keyPath);
    debug("First index unique: " + index1.unique);
    debug("Second index unique: " + index2.unique);
    debug("First index references back to objectStore: " + (index1.objectStore == objectStore));
    debug("Second index references back to objectStore: " + (index2.objectStore == objectStore));
    debug("objectStore's index for 'FooIndex' is equal to first index: " + (objectStore.index("FooIndex") == index1));
    debug("objectStore's index for 'FooIndex' is equal to second index: " + (objectStore.index("FooIndex") == index2));
    
    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected abort");
        finishJSTest();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        finishJSTest();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        finishJSTest();
    }
}
