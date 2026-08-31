function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} got ${actual}`);
}

// A needle that cannot fit in the searched range must report "not found" without disturbing any of
// the other cases, including the empty needle, which always matches.
const nonLatin1 = "\u2014";

function lastIndexOf(subject, search) { return subject.lastIndexOf(search); }
noInline(lastIndexOf);
function lastIndexOfAt(subject, search, position) { return subject.lastIndexOf(search, position); }
noInline(lastIndexOfAt);
function startsWith(subject, search) { return subject.startsWith(search); }
noInline(startsWith);
function startsWithAt(subject, search, position) { return subject.startsWith(search, position); }
noInline(startsWithAt);
function endsWith(subject, search) { return subject.endsWith(search); }
noInline(endsWith);
function endsWithAt(subject, search, position) { return subject.endsWith(search, position); }
noInline(endsWithAt);

// Compute expectations with straightforward reference implementations.
function matchesAt(subject, search, index) {
    if (index < 0 || index + search.length > subject.length)
        return false;
    for (let i = 0; i < search.length; ++i) {
        if (subject[index + i] !== search[i])
            return false;
    }
    return true;
}

function referenceLastIndexOf(subject, search, position) {
    const maxStart = subject.length - search.length;
    if (maxStart < 0)
        return -1;
    let start = maxStart;
    if (position !== undefined)
        start = Math.min(Math.max(position | 0, 0), maxStart);
    for (let i = start; i >= 0; --i) {
        if (matchesAt(subject, search, i))
            return i;
    }
    return -1;
}

function referenceStartsWith(subject, search, position) {
    const start = Math.min(Math.max((position === undefined ? 0 : position) | 0, 0), subject.length);
    return matchesAt(subject, search, start);
}

function referenceEndsWith(subject, search, position) {
    const end = position === undefined ? subject.length : Math.min(Math.max(position | 0, 0), subject.length);
    return matchesAt(subject, search, end - search.length);
}

const subjects = ["", "a", "abc", "abcabcabc", "abc" + nonLatin1, nonLatin1 + "abc"];
const searches = ["", "a", "c", "abc", "abcd", "abcabcabc", "abcabcabca", nonLatin1, "x"];
const positions = [0, 1, 2, 3, 100];

const cases = [];
for (const subject of subjects) {
    for (const search of searches) {
        cases.push({
            subject, search,
            lastIndexOf: referenceLastIndexOf(subject, search, undefined),
            startsWith: referenceStartsWith(subject, search, undefined),
            endsWith: referenceEndsWith(subject, search, undefined),
            at: positions.map(position => ({
                position,
                lastIndexOf: referenceLastIndexOf(subject, search, position),
                startsWith: referenceStartsWith(subject, search, position),
                endsWith: referenceEndsWith(subject, search, position),
            })),
        });
    }
}

for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (const c of cases) {
        shouldBe(lastIndexOf(c.subject, c.search), c.lastIndexOf);
        shouldBe(startsWith(c.subject, c.search), c.startsWith);
        shouldBe(endsWith(c.subject, c.search), c.endsWith);
        for (const at of c.at) {
            shouldBe(lastIndexOfAt(c.subject, c.search, at.position), at.lastIndexOf);
            shouldBe(startsWithAt(c.subject, c.search, at.position), at.startsWith);
            shouldBe(endsWithAt(c.subject, c.search, at.position), at.endsWith);
        }
    }
}

// Ropes: the length checks must not consult a resolved view, on either the subject or the needle.
// "long" is longer than JSString::minLengthForRopeWalk, so it also exercises the rope-walking paths.
const long = "abcdefgh".repeat(38);

function lastIndexOfRopeSubject(a, b, search) { return (a + b).lastIndexOf(search); }
noInline(lastIndexOfRopeSubject);
function startsWithRopeSubject(a, b, search) { return (a + b).startsWith(search); }
noInline(startsWithRopeSubject);
function endsWithRopeSubject(a, b, search) { return (a + b).endsWith(search); }
noInline(endsWithRopeSubject);
function lastIndexOfRopeSearch(subject, c, d) { return subject.lastIndexOf(c + d); }
noInline(lastIndexOfRopeSearch);
function startsWithRopeSearch(subject, c, d) { return subject.startsWith(c + d); }
noInline(startsWithRopeSearch);
function endsWithRopeSearch(subject, c, d) { return subject.endsWith(c + d); }
noInline(endsWithRopeSearch);
function lastIndexOfRopeSubjectAt(a, b, search, position) { return (a + b).lastIndexOf(search, position); }
noInline(lastIndexOfRopeSubjectAt);

const ropeCases = [
    ["abc", "def", "cd"],
    ["abc", "def", "abcdefg"],
    ["abc", nonLatin1, nonLatin1],
    ["abc", "def", ""],
    [long, "abc", "c"],
    [long, "abc", "cd"],
    ["abc", long, long + "x"],
    ["", "abc", "abcd"],
].map(([a, b, search]) => {
    const joined = a + b;
    return {
        a, b, search,
        lastIndexOf: referenceLastIndexOf(joined, search, undefined),
        startsWith: referenceStartsWith(joined, search, undefined),
        endsWith: referenceEndsWith(joined, search, undefined),
        lastIndexOfAtFour: referenceLastIndexOf(joined, search, 4),
        lastIndexOfAtMinusOne: referenceLastIndexOf(joined, search, -1),
    };
});

for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (const c of ropeCases) {
        shouldBe(lastIndexOfRopeSubject(c.a, c.b, c.search), c.lastIndexOf);
        shouldBe(startsWithRopeSubject(c.a, c.b, c.search), c.startsWith);
        shouldBe(endsWithRopeSubject(c.a, c.b, c.search), c.endsWith);
        shouldBe(lastIndexOfRopeSubjectAt(c.a, c.b, c.search, 4), c.lastIndexOfAtFour);
        shouldBe(lastIndexOfRopeSubjectAt(c.a, c.b, c.search, -1), c.lastIndexOfAtMinusOne);
    }

    // The needle is a rope that is longer than the subject, so it must be rejected without being
    // resolved.
    shouldBe(lastIndexOfRopeSearch("abc", long, long), -1);
    shouldBe(startsWithRopeSearch("abc", long, long), false);
    shouldBe(endsWithRopeSearch("abc", long, long), false);
    shouldBe(lastIndexOfRopeSearch("abcabc", "ab", "c"), 3);
    shouldBe(startsWithRopeSearch("abcabc", "ab", "c"), true);
    shouldBe(endsWithRopeSearch("abcabc", "ab", "c"), true);
}
