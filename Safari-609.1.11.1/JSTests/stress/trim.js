function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function startTest(string, expected) {
    shouldBe(string.trimStart(), expected);
    shouldBe(string.trimLeft(), expected);
}

function endTest(string, expected) {
    shouldBe(string.trimEnd(), expected);
    shouldBe(string.trimRight(), expected);
}

function trimTest(string, expected) {
    shouldBe(string.trim(), expected);
}

startTest(`    Hello   `, `Hello   `);
endTest(`    Hello   `, `    Hello`);
trimTest(`    Hello   `, `Hello`);

startTest(`    日本語   `, `日本語   `);
endTest(`    日本語   `, `    日本語`);
trimTest(`    日本語   `, `日本語`);

startTest(`Hello`, `Hello`);
endTest(`Hello`, `Hello`);
trimTest(`Hello`, `Hello`);

startTest(`日本語`, `日本語`);
endTest(`日本語`, `日本語`);
trimTest(`日本語`, `日本語`);

startTest(``, ``);
endTest(``, ``);
trimTest(``, ``);

startTest(`    `, ``);
endTest(`    `, ``);
trimTest(`    `, ``);

startTest(`    A`, `A`);
endTest(`    A`, `    A`);
trimTest(`    A`, `A`);

startTest(`A    `, `A    `);
endTest(`A    `, `A`);
trimTest(`A    `, `A`);

shouldBe(String.prototype.trimStart, String.prototype.trimLeft);
shouldBe(String.prototype.trimEnd, String.prototype.trimRight);
