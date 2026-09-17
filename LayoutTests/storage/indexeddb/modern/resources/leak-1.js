description("This tests that certain IDB object relationships don't cause leaks.");

const databaseCount = 20;

evalAndLog('dbnamePrefix = "leak-1"');

databases = [];
versionChangeTransactionRefs = [];
openRequestRefs = [];
databaseRefs = [];
objectStoreRefs = [];
transactionRefs = [];
requestRefs = [];

function prepareDatabase(index) {
    let dbname = dbnamePrefix + "-" + index;
    let deleteRequest = indexedDB.deleteDatabase(dbname);
    return new Promise((resolve, reject) => {
        deleteRequest.onerror = reject;
        deleteRequest.onsuccess = () => {
            let openRequest = indexedDB.open(dbname);
            openRequest.onupgradeneeded = () => {
                let database = openRequest.result;
                databases[index] = database;
                versionChangeTransactionRefs.push(new WeakRef(openRequest.transaction));
                openRequestRefs.push(new WeakRef(openRequest));
                databaseRefs.push(new WeakRef(database));
                objectStoreRefs.push(new WeakRef(database.createObjectStore("foo")));
            }
            openRequest.onsuccess = () => resolve();
            openRequest.onerror = reject;
        };
    });
}

function performDatabaseOperation(index) {
    let transaction = databases[index].transaction("foo");
    let promise = new Promise((resolve, reject) => {
        transaction.oncomplete = () => resolve();
        transaction.onerror = reject;
    });
    let request = transaction.objectStore("foo").get("foo");
    transactionRefs.push(new WeakRef(transaction));
    requestRefs.push(new WeakRef(request));
    databases[index] = null;
    return promise;
}

function forEachDatabase(callback)
{
    return Promise.all(Array.from({ length: databaseCount }, (unused, index) => callback(index)));
}

async function test()
{
    debug("Open " + databaseCount + " databases and run a transaction against each.");
    await forEachDatabase(prepareDatabase);
    await forEachDatabase(performDatabaseOperation);
    nukeArray(databases);

    collected = await gcUntil(() => anyCollected(versionChangeTransactionRefs)
        && anyCollected(openRequestRefs)
        && anyCollected(databaseRefs)
        && anyCollected(objectStoreRefs)
        && anyCollected(transactionRefs)
        && anyCollected(requestRefs));
    shouldBeTrue("collected");

    finishJSTest();
}

test();
