const testStrings = [
    "a", "Z", "5", "_",  // word chars
    "!", " ", "@", "#",  // non-word chars
    " ", "\t", "\n",     // spaces
    "x", "Y", "0"        // non-spaces
];

function benchWordChar() {
    const reg = /\w/;
    for (let i = 0; i < 1e5; i++) {
        for (let j = 0; j < testStrings.length; j++) {
            reg.test(testStrings[j]);
        }
    }
}
noInline(benchWordChar);

function benchNonWordChar() {
    const reg = /\W/;
    for (let i = 0; i < 1e5; i++) {
        for (let j = 0; j < testStrings.length; j++) {
            reg.test(testStrings[j]);
        }
    }
}
noInline(benchNonWordChar);

function benchSpaces() {
    const reg = /\s/;
    for (let i = 0; i < 1e5; i++) {
        for (let j = 0; j < testStrings.length; j++) {
            reg.test(testStrings[j]);
        }
    }
}
noInline(benchSpaces);

function benchNonSpaces() {
    const reg = /\S/;
    for (let i = 0; i < 1e5; i++) {
        for (let j = 0; j < testStrings.length; j++) {
            reg.test(testStrings[j]);
        }
    }
}
noInline(benchNonSpaces);

benchWordChar();
benchNonWordChar();
benchSpaces();
benchNonSpaces();
