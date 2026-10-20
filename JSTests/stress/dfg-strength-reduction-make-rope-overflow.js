//@ memoryHog!
//@ skip if $addressBits <= 32
//@ runDefault("--useConcurrentJIT=0")

function shouldThrowRangeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof RangeError))
        throw new Error("bad error: " + error);
}

function test() {
    let s = "ab";
    s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s;
    s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s;
    s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s;
    return s.length;
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    shouldThrowRangeError(test);
