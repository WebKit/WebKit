//@ runDefault

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const letters = "abcdefghijklmnopqrstuvwxyz";

function test(termCount, target, hasSecondAlternative, unicode) {
    let body = "";
    let first = "";
    let second = "";
    for (let i = 0; i < termCount; ++i) {
        const letter = letters[i % letters.length];
        body += unicode ? `[${letter}\\u{1F600}]*` : `${letter}*`;
        if (i >= target)
            first += letter;
        second += letter;
    }
    if (hasSecondAlternative)
        body += "|_";
    const letter = letters[target % letters.length];
    const regExp = new RegExp(`^(?:#${body})+${letter}${letter}`, unicode ? "u" : "");
    const padding = unicode ? "\u3042\u3042\u3042" : "!!!";
    shouldBe(String(regExp.exec(`#${letter}${first}#${second}${padding}`)), `#${letter}${letter}`);
    if (hasSecondAlternative)
        shouldBe(String(regExp.exec(`#${letter}${first}_${padding}`)), `#${letter}${letter}`);
}

for (const termCount of [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 33]) {
    for (let target = 0; target < termCount; ++target) {
        test(termCount, target, false, false);
        test(termCount, target, true, false);
        test(termCount, target, false, true);
        test(termCount, target, true, true);
    }
}
