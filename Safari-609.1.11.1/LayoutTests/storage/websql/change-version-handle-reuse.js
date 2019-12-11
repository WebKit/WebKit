function finishTest()
{
    log("TEST COMPLETE.");

    if (window.testRunner)
        testRunner.notifyDone();
}

function runTest()
{
    try {
        db = openDatabaseWithSuffix("ChangeVersion", "", "Test that changing a database version doesn't kill our handle to it", 1);
        var version = db.version;
        var newVersion = version ? (parseInt(version) + 1).toString() : "1"; 
        db.changeVersion(version, newVersion, function(tx) {
            log("changeVersion: transaction callback");
        }, function(error) {
            log("changeVersion: error callback: " + error.message);
        }, function() {
            log("changeVersion: success callback");
            runTest2();
        });
        
    } catch (e) {
        log("changeVersion exception: " + e);
        finishTest();
    }
}
 
function runTest2()
{
    try {
        db.transaction(function(tx) {
            tx.executeSql("SELECT * from FooBar", [], function(tx) {
                log("transaction: statement callback");
                finishTest();
            }, function(tx, error) {
                log("transaction: statement error callback: " + error.message);
                finishTest();
            });
        });
    } catch (e) {
        log("transaction exception: " + e);
        finishTest();
    }
}
