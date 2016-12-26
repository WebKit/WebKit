function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(callerSourceOrigin().endsWith('source-origin.js'), true);
shouldBe([ 0 ].map(callerSourceOrigin)[0].endsWith('source-origin.js'), true);
shouldBe(eval(`callerSourceOrigin()`).endsWith('source-origin.js'), true);
shouldBe((0, eval)(`callerSourceOrigin()`).endsWith('source-origin.js'), true);
shouldBe((new Function(`return callerSourceOrigin()`))().endsWith('source-origin.js'), true);
shouldBe((Function(`return callerSourceOrigin()`))().endsWith('source-origin.js'), true);
