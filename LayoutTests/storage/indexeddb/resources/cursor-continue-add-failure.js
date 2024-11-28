if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Adding record fails during cursor iteration.");

function prepareDatabase()
{
    database = event.target.result;
    transaction = event.target.transaction;
    // Transaction will be aborted due to failed request.
    transaction.onabort = finishJSTest;

    objectStore = database.createObjectStore('records', {autoIncrement: true});
    objectStore.add({userid:10, score:100}).onerror = unexpectedErrorCallback;
    objectStore.add({userid:20, score:200}).onerror = unexpectedErrorCallback;
    objectStore.add({userid:30, score:200}).onerror = unexpectedErrorCallback;

    // Different users can have same score.
    scoreIndex = objectStore.createIndex('index1', 'score', {unique: false});
    // One user can only have one record.
    userIndex = objectStore.createIndex('index2', 'userid', {unique: true});

    expectedScores = [100, 200];
    evalAndLog("cursorRequest = scoreIndex.openCursor(null, 'nextunique')");
    cursorRequest.onsuccess = (event) => {
        evalAndLog("cursor = event.target.result");
        if (cursor) {
            shouldBe("cursor.key", expectedScores.shift().toString());
            if (cursor.key == 200) {
                // This insertion should fail because user cannot have two records.
                addRequest = objectStore.add({userid:30, score:300});
                addRequest.onsuccess = unexpectedSuccessCallback;
            }
            evalAndLog("cursor.continue()");
        }
    }
}

indexedDBTest(prepareDatabase);
