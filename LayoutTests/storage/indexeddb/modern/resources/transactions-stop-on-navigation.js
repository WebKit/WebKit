description("Makes sure transactions stop on navigation to a new page.");

indexedDBTest(prepareDatabase, openSuccess);

function prepareDatabase()
{
    evalAndLog("connection = event.target.result;");

    evalAndLog("store = connection.createObjectStore('name');");
    evalAndLog("index = store.createIndex('name', 'foo');");
}

function openSuccess()
{
    evalAndLog("connection.close();");
    evalAndLog("request = window.indexedDB.open('transactions-stop-on-navigation.html');");
    evalAndLog("request.onsuccess = opened;");
}

function opened(event)
{
    evalAndLog("connection = event.target.result;");
    evalAndLog("transaction = connection.transaction('name', 'readwrite');");
    for (var i = 0; i < 100; ++i)
        evalAndLog("transaction.objectStore('name').put('bar', 'foo');");
    
    window.location.href = "resources/transactions-stop-on-navigation-2.html";
}
