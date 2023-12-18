//@ skip if $memoryLimited

var exception;
try {
    let a = 'a'.repeat(2147483647);
    const segmenter = new Intl.Segmenter('en');
    const iter = segmenter.segment(a);
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";

