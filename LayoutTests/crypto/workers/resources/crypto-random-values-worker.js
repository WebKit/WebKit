importScripts('../../../resources/js-test-pre.js');

description("Tests crypto.randomValues.");

shouldBe("'crypto' in self", "true");
shouldBe("'getRandomValues' in self.crypto", "true");

try {
    // FIXME: This test is flaky.  If we ran this test every second since the
    //        beginning of the universe, on average, it would have failed
    //        2^{-748} times.

    var reference = new Uint8Array(100);
    var sample = new Uint8Array(100);

    crypto.getRandomValues(reference);
    crypto.getRandomValues(sample);

    var matchingBytes = 0;

    for (var i = 0; i < reference.length; i++) {
        if (reference[i] == sample[i])
            matchingBytes++;
    }

    shouldBe("matchingBytes < 100", "true");
} catch(ex) {
    debug(ex);
}

finishJSTest();
