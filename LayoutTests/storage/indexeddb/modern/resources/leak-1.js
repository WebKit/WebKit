description("This tests that certain IDB object relationships don't cause leaks.");

evalAndLog('dbname = "leak-1"');
database = null;

function prepareDatabase() {
    let openRequest = indexedDB.open(dbname);
    openRequest.onupgradeneeded = (event) => {
        debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
        database = openRequest.result;
        versionChangeTransactionObserver = internals.observeGC(openRequest.transaction);
        openRequestObserver = internals.observeGC(openRequest);
        databaseObserver = internals.observeGC(database);
        objectStoreObserver = internals.observeGC(database.createObjectStore("foo"));
    }
    return new Promise((resolve, reject) => {
        openRequest.onsuccess = resolve;
        openRequest.onerror = reject;
    });
}

function performDatabaseOperation() {
    let transaction = database.transaction("foo");
    let promise = new Promise((resolve, reject) => {
        transaction.oncomplete = resolve;
        transaction.onerror = reject;
    });
    let request = transaction.objectStore("foo").get("foo");
    transactionObserver = internals.observeGC(transaction);
    requestObserver = internals.observeGC(request);
    database = null;
    return promise;
}

function testSucceeded()
{
    return transactionObserver.wasCollected 
        && requestObserver.wasCollected 
        && versionChangeTransactionObserver.wasCollected 
        && databaseObserver.wasCollected
        && openRequestObserver.wasCollected
        && objectStoreObserver.wasCollected;
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

async function test()
{
    await prepareDatabase();
    await performDatabaseOperation();

    var gcCountDown = 10;
    while (gcCountDown-- && !testSucceeded()) {
        gc();
        await sleep(100);
    }

    shouldBeTrue("transactionObserver.wasCollected");
    shouldBeTrue("requestObserver.wasCollected");
    shouldBeTrue("versionChangeTransactionObserver.wasCollected");
    shouldBeTrue("databaseObserver.wasCollected");
    shouldBeTrue("openRequestObserver.wasCollected");
    shouldBeTrue("objectStoreObserver.wasCollected");

    finishJSTest();
}

test();