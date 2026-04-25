if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

const objectStoreName = 'object-store';
const databaseName = 'transaction-abort-on-worker-terminate';
const recordKey = 'record-key';

async function openDatabase()
{
    const openRequest = indexedDB.open(databaseName);
    openRequest.onupgradeneeded = (event) => {
        database = event.target.result;
        database.createObjectStore(objectStoreName);
    }
    return new Promise((resolve, reject) => {
        openRequest.onsuccess = (event) => { resolve(event.target.result); };
        openRequest.onerror = reject;
    });
}

async function getRecord(transaction, key)
{
    const objectStore = transaction.objectStore(objectStoreName);
    const request = objectStore.get(key);
    return new Promise((resolve, reject) => {
        request.onsuccess = resolve;
        request.onerror = reject;
    });
}

async function writeRecord(transaction, key, value)
{
    const objectStore = transaction.objectStore(objectStoreName);
    const request = objectStore.put(value, key);
    return new Promise((resolve, reject) => {
        request.onsuccess = resolve;
        request.onerror = reject;
    });
}

async function deleteRecord(transaction, key)
{
    const objectStore = transaction.objectStore(objectStoreName);
    const request = objectStore.delete(key);
    return new Promise((resolve, reject) => {
        request.onsuccess = resolve;
        request.onerror = reject;
    });
}

async function executeTransaction(database, callback)
{
    let transaction = database.transaction(objectStoreName, 'readwrite');
    await writeRecord(transaction, recordKey, "record-value");
    self.postMessage('continue'); 
    await callback(transaction);
    await deleteRecord(transaction, recordKey);
}

function sleep(duration) 
{
    const start = Date.now();
    while (Date.now() - start < duration) {
        Math.sqrt(Math.random());
    }
}

async function test()
{
    const database = await openDatabase();
    await executeTransaction(database, async (transaction) => {
        sleep(1000);
        // New request is scheduled, so transaction should not be committed automatically.
        const record = await getRecord(transaction, recordKey);
    });
}

test();

