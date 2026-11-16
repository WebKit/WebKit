function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

function freshCopy(s) {
    return (s + "!").slice(0, s.length);
}

// Long 8-bit strings (length >= 8) so the word-at-a-time path runs.
(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    function includes(array, value)
    {
        return array.includes(value);
    }
    noInline(includes);

    const N = 50;
    const array = [];
    for (let i = 0; i < N; ++i)
        array.push(freshCopy(String.fromCharCode(65 + i) + "x".repeat(31)));

    for (let i = 0; i < testLoopCount; ++i) {
        const targetIdx = i % N;
        const target = freshCopy(String.fromCharCode(65 + targetIdx) + "x".repeat(31));
        shouldBe(indexOf(array, target), targetIdx);
        shouldBe(includes(array, target), true);
        const missing = freshCopy("@" + "x".repeat(31));
        shouldBe(indexOf(array, missing), -1);
        shouldBe(includes(array, missing), false);
        const tailMismatch = freshCopy(String.fromCharCode(65 + targetIdx) + "x".repeat(30) + "y");
        shouldBe(indexOf(array, tailMismatch), -1);
        shouldBe(includes(array, tailMismatch), false);
    }
}());

// Length not a multiple of 8: exercises the overlapping word tail.
(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    const N = 32;
    const array = [];
    for (let i = 0; i < N; ++i)
        array.push(freshCopy(String.fromCharCode(65 + i) + "y".repeat(12)));

    for (let i = 0; i < testLoopCount; ++i) {
        const targetIdx = i % N;
        const target = freshCopy(String.fromCharCode(65 + targetIdx) + "y".repeat(12));
        shouldBe(indexOf(array, target), targetIdx);
        const headMismatch = freshCopy("@" + "y".repeat(12));
        shouldBe(indexOf(array, headMismatch), -1);
    }
}());

// Short strings (length < 8) still go through the byte loop.
(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    const N = 50;
    const array = [];
    for (let i = 0; i < N; ++i)
        array.push(freshCopy("ab" + String.fromCharCode(65 + i)));

    for (let i = 0; i < testLoopCount; ++i) {
        const targetIdx = i % N;
        const target = freshCopy("ab" + String.fromCharCode(65 + targetIdx));
        shouldBe(indexOf(array, target), targetIdx);
        shouldBe(indexOf(array, freshCopy("ab@")), -1);
    }
}());

// 16-bit search element bails to the slow path; verify it still produces the right result.
(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    function includes(array, value)
    {
        return array.includes(value);
    }
    noInline(includes);

    const N = 32;
    const array = [];
    for (let i = 0; i < N; ++i)
        array.push(freshCopy(String.fromCharCode(0x3041 + i) + "あ".repeat(11)));

    for (let i = 0; i < testLoopCount; ++i) {
        const targetIdx = i % N;
        const target = freshCopy(String.fromCharCode(0x3041 + targetIdx) + "あ".repeat(11));
        shouldBe(indexOf(array, target), targetIdx);
        shouldBe(includes(array, target), true);
        const missing = freshCopy("ヿ" + "あ".repeat(11));
        shouldBe(indexOf(array, missing), -1);
        shouldBe(includes(array, missing), false);
    }
}());

// Mixed-width: 8-bit search vs 16-bit array elements that compare equal goes to the slow path.
(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    const N = 32;
    const array = [];
    for (let i = 0; i < N; ++i) {
        // Force 16-bit storage even though the contents are Latin-1.
        const s = ("あ" + String.fromCharCode(65 + i) + "z".repeat(10)).slice(1);
        array.push(s);
    }

    for (let i = 0; i < testLoopCount; ++i) {
        const targetIdx = i % N;
        const target = freshCopy(String.fromCharCode(65 + targetIdx) + "z".repeat(10));
        shouldBe(indexOf(array, target), targetIdx);
    }
}());
