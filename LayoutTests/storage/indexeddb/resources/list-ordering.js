if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test string list ordering in IndexedDB.");

var expected_order = [
  "",
  "\x00", // 'NULL' (U+0000)
  "0",
  "1",
  "A",
  "B",
  "a",
  "b",
  "\x7F", // 'DELETE' (U+007F)
  "\xC0", // 'LATIN CAPITAL LETTER A WITH GRAVE' (U+00C0)
  "\xC1", // 'LATIN CAPITAL LETTER A WITH ACUTE' (U+00C1)
  "\xE0", // 'LATIN SMALL LETTER A WITH GRAVE' (U+00E0)
  "\xE1", // 'LATIN SMALL LETTER A WITH ACUTE' (U+00E1)
  "\xFF", // 'LATIN SMALL LETTER Y WITH DIAERESIS' (U+00FF)
  "\u0100", // 'LATIN CAPITAL LETTER A WITH MACRON' (U+0100)
  "\u1000", // 'MYANMAR LETTER KA' (U+1000)
  "\uD834\uDD1E", // 'MUSICAL SYMBOL G-CLEF' (U+1D11E), UTF-16 surrogate pairs
  "\uFFFD" // 'REPLACEMENT CHARACTER' (U+FFFD)
];
var i, tmp, permuted_order = expected_order.slice(); permuted_order.reverse();
for (i = 0; i < permuted_order.length - 2; i += 2) {
    tmp = permuted_order[i];
    permuted_order[i] = permuted_order[i + 1];
    permuted_order[i + 1] = tmp;
}

indexedDBTest(prepareDatabase, finishJSTest);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    debug("check that the expected order is the canonical JS sort order:");
    evalAndLog("sorted_order = expected_order.slice(); sorted_order.sort()");
    shouldBeTrue("areArraysEqual(sorted_order, expected_order)");

    debug("");
    debug("Object stores:");
    permuted_order.forEach(function (name) {
        evalAndLog("db.createObjectStore(" + JSON.stringify(name) +")");
    });

    shouldBeTrue("areArraysEqual(db.objectStoreNames, expected_order)");

    debug("");
    debug("Indexes:");
    store = evalAndLog("store = db.createObjectStore('store')");
    permuted_order.forEach(function (name) {
        evalAndLog("store.createIndex(" + JSON.stringify(name) +", 'keyPath')");
    });

    shouldBeTrue("areArraysEqual(store.indexNames, expected_order)");
}
