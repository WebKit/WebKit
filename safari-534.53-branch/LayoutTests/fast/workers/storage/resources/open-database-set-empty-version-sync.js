var db = openDatabaseSync("OpenDatabaseSetEmptyVersionTest", "", "Test that the DB version is set to the empty string.", 1);

try {
    var db2 = openDatabaseSync("OpenDatabaseSetEmptyVersionTest", "test", "Test that the DB version is set to the empty string.", 1);
    postMessage("FAIL: An exception should've been thrown.");
} catch (err) {
    postMessage("PASS");
}
postMessage("done");
