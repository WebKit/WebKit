var throwOnToStringObject = { };
throwOnToStringObject.toString = function () { throw "Cannot call toString on this object." };

var throwOnGetLengthObject = { };
throwOnGetLengthObject.__defineGetter__("length", function () { throw "Cannot get length of this object."; });

var throwOnGetZeroObject = { length: 1 };
throwOnGetZeroObject.__defineGetter__("0", function () { throw "Cannot get 0 property of this object."; });

var expectNoException = [
    '""',
    '"", null',
    '"", undefined',
    '"", []',
    '"", [ "arg0" ]',
    '"", { }',
    '"", { length: 0 }',
    '"", { length: 1, 0: "arg0" }',
];

var expectException = [
    '',
    'null',
    'undefined',
    '0',
    'throwOnToStringObject',
    '"", throwOnGetLengthObject',
    '"", throwOnGetZeroObject',
    '"", [ throwOnToStringObject ]',
    '"", 0',
    '"", ""',
];

function tryExecuteSql(transaction, parameterList)
{
    try {
        eval('transaction.executeSql(' + parameterList + ')');
        return null;
    } catch (exception) {
        return exception;
    }
}

function runTransactionTest(transaction, parameterList, expectException)
{
    var exception = tryExecuteSql(transaction, parameterList);
    if (expectException) {
        if (exception)
            postMessage("PASS: executeSql(" + parameterList + ") threw an exception as expected.");
        else
            postMessage("FAIL: executeSql(" + parameterList + ") did not throw an exception.");
    } else {
        if (exception)
            postMessage("FAIL: executeSql(" + parameterList + ") threw an exception: " + exception);
        else
            postMessage("PASS: executeSql(" + parameterList + ") did not throw an exception.");
    }
}

function runTransactionTests(transaction)
{
    for (i in expectNoException)
        runTransactionTest(transaction, expectNoException[i], false);
    for (i in expectException)
        runTransactionTest(transaction, expectException[i], true);
}

var db = openDatabaseSync("ExecuteSQLArgsTest", "1.0", "Test of handling of the arguments to SQLTransactionSync.executeSql", 1);
db.transaction(runTransactionTests);
postMessage("done");
