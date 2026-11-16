// Regression test: when ToString(this) is observable and recompiles the regexp,
// String.prototype.matchAll's fast path must use the post-toString RegExp (and
// its flags), not the captured-before-toString one.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function arrFromIter(iter) {
    return [...iter].map(m => m[0]);
}

// Recompile to a non-global pattern inside toString; the iterator must observe
// global=false and the new pattern.
{
    const re = /a/g;
    const receiver = { toString() { re.compile("b", ""); return "abab"; } };
    const result = arrFromIter(String.prototype.matchAll.call(receiver, re));
    shouldBe(result.length, 1);
    shouldBe(result[0], "b");
}

// Recompile to a global pattern with a different body; the iterator must use
// the new pattern and continue to be global.
{
    const re = /a/g;
    const receiver = { toString() { re.compile("b", "g"); return "abab"; } };
    const result = arrFromIter(String.prototype.matchAll.call(receiver, re));
    shouldBe(result.length, 2);
    shouldBe(result[0], "b");
    shouldBe(result[1], "b");
}

// Recompile to add the unicode flag; the iterator's fullUnicode bit must
// reflect the post-toString state.
{
    const re = /😀/g;
    const receiver = { toString() { re.compile("\\ud83d\\ude00", "gu"); return "😀😀"; } };
    const result = arrFromIter(String.prototype.matchAll.call(receiver, re));
    shouldBe(result.length, 2);
    shouldBe(result[0], "😀");
    shouldBe(result[1], "😀");
}
