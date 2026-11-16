if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("This tests verifies put operation can be performed on renamed object store");

indexedDBTest(prepareDatabase, openSuccess);

var db;
var dbName;
var dbVersion;
const objectStoreName = "ObjectStore";
const newOjectStoreName = "RenamedObjectStore";

function prepareDatabase(event)
{
    debug("Open database upgradeneeded: database old version - " + event.oldVersion + ", new version - " + event.newVersion);

    db = event.target.result;
	dbName = db.name;
	dbVersion = db.version;

	objectStore = db.createObjectStore(objectStoreName);
	debug("Current objectStore name: " + objectStore.name);

	try {
        objectStore.name = newOjectStoreName;
        debug("Current objectStore name: " + objectStore.name);
	} catch(e) {
        debug("Caught exception when renaming object store: " + e);
	}
}

function openSuccess(event)
{
	debug("Open database success");
	transaction = db.transaction(newOjectStoreName, "readwrite");
	objectStore = transaction.objectStore(newOjectStoreName);
	request = objectStore.put('value', 'key');
	request.onsuccess = () => {
        debug("Put success in renamed object store");
        finishJSTest();
	}
	request.onerror = unexpectedErrorCallback;
}