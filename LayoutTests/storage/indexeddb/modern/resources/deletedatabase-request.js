description("This test calls deleteDatabase on window.indexedDB and verifies that an IDBOpenDBRequest object is returned.");


function done()
{
    finishJSTest();
}

var request = window.indexedDB.deleteDatabase("TestDatabase");
debug(request);

done();
