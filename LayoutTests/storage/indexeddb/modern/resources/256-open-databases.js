description("This test makes sure that if you open 128 unique databases, close your connections to them, and then open 128 other unique databases, that it works.");

var databaseConnections = new Array;

for (var i = 0; i < 128; ++i) {
    var request = window.indexedDB.open("database" + i);
    request.onerror = function() {
        debug("Unexpected error opening database " + i);
        finishJSTest();
    }
    request.onsuccess = function(event) {
        databaseConnections.push(event.target.result);
        if (databaseConnections.length == 128)
            continueTest();
    }
}

function continueTest()
{
    debug("Opened the first 128 databases. Now closing them...");
    for (var i = 0; i < 128; ++i)
        databaseConnections[i].close();

    debug("Now opening the second 128 databases");
    for (var i = 128; i < 256; ++i) {
        var request = window.indexedDB.open("database" + i);
        request.onerror = function() {
            debug("Unexpected error opening database " + i);
            finishJSTest();
        }
        request.onsuccess = function(event) {
            databaseConnections.push(event.target.result);
            if (databaseConnections.length == 256) {
                debug("Successfully opened 256 databases (after closing the first 128)");
                finishJSTest();
            }
        }
    }
}
