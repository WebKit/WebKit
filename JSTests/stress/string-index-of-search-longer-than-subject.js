function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} got ${actual}`);
}

// A needle that cannot fit in the remaining subject must report "not found" without disturbing any
// of the other cases, including the empty needle, which matches at the start position.
const nonLatin1 = "—";

function indexOf(subject, search) { return subject.indexOf(search); }
noInline(indexOf);
function indexOfAt(subject, search, position) { return subject.indexOf(search, position); }
noInline(indexOfAt);
function includes(subject, search) { return subject.includes(search); }
noInline(includes);
function includesAt(subject, search, position) { return subject.includes(search, position); }
noInline(includesAt);

const subjects = ["", "a", "abc", "abcabcabc", "abc" + nonLatin1, nonLatin1 + "abc"];
const searches = ["", "a", "c", "abc", "abcd", "abcabcabc", "abcabcabca", nonLatin1, "x"];

// Compute expectations with a straightforward reference implementation.
function referenceIndexOf(subject, search, position) {
    const start = Math.min(Math.max(position | 0, 0), subject.length);
    for (let i = start; i + search.length <= subject.length; ++i) {
        let match = true;
        for (let j = 0; j < search.length; ++j) {
            if (subject[i + j] !== search[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }
    return -1;
}

const positions = [0, 1, 2, 3, 100];
for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (const subject of subjects) {
        for (const search of searches) {
            const expected = referenceIndexOf(subject, search, 0);
            shouldBe(indexOf(subject, search), expected);
            shouldBe(includes(subject, search), expected !== -1);
            for (const position of positions) {
                const expectedAt = referenceIndexOf(subject, search, position);
                shouldBe(indexOfAt(subject, search, position), expectedAt);
                shouldBe(includesAt(subject, search, position), expectedAt !== -1);
            }
        }
    }
}

// Ropes: the length check must not consult a resolved view.
function indexOfRope(a, b, search) { return (a + b).indexOf(search); }
noInline(indexOfRope);
for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    shouldBe(indexOfRope("abc", "def", "cd"), 2);
    shouldBe(indexOfRope("abc", "def", "abcdefg"), -1);
    shouldBe(indexOfRope("abc", "def", "f"), 5);
    shouldBe(indexOfRope("abc", nonLatin1, nonLatin1), 3);
}
