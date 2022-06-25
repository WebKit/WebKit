if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("No crashes when opening and reloading with bad version numbers");

function test()
{
  removeVendorPrefixes();
  evalAndLog("dbname = String(window.location)");

  try { indexedDB.open(dbname, -1); } catch (e) { }
  indexedDB.open(dbname, 2);
  try { indexedDB.open(dbname, -1); } catch (e) { }

  setTimeout(function() {
    if (!window.location.search) {
      window.location = window.location + "?2";
    } else {
      finishJSTest();
    }
  }, 500);
}

test();
