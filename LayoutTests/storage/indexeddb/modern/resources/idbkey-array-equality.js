description("This test makes sure that array IDBKeys are correctly compared for equality during object store additions.");

indexedDBTest(prepareDatabase);

var iterationCount = 500;
var successCount = 0;
function doAdd(objectStore, value)
{
    var key = [ 0, value ];
    var request = objectStore.add("value", key);
    request.onsuccess = function() {
        if (++successCount == iterationCount) {
            debug("Successfully added all 500 array keys, without any conflicts.");
            finishJSTest();
        }
    };

    request.onerror = function(event) {
        debug("Error putting value into database (" + value + "): " + event.type);
        finishJSTest();
    }    
}

function prepareDatabase(event)
{    
    var transaction = event.target.transaction;
    var database = event.target.result;

    var objectStore = database.createObjectStore("TestObjectStore");
    
    for (var i = 0; i < iterationCount; ++i)
        doAdd(objectStore, i);
}
