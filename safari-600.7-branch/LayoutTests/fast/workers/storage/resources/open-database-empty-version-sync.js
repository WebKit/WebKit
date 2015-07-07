try {
    var db = openDatabaseSync("OpenDatabaseEmptyVersionTest", "", "Test that we can open databases with an empty version.", 1);
    postMessage("PASS");
} catch (err) {
    postMessage("FAIL: " + err);
}
postMessage("done");
