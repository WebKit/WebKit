if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB persistence");

function test()
{
    removeVendorPrefixes();
    evalAndLog("dbname = self.location.pathname");

    openAndChangeVersion("1", function (connection) {
        db = connection;
        shouldBeEqualToString("db.version", "1");
        shouldBeEqualToString("db.name", "dbname");
        shouldBe("db.objectStoreNames.length", "0");
        evalAndLog("db.createObjectStore('store1')");
        shouldBe("db.objectStoreNames.length", "1");
    }, second);
}

function second()
{
    openAndChangeVersion("2", function (connection) {
        db = connection;
        shouldBeEqualToString("db.version", "2");
        shouldBeEqualToString("db.name", "dbname");
        shouldBe("db.objectStoreNames.length", "1");
        shouldBeTrue("db.objectStoreNames.contains('store1')");
        evalAndLog("db.createObjectStore('store2')");
        shouldBe("db.objectStoreNames.length", "2");
        shouldBeTrue("db.objectStoreNames.contains('store1')");
        shouldBeTrue("db.objectStoreNames.contains('store2')");
    }, third);
}

function third()
{
    openAndChangeVersion("3", function (connection) {
        db = connection;
        shouldBeEqualToString("db.version", "3");
        shouldBeEqualToString("db.name", "dbname");
        shouldBe("db.objectStoreNames.length", "2");
        shouldBeTrue("db.objectStoreNames.contains('store1')");
        shouldBeTrue("db.objectStoreNames.contains('store2')");
        evalAndLog("db.deleteObjectStore('store1')");
        shouldBe("db.objectStoreNames.length", "1");
        shouldBeFalse("db.objectStoreNames.contains('store1')");
        shouldBeTrue("db.objectStoreNames.contains('store2')");
    }, fourth);
}

function fourth()
{
    openAndChangeVersion("4", function (connection) {
        db = connection;
        shouldBeEqualToString("db.version", "4");
        shouldBeEqualToString("db.name", "dbname");
        shouldBe("db.objectStoreNames.length", "1");
        shouldBeFalse("db.objectStoreNames.contains('store1')");
        shouldBeTrue("db.objectStoreNames.contains('store2')");
        evalAndLog("db.deleteObjectStore('store2')");
        shouldBe("db.objectStoreNames.length", "0");
        shouldBeFalse("db.objectStoreNames.contains('store1')");
        shouldBeFalse("db.objectStoreNames.contains('store2')");
    }, fifth);
}

function fifth()
{
    openAndChangeVersion("5", function (connection) {
        db = connection;
        shouldBeEqualToString("db.version", "5");
        shouldBeEqualToString("db.name", "dbname");
        shouldBe("db.objectStoreNames.length", "0");
        shouldBeFalse("db.objectStoreNames.contains('store1')");
        shouldBeFalse("db.objectStoreNames.contains('store2')");
    }, finishJSTest);
}


function openAndChangeVersion(version, callback, next)
{
    debug("");
    evalAndLog("request = indexedDB.open('dbname')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        evalAndLog("db = request.result");
        shouldBeNonNull("db");
        request = evalAndLog("db.setVersion(" + JSON.stringify(version) + ")");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function () {
            var trans = request.result;
            trans.onabort = unexpectedAbortCallback;
            callback(db);
            trans.oncomplete = function () {
                evalAndLog("db.close()");
                next();
            };
        };
    };
}

test();
