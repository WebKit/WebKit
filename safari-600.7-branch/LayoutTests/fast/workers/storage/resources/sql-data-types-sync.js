var testValues = {
    timestamp: new Date("Wed Feb 06 2008 12:16:52 GMT+0200 (EET)").valueOf(),
    id: 1001,
    real: 101.444,
    text: "WebKit db TEXT",
    blob: "supercalifragilistic"
};

function shouldBeSameTypeAndValue(propName, testValue, result) {
    if (testValue == result && typeof testValue == typeof result)
        postMessage("PASS: property '" + propName + "' OK, type was " + typeof result);
    else
        postMessage("FAIL: property '" + propName + "' failed; " +
                    "expected " + typeof testValue + ":'" + testValue + "', " +
                    "got " + typeof result + ":'" + result +"'");
}

function testDBValues(result) {
    var i = "timestamp"; shouldBeSameTypeAndValue(i, testValues[i], result[i]);
    i = "id"; shouldBeSameTypeAndValue(i, testValues[i], result[i]);
    i = "real"; shouldBeSameTypeAndValue(i, testValues[i], result[i]);
    i = "text"; shouldBeSameTypeAndValue(i, testValues[i], result[i]);
    i = "blob"; shouldBeSameTypeAndValue(i, testValues[i], result[i]);
}

var db = openDatabaseSync("SQLDataTypesTest", "1.0", "Database for SQL data type test", 1);
db.transaction(function(tx) {
    tx.executeSql("CREATE TABLE IF NOT EXISTS DataTypeTestTable (id INTEGER UNIQUE, real REAL, timestamp INTEGER, text TEXT, blob BLOB)");
    tx.executeSql("INSERT INTO DataTypeTestTable (id, real, timestamp, text, blob) VALUES (?,?,?,?,?)",
                  [testValues.id, testValues.real, testValues.timestamp, testValues.text, testValues.blob]);
    var result = tx.executeSql("SELECT * FROM DataTypeTestTable");
    testDBValues(result.rows.item(0));
    tx.executeSql("DROP TABLE DataTypeTestTable");
});

postMessage("done");
