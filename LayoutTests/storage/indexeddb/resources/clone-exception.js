if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure DataCloneError is consistently thrown by IndexedDB methods");

var NON_CLONEABLE = self;
var INVALID_KEY = {};

setDBNameFromPath();
doFirstOpen();

function doFirstOpen()
{
    preamble();
    request = evalAndLog("indexedDB.open(dbname + '1')");
    request.onupgradeneeded = function onUpgradeNeeded(e) {
        preamble();
        db = e.target.result;
        evalAndExpectException("db.createObjectStore('store').put(NON_CLONEABLE, 0);", "25", "'DataCloneError'");
        doSecondOpen();
    };
}

function doSecondOpen()
{
    preamble();
    request = evalAndLog("indexedDB.open(dbname + '2')");
    request.onupgradeneeded = function onUpgradeNeeded(e) {
        preamble();
        db = e.target.result;
        evalAndExpectException("db.createObjectStore('store').put(NON_CLONEABLE, 0);", "25", "'DataCloneError'");
        doThirdOpen();
    };
}

function doThirdOpen()
{
    preamble();
    request = evalAndLog("indexedDB.open(dbname + '3')");
    request.onupgradeneeded = function onUpgradeNeeded(e) {
        preamble();
        db = e.target.result;
        evalAndExpectException("db.createObjectStore('store').put(NON_CLONEABLE, INVALID_KEY);", "25", "'DataCloneError'");
        finishJSTest();
    };
}
