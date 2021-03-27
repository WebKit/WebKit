//@ runDefault
//@ runNoJIT

function shouldMatch(regexp, str) {
    let result = regexp.test(str);
    if (result !== true)
        throw new Error("Expected " + regexp + ".test(\"" + str + "\") to be true, but wasn't");
}

function shouldntMatch(regexp, str) {
    let result = regexp.test(str);
    if (result !== false)
        throw new Error("Expected " + regexp + ".test(\"" + str + "\") to be false, but wasn't");
}

let s = String.fromCodePoint(0x10000);

shouldMatch(/./, s);
shouldMatch(/./u, s);
shouldMatch(/../, s);
shouldntMatch(/../u, s);
shouldntMatch(/.../, s);
shouldntMatch(/.../u, s);

shouldMatch(/./s, s);
shouldMatch(/./su, s);
shouldMatch(/../s, s);
shouldntMatch(/../su, s);
shouldntMatch(/.../s, s);
shouldntMatch(/.../su, s);
