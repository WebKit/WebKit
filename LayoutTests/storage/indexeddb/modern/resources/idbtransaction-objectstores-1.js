description("This tests the IDBTransaction.objectStoreNames API");

indexedDBTest(prepareDatabase);

function done()
{
    finishJSTest();
}

function dumpList(list)
{
	debug("List has " + list.length + " entries");
	
	for (i = 0; i < list.length; ++i)
		debug("Entry " + i + ": " + list.item(i));
}

function prepareDatabase(event)
{
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
    var tx = event.target.transaction;
    db = event.target.result;

	db.createObjectStore("ObjectStore1");
	dumpList(tx.objectStoreNames);

	db.createObjectStore("ObjectStore2");
	dumpList(tx.objectStoreNames);

	db.deleteObjectStore("ObjectStore1");
	dumpList(tx.objectStoreNames);

	db.createObjectStore("ObjectStore3");
	db.createObjectStore("ObjectStore4");
	dumpList(tx.objectStoreNames);
	
    tx.onabort = function(event) {
        debug("Version change transaction unexpected abort");
        done();
    }

    tx.oncomplete = function(event) {
        debug("Version change transaction complete");
        continueTest();
    }

    tx.onerror = function(event) {
        debug("Version change transaction unexpected error");
        done();
    }
}

function continueTest()
{
	tx = db.transaction(["ObjectStore2", "ObjectStore3", "ObjectStore4"]);
	dumpList(tx.objectStoreNames);

	tx = db.transaction(["ObjectStore2", "ObjectStore4"]);
	dumpList(tx.objectStoreNames);
	
	done();
}
