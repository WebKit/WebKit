//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// IRO bounds StringCharCodeAt to [0, 0xFFFF] and StringCodePointAt to
// [0, 0x10FFFF]. Pins the proven ranges so the rules silently not firing fails
// here (the correctness-only b4cca18 test wouldn't).

load("./resources/iro-test-helpers.js", "caller relative");

function charCode(str, i) {
    return $vm.probe("c", str.charCodeAt(i));
}
noInline(charCode);

function codePoint(str, i) {
    return $vm.probe("cp", str.codePointAt(i));
}
noInline(codePoint);

const str = "Hello, World!";
for (let i = 0; i < 300000; ++i) {
    charCode(str, i & 7);
    codePoint(str, i & 7);
}

{
    const iro = makeIROHelper(charCode);
    const r = iro.range("c");
    if (!r || r.min !== 0 || r.max !== 0xFFFF)
        throw new Error('charCodeAt: expected IRO range [0,65535], got '
            + JSON.stringify(r) + ' — the StringCharCodeAt range rule did not fire');
}

{
    const iro = makeIROHelper(codePoint);
    const r = iro.range("cp");
    if (!r || r.min !== 0 || r.max !== 0x10FFFF)
        throw new Error('codePointAt: expected IRO range [0,1114111], got '
            + JSON.stringify(r) + ' — the StringCodePointAt range rule did not fire');
}

print("PASS");
