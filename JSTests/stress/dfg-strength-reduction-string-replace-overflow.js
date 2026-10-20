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
    let a = "a";
    a = a + a + "a"; a = a + a + "a"; a = a + a + "a"; a = a + a + "a"; a = a + a + "a"; a = a + a + "a"; a = a + a + "a";
    a = a + a + "a"; a = a + a + "a";
    let b = "bb";
    b += b; b += b; b += b; b += b; b += b; b += b; b += b; b += b; b += b; b += b;
    b += b; b += b; b += b; b += b; b += b; b += b; b += b; b += b; b += b; b += b;
    return (a + b).replace(/a/g, b).length;
}
noInline(test);

for (let i = 0; i < testLoopCount && !numberOfDFGCompiles(test); ++i)
    shouldThrowRangeError(test);
