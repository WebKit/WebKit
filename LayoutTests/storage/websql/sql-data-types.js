//description("This test verifies that the javascript values returned by database queries are of same type as the values put into the database.");

function writeMessageToLog(message)
{
    document.getElementById("console").innerText += message + "\n";
}

function notifyDone(str) {
    writeMessageToLog(str);
    if (window.testRunner)
        testRunner.notifyDone();
}

var testValues = {
    timestamp: new Date("Wed Feb 06 2008 12:16:52 GMT+0200 (EET)").valueOf(),
    id: 1001,
    real: 101.444,
    text: "WebKit db TEXT",
    blob: "supercalifragilistic"
};

function shouldBeSameTypeAndValue(propName, testValue, result) {
    if (testValue == result && typeof testValue == typeof result) {
        writeMessageToLog("PASS: property '" + propName + "' ok, type was " + typeof result);
        return true;
    }
    writeMessageToLog("FAIL: property '" + propName + "' failed."
        + " expected: " + typeof testValue + ":'" + testValue + "' "
        + " got: " + typeof result + ":'" + result +"'");
    return false;
}

function testDBValues(tx, result) {
    var rs = result.rows.item(0);
    // Avoid for .. in because (theretically) the order can change
    i = "timestamp"; shouldBeSameTypeAndValue(i, testValues[i], rs[i]);
    i = "id"; shouldBeSameTypeAndValue(i, testValues[i], rs[i]);
    i = "real"; shouldBeSameTypeAndValue(i, testValues[i], rs[i]);
    i = "text"; shouldBeSameTypeAndValue(i, testValues[i], rs[i]);
    i = "blob"; shouldBeSameTypeAndValue(i, testValues[i], rs[i]);
    
    tx.executeSql("DROP TABLE DataTypeTestTable", [],
        function(tx, result) {
            notifyDone("PASS: database clean up ok.");
        },
        function(tx, result) {
            notifyDone("FAIL: Database clean up failed.");
        });
}

function fetchDBValuesStmt(tx, result) {
    tx.executeSql("SELECT * FROM DataTypeTestTable", [],
        testDBValues,
        function(tx,error) {
            notifyDone("FAIL: Error fetching values from the db.")
        });
}

function insertTestValuesStmt(tx, result) {
    tx.executeSql("INSERT INTO DataTypeTestTable (id, real, timestamp, text, blob) VALUES (?,?,?,?,?)",
        [testValues.id, testValues.real, testValues.timestamp, testValues.text, testValues.blob],
        fetchDBValuesStmt,
        function(tx, error) {
            notifyDone("FAIL: Error inserting values to the db.");
        });
}

function createTestDBStmt(tx)
{
    tx.executeSql("CREATE TABLE IF NOT EXISTS DataTypeTestTable (id INTEGER UNIQUE, real REAL, timestamp INTEGER, text TEXT, blob BLOB)", [],
        insertTestValuesStmt,
        function(tx, error) {
            notifyDone("FAIL: Error creating the db.");
        });
}

function runTest() {
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }
    var db = openDatabase("DataTypeTest", "1.0", "Database for sql data type test", 1);
    if (db)
        db.transaction(createTestDBStmt);
    else
        notifyDone("FAIL: Error opening the db");
}
