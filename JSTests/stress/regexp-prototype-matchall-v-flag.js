function assertSameArray(a, b) {
    if (a.toString() !== b.toString())
        throw new Error(`Expected ${a} but got ${b}`);
}

function doMatchAll(regex) {
    // 𠮷 is a surrogate pair
    const text = '𠮷a𠮷';
    const matches = [...RegExp.prototype[Symbol.matchAll].call(regex, text)];
    return matches.map(match => match.index);
}

assertSameArray([0, 1, 2, 3, 4, 5], doMatchAll(/(?:)/g));
assertSameArray([0, 2, 3, 5], doMatchAll(/(?:)/gu));
assertSameArray([0, 2, 3, 5], doMatchAll(/(?:)/gv));
