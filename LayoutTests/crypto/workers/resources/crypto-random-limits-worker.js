importScripts('../../../resources/js-test-pre.js');

description("Tests the limits of crypto.randomValues.");

shouldBe("'crypto' in self", "true");
shouldBe("'getRandomValues' in self.crypto", "true");

try {
    var almostTooLargeArray = new Uint8Array(65536);
    var tooLargeArray = new Uint8Array(65537);

    shouldNotThrow("crypto.getRandomValues(almostTooLargeArray)");
    shouldThrow("crypto.getRandomValues(tooLargeArray)");
} catch(ex) {
    debug(ex);
}

finishJSTest();
