description("This test makes sure that script gets an error if too many databases are opened.");

var databaseConnections = new Array;
var index = 0;

function openDatabase(index)
{
    var request = window.indexedDB.open("database" + index);
    request.onerror = function() {
        debug("Error opening database");
        finishJSTest();
    }
    request.onsuccess = function(event) {
        databaseConnections.push(event.target.result);
        if (databaseConnections.length == 100000) {
            debug("Successfully opened 100000 databases - That should *not* have happened.");
            finishJSTest();
        } else
            openDatabase(++index);
    } 
}

openDatabase(index);
