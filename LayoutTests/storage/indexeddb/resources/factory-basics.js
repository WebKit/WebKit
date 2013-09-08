if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the basics of IndexedDB's IDBFactory.");

function test()
{
    removeVendorPrefixes();

    shouldBeEqualToString("typeof indexedDB.open", "function");
    shouldBeEqualToString("typeof indexedDB.cmp", "function");
    shouldBeEqualToString("typeof indexedDB.deleteDatabase", "function");

    // Non-standard, must be prefixed
    shouldBeEqualToString("typeof indexedDB.webkitGetDatabaseNames", "function");
    shouldBeEqualToString("typeof indexedDB.getDatabaseNames", "undefined");


    name = 'storage/indexeddb/factory-basics';

    request = evalAndLog("indexedDB.webkitGetDatabaseNames()");
    request.onsuccess = getDatabaseNamesSuccess1;
    request.onerror = unexpectedErrorCallback;
}

function getDatabaseNamesSuccess1(evt)
{
    event = evt;
    var databaseNames;
    evalAndLog("databaseNames = event.target.result");
    shouldBeFalse("databaseNames.contains('" + name + "')");
    shouldBeFalse("databaseNames.contains('DATABASE THAT DOES NOT EXIST')");

    request = evalAndLog("indexedDB.open(name)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess(evt)
{
    event = evt;
    evalAndLog("event.target.result.close()");
    request = evalAndLog("indexedDB.webkitGetDatabaseNames()");
    request.onsuccess = getDatabaseNamesSuccess2;
    request.onerror = unexpectedErrorCallback;
}

function getDatabaseNamesSuccess2(evt)
{
    event = evt;
    var databaseNames;
    evalAndLog("databaseNames = event.target.result");
    shouldBeTrue("databaseNames.contains('" + name + "')");
    shouldBeFalse("databaseNames.contains('DATABASE THAT DOES NOT EXIST')");

    request = evalAndLog("indexedDB.deleteDatabase('" + name + "')");
    request.onsuccess = deleteDatabaseSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteDatabaseSuccess()
{
    request = evalAndLog("indexedDB.webkitGetDatabaseNames()");
    request.onsuccess = getDatabaseNamesSuccess3;
    request.onerror = unexpectedErrorCallback;
}

function getDatabaseNamesSuccess3(evt)
{
    event = evt;
    var databaseNames;
    evalAndLog("databaseNames = event.target.result");
    shouldBeFalse("databaseNames.contains('" + name + "')");
    shouldBeFalse("databaseNames.contains('DATABASE THAT DOES NOT EXIST')");

    request = evalAndLog("indexedDB.deleteDatabase('DATABASE THAT DOES NOT EXIST')");
    request.onsuccess = deleteDatabaseSuccess2;
    request.onerror = unexpectedErrorCallback;
}

function deleteDatabaseSuccess2()
{
    finishJSTest();
}

test();
