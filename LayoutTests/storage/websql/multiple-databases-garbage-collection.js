function GC()
{
    // Force GC.
    if (window.GCController)
        GCController.collect();
    else {
        for (var i = 0; i < 10000; ++i) {
            ({ });
        }
    }
}

// Variable for the database that will never be forgotten
var persistentDB = 0;
// Variable for the forgotten database
var forgottenDB = 0;

var completed = 0;
function checkCompletion()
{
    if (++completed == 2 && window.testRunner)
        testRunner.notifyDone();
}

function runTest()
{
    persistentDB = openDatabaseWithSuffix("MultipleDatabasesTest1", "1.0", "Test one out of a set of databases being destroyed (1)", 32768);
    forgottenDB = openDatabaseWithSuffix("MultipleDatabasesTest2", "1.0", "Test one out of a set of databases being destroyed (2)", 32768);

    forgottenDB.transaction(function(tx) {
        tx.executeSql("CREATE TABLE IF NOT EXISTS EmptyTable (unimportantData)", []);
    }, function(err) {
        log("Forgotten Database Transaction Errored - " + err);
        forgottenDB = 0;
        GC();
        checkCompletion();
    }, function() {
        log("Forgotten Database Transaction Complete");
        forgottenDB = 0;
        GC();
        checkCompletion();
    });

    persistentDB.transaction(function(tx) {
        tx.executeSql("CREATE TABLE IF NOT EXISTS DataTest (randomData)", [], function(tx, result) { 
            for (var i = 0; i < 25; ++i)
                tx.executeSql("INSERT INTO DataTest (randomData) VALUES (1)", []);
        }); 
    }, function(err) {
        log("Persistent Database Transaction Errored - " + err);
        checkCompletion();
    }, function() {
        log("Persistent Database Transaction Complete");
        checkCompletion();
    });
}
