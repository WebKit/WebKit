// Opens the database used in this test case
function openTestDatabase()
{
    return openDatabaseWithSuffix("OpenDatabaseWhileTransactionInProgressTest",
                                  "1.0",
                                  "Test to make sure that calling openDatabase() while a transaction is in progress on a different handle to the same database does not result in a deadlock.",
                                  2100000); // 2MB + epsilon
}

// See https://bugs.webkit.org/show_bug.cgi?id=28207
// In order to trigger this bug, the transaction must acquire an exclusive
// lock on the DB file before trying to obtain a second handle to the same DB.
// The only way to force SQLite to obtain an exclusive lock is to change more
// than cache_size * page_size bytes in the database. The default value for
// cache_size is 2000 pages, and the default page_size is 1024 bytes. So the
// size of the blob must be at least 2MB.
function runTest()
{
    var db1 = openTestDatabase();
    db1.transaction(function(tx) {
        // Create the Test table if it does not exist
        tx.executeSql("CREATE TABLE IF NOT EXISTS Test (Foo BLOB);");
        tx.executeSql("INSERT INTO Test VALUES (ZEROBLOB(2097152));", [],
                      function(result) {
                          var db2 = openTestDatabase();
                          log("openDatabase() succeeded.");
                      },
                      function(tx, error) {
                          log("Executing statement failed: " + error.message);
                      });

        // Clean up the DB to allow for repeated runs of this test
        // without needing to increase the default allowed quota (5MB)
        tx.executeSql("DELETE FROM Test;");
    }, function(error) {
        log("Transaction failed: " + error.message);
    }, function() {
        if (window.testRunner)
            testRunner.notifyDone();
    });
}
