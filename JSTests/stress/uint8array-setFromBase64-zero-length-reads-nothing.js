// FromBase64 returns before examining any character when maxLength is 0, so a zero length destination
// accepts input that is not valid base64 at all.
// https://tc39.es/proposal-arraybuffer-base64/spec/#sec-frombase64

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function shouldThrow(callback, errorConstructor) {
    try {
        callback();
    } catch (e) {
        shouldBe(e instanceof errorConstructor, true);
        return;
    }
    throw new Error('FAIL: should have thrown');
}

var lastChunkHandlings = ["loose", "strict", "stop-before-partial"];
// "aa==" is deliberately absent: it is valid base64 under loose and stop-before-partial.
var invalidStrings = ["#", "a#", "aa#", "aaa#", "aaaa#", "=", "a=", "aa=a", "===="];

for (var lastChunkHandling of lastChunkHandlings) {
    for (var string of invalidStrings) {
        var empty = new Uint8Array(0);
        var result = empty.setFromBase64(string, { lastChunkHandling });
        shouldBe(result.read, 0);
        shouldBe(result.written, 0);
    }

    // A zero length view onto a larger buffer behaves the same way.
    for (var string of invalidStrings) {
        var view = new Uint8Array(new ArrayBuffer(8), 4, 0);
        var result = view.setFromBase64(string, { lastChunkHandling });
        shouldBe(result.read, 0);
        shouldBe(result.written, 0);
    }

    // Uint8Array.fromBase64 has no maxLength, so it still rejects the same input.
    for (var string of invalidStrings) {
        shouldThrow(() => {
            Uint8Array.fromBase64(string, { lastChunkHandling });
        }, SyntaxError);
    }
}
