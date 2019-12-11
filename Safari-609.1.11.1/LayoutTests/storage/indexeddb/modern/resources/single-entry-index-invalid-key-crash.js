description("Tests that adding to an object store, with a single-entry Index, where the index key is an array that is not entirely valid... does not crash.");

indexedDBTest(prepareDatabase);

function log(message)
{
    debug(message);
}

function prepareDatabase(event)
{
    db = event.target.result;
    os = db.createObjectStore("friends", { keyPath: "id", autoIncrement: true });
	idx = os.createIndex("[age+shoeSize]", ["age", "shoeSize"]);
	os.add({ name: "Mark", age: 29, shoeSize: null });

	idx.openCursor().onsuccess = function(event) {
	    if (event.target.result)
	        log("Index unexpectedly has an entry");
        else
	        log("Index has no entries");
    };
	
    event.target.transaction.oncomplete = function() {
        finishJSTest();
    };
}

 