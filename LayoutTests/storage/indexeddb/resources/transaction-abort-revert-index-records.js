if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test aborting transaction reverts changes maded to index records");

indexedDBTest(prepareDatabase, onDatabaseOpen);

var recordsCount, indexRecordKey, indexRecordValue;

const originalIndexRecords = [
    { 
        name: "timestampIndex", count: 4, records: [
            { key: 101, values: ["key1"] }, 
            { key: 102, values: ["key2"] }, 
            { key: 103, values: ["key3"] }, 
            { key: 104, values: ["key4"] }
        ]
    },
    { 
        name: "groupIndex", count: 3, records: [
            { key: "a", values: ["key1"] }, 
            { key: "b", values: ["key2", "key3"] }
        ] 
    },
    { 
        name: "groupCountIndex", count: 3, records: [
            { key: ["a", 1], values: ["key1"] }, 
            { key: ["b", 2], values: ["key2", "key3"] }
        ]
    },
    { 
        name: "categoriesIndex", count: 6, 
        records: [
            { key: "xx", values: ["key1", "key4"] }, 
            { key: "yy", values: ["key2", "key4"] }, 
            { key: "zz", values: ["key3", "key4"] }
        ] 
    }
];

const updatedIndexRecords = [
    { 
        name: "timestampIndex", count: 4, records: [
            { key: 101, values: ["key1"] }, 
            { key: 102, values: ["key2"] }, 
            { key: 105, values: ["key5"] },
            { key: 106, values: ["key3"] },
        ]
    },
    { 
        name: "groupIndex", count: 4, records: [
            { key: "a", values: ["key1"] }, 
            { key: "b", values: ["key2"] },
            { key: "c", values: ["key3", "key5"] },
        ] 
    },
    { 
        name: "groupCountIndex", count: 4, records: [
            { key: ["a", 1], values: ["key1"] }, 
            { key: ["b", 2], values: ["key2"] },
            { key: ["c", 3], values: ["key3", "key5"] }
        ] 
    },
    { 
        name: "categoriesIndex", count: 5, 
        records: [
            { key: "ww", values: ["key3", "key5"] },
            { key: "xx", values: ["key1"] }, 
            { key: "yy", values: ["key2", "key3"] }
        ] 
    }
];

async function getAllKeys(index, key)
{
    promise = new Promise((resolve, reject) => {
        if (!index)
            reject('index is not valid.');
        else {
            request = index.getAllKeys(key);
            request.onsuccess = (event) => { resolve(event.target.result); }
            request.onerror = (event) => { reject(event.target.error); }
        }
    });
    result = await promise;
    return result;
}

async function getIndexCount(index)
{
    promise = new Promise((resolve, reject) => {
        if (!index)
            reject('index is not valid.');
        else {
            request = index.count();
            request.onsuccess = (event) => { resolve(event.target.result); }
            request.onerror = (event) => { reject(event.target.error); }
        }
    });
    count = await promise;
    return count;
}

async function validateIndexRecords(indexName, expectdRecordsCount, expectedRecords)
{
    debug("");
    debug("Validating records for: " + indexName);

    index = evalAndLog("index = store.index('" + indexName + "')");
    recordsCount = await getIndexCount(index);
    shouldBe("recordsCount", expectdRecordsCount + "");

    for (const record of expectedRecords) {
        indexRecordKey = record.key;
        debug("will getAllKeys");
        result = await getAllKeys(index, indexRecordKey);
        debug("done getAllKeys");
        indexRecordValues = JSON.stringify(result);
        debug("Getting index record with key: " + indexRecordKey);
        shouldBeEqualToString("indexRecordValues", JSON.stringify(record.values));
    }
}

async function validateAllIndexRecords(indexes, callback)
{
    for (const index of indexes)
        await validateIndexRecords(index.name, index.count, index.records);

    if (callback)
        callback();
}

function prepareDatabase()
{
    preamble(event);

    database = event.target.result;
    evalAndLog("store = database.createObjectStore('store')");
    evalAndLog("store.createIndex('timestampIndex', 'timestamp', { unique: true })");
    evalAndLog("store.createIndex('groupIndex', 'group', { unique: false })");
    evalAndLog("store.createIndex('groupCountIndex', ['group', 'count'])");
    evalAndLog("store.createIndex('categoriesIndex', 'categories', { multiEntry: true })");

    evalAndLog("store.add({ timestamp: 101, group: 'a', count: 1, categories: 'xx' }, 'key1')");
    evalAndLog("store.add({ timestamp: 102, group: 'b', count: 2, categories: 'yy' }, 'key2')");
    evalAndLog("store.add({ timestamp: 103, group: 'b', count: 2, categories: 'zz' }, 'key3')");
    evalAndLog("store.add({ timestamp: 104, categories: ['xx', 'yy', 'zz'] }, 'key4')");

    validateAllIndexRecords(originalIndexRecords);
}

function onDatabaseOpen()
{
    preamble(event);

    // Start a new transaction to modify index records.
    database = event.target.result;
    transaction = evalAndLog("transcation = database.transaction('store', 'readwrite')");
    transaction.onabort = onTransactionAbort;
    evalAndLog("store = transcation.objectStore('store')");
    evalAndLog("store.add({ timestamp: 105, group: 'c', count: 3, categories: 'ww' }, 'key5')");
    evalAndLog("store.put({ timestamp: 106, group: 'c', count: 3, categories: ['ww', 'yy'] }, 'key3')");
    evalAndLog("store.delete('key4')");

    validateAllIndexRecords(updatedIndexRecords, abortTransaction);
}

function abortTransaction()
{
    // Adding a record with existing key will cause request to fail and transaction to abort.
    evalAndLog("store.add({ timestamp: 107 }, 'key1')");
}

function onTransactionAbort()
{
    preamble(event);

    transaction = evalAndLog("transcation = database.transaction('store', 'readwrite')");
    evalAndLog("store = transcation.objectStore('store')");
    validateAllIndexRecords(originalIndexRecords, finishJSTest);
}
