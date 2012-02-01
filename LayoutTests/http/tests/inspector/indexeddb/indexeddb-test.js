var initialize_IndexedDBTest = function() {

var snifferInstalled = false;
var lastCallbackId = 0;
var callbacks = {};
var callbackIdPrefix = "InspectorTest.IndexedDB_callback";
InspectorTest.evaluateWithCallback = function(frameId, methodName, parameters, callback)
{
    if (!snifferInstalled)
        InspectorTest._installIndexedDBSniffer();
    var callbackId = ++lastCallbackId;
    callbacks[callbackId] = callback;
    var parametersString = "\"" + callbackIdPrefix + callbackId + "\"";
    for (var i = 0; i < parameters.length; ++i) {
        if (typeof(parameters[i]) === "string")
            parametersString += ", \"" + parameters[i] + "\"";
        else if ((typeof(parameters[i]) === "number") || (typeof(parameters[i]) === "boolean"))
            parametersString += ", " + parameters[i];
    }

    var requestString = methodName + "(" + parametersString + ")";
    InspectorTest.evaluateInPage(requestString);
};

InspectorTest._installIndexedDBSniffer = function()
{
    snifferInstalled = true;
    InspectorTest.addConsoleSniffer(consoleMessageOverride, true);

    function consoleMessageOverride(msg)
    {
        var text = msg._messageText;
        if (text.indexOf(callbackIdPrefix) !== 0)
            return;
        var callbackId = text.substring(callbackIdPrefix.length);
        callbacks[callbackId].call();
        delete callbacks[callbackId];
    }
};

InspectorTest.createDatabase = function(frameId, databaseName, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "createDatabase", [databaseName], callback)
};

InspectorTest.deleteDatabase = function(frameId, databaseName, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "deleteDatabase", [databaseName], callback)
};

InspectorTest.createObjectStore = function(frameId, databaseName, objectStoreName, keyPath, autoIncrement, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "createObjectStore", [databaseName, objectStoreName, keyPath, autoIncrement], callback)
};

InspectorTest.deleteObjectStore = function(frameId, databaseName, objectStoreName, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "deleteObjectStore", [databaseName, objectStoreName], callback)
};

InspectorTest.createObjectStoreIndex = function(frameId, databaseName, objectStoreName, objectStoreIndexName, keyPath, unique, multiEntry, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "createObjectStoreIndex", [databaseName, objectStoreName, objectStoreIndexName, keyPath, unique, multiEntry], callback)
};

InspectorTest.deleteObjectStoreIndex = function(frameId, databaseName, objectStoreName, objectStoreIndexName, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "deleteObjectStoreIndex", [databaseName, objectStoreName, objectStoreIndexName], callback)
};

};

var indexedDB = window.indexeddb || window.webkitIndexedDB;

function dispatchCallback(callbackId)
{
    console.log(callbackId);
}

function onIndexedDBError(e)
{
    console.log(e);
}

function doWithDatabase(databaseName, callback)
{
    function innerCallback()
    {
        var db = request.result;
        callback(db);
    }

    var request = indexedDB.open(databaseName);
    request.onblocked = onIndexedDBError;
    request.onerror = onIndexedDBError;
    request.onsuccess = innerCallback;
}

function doWithVersionTransaction(databaseName, callback, commitCallback)
{
    doWithDatabase(databaseName, step2);

    function step2(db)
    {
        var request = db.setVersion(Number(db.version) + 1);
        request.onblocked = onIndexedDBError;
        request.onerror = onIndexedDBError;
        request.onsuccess = step3;

        function step3()
        {
            callback(db, request.result);
            db.close();
            commitCallback();
        }
    }
}

function createDatabase(callbackId, databaseName)
{
    var request = indexedDB.open(databaseName, 0);
    request.onerror = onIndexedDBError;
    request.onsuccess = closeDatabase;

    function closeDatabase()
    {
        request.result.close();
        dispatchCallback(callbackId);
    }
}

function deleteDatabase(callbackId, databaseName)
{
    var request = indexedDB.deleteDatabase(databaseName);
    request.onerror = onIndexedDBError;
    request.onsuccess = dispatchCallback.bind(this, callbackId);
}

function createObjectStore(callbackId, databaseName, objectStoreName, keyPath, autoIncrement)
{
    doWithVersionTransaction(databaseName, withTransactionCallback, dispatchCallback.bind(this, callbackId));

    function withTransactionCallback(db, transaction)
    {
        var store = db.createObjectStore(objectStoreName, { keyPath: keyPath, autoIncrement: autoIncrement });
    }
}

function deleteObjectStore(callbackId, databaseName, objectStoreName)
{
    doWithVersionTransaction(databaseName, withTransactionCallback, dispatchCallback.bind(this, callbackId));

    function withTransactionCallback(db, transaction)
    {
        var store = db.deleteObjectStore(objectStoreName);
    }
}

function createObjectStoreIndex(callbackId, databaseName, objectStoreName, objectStoreIndexName, keyPath, unique, multiEntry)
{
    doWithVersionTransaction(databaseName, withTransactionCallback, dispatchCallback.bind(this, callbackId));

    function withTransactionCallback(db, transaction)
    {
        var objectStore = transaction.objectStore(objectStoreName);
        objectStore.createIndex(objectStoreIndexName, keyPath, { unique: unique, multiEntry: multiEntry });
    }
}

function deleteObjectStoreIndex(callbackId, databaseName, objectStoreName, objectStoreIndexName)
{
    doWithVersionTransaction(databaseName, withTransactionCallback, dispatchCallback.bind(this, callbackId));

    function withTransactionCallback(db, transaction)
    {
        var objectStore = transaction.objectStore(objectStoreName);
        objectStore.deleteIndex(objectStoreIndexName);
    }
}

