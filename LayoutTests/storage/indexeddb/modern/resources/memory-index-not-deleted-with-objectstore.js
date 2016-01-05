description("Makes sure that when an in-memory object store is deleted, any in-memory indexes it has are also deleted. If the test does not crash, it passes.");

indexedDBTest(prepareDatabase, versionChangeDone);

function prepareDatabase()
{
    evalAndLog("connection = event.target.result;");

    for (var i = 0; i < 50; ++i) {
        evalAndLog("store = connection.createObjectStore('name');");
        evalAndLog("index = store.createIndex('name', 'foo');");
        deleteAllObjectStores(connection);
    }
}

function versionChangeDone()
{
    finishJSTest();
}
