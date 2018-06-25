Object.defineProperty(Array.prototype, '0', {
    get() { },
    set() { throw new Error(); }
});

var __v_7772 = "GGCCGGGTAAAGTGGCTCACGCCTGTAATCCCAGCACTTTACCCCCCGAGGCGGGCGGA";
var exception;

try {
    __v_7772.match(/[cgt]gggtaaa|tttaccc[acg]/ig);
} catch (e) {
    exception = e;
}

if (exception != "Error")
    throw "FAILED";
