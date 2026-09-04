// Split fast paths keyed on Yarr::SpecificPattern must be indistinguishable from the generic split
// loop, including the legacy RegExp statics (RegExp.input, lastMatch, leftContext, rightContext, $1)
// and the results served out of the StringSplitCache.
//
// Oracle: for any pattern P, `P|P` matches exactly what P does, but having two alternatives makes
// Yarr classify it as SpecificPattern::None, so it always runs the generic path.

function snapshot()
{
    return JSON.stringify([RegExp.input, RegExp.lastMatch, RegExp.leftContext, RegExp.rightContext, RegExp.$1]);
}

function clobber()
{
    // Point the statics at an unrelated match, so a path that forgets to update them, or updates
    // them when it must not, is visible.
    /seed(s)/.exec("seeds are seeds");
}

function runSplit(regexp, subject, limit)
{
    clobber();
    let result = limit === undefined ? subject.split(regexp) : subject.split(regexp, limit);
    return JSON.stringify([result, snapshot()]);
}

// A subject built at runtime has a non-atom StringImpl, which is not cacheable. Concatenating with
// an empty string returns the original string, so for a subject shorter than two characters this is
// the same cacheable string back; the non-maximum limits in checkAllLimits are what cover the
// uncached sink for those.
function rope(string)
{
    return string.substring(0, 1) + string.substring(1);
}

function check(source, flags, subject, limit)
{
    let oracle = new RegExp(source + "|" + source, flags);
    let expected = runSplit(oracle, subject, limit);

    let description = `/${source}/${flags} on ${JSON.stringify(subject)} with limit ${limit}`;
    let fast = new RegExp(source, flags);

    // Run repeatedly: everything after the first run may be served by the StringSplitCache, which
    // has to replay the final match into the statics.
    for (let i = 0; i < 3; ++i) {
        let actual = runSplit(fast, subject, limit);
        if (actual !== expected)
            throw new Error(`${description} run ${i}: expected ${expected} but got ${actual}`);
    }

    let actual = runSplit(fast, rope(subject), limit);
    if (actual !== expected)
        throw new Error(`${description} rope subject: expected ${expected} but got ${actual}`);
}

function checkAllLimits(source, flags, subject)
{
    for (let limit of [undefined, 0, 1, 2, 3, 0xFFFFFFFF, -1])
        check(source, flags, subject, limit);
}

let atomSubjects = [
    "",
    ",",
    ",,",
    ",,,",
    "a,b,c",
    ",a,b,",
    "a",
    "no separator here",
    "aaaa",
    "aa",
    "abab",
    "a\uD83D\uDE00,b",
    "\u1361,\u1362,\u1363",
    "long,,,string,with,many,,commas,,,",
];

// The i, m and y flags are the ones that stop a pattern being classified at all, so they are here to
// catch a classification that ever widens to include them: the fast paths search case-sensitively
// from the current position and anchor only to the ends of the subject.
for (let subject of atomSubjects) {
    for (let source of [",", ",,", "aa", "ab", "a", "long"]) {
        for (let flags of ["", "g", "u", "gd", "i", "m", "y", "gi", "my"])
            checkAllLimits(source, flags, subject);
    }
}

// 16-bit subject and separator.
checkAllLimits("\u1361", "", "a\u1361b\u1361\u1361c");
checkAllLimits("\u1361\u1362", "", "\u1361\u1362x\u1361\u1362");

let spaceSubjects = [
    "",
    " ",
    "   ",
    "abc",
    "  abc",
    "abc  ",
    "  abc  ",
    " a b ",
    // U+00A0, U+1680 and U+FEFF are \s; U+1361 is not.
    "\t\n\u00a0\u1680\uFEFF abc \t",
    "\u1361 ",
    " \u1361",
];

for (let subject of spaceSubjects) {
    for (let source of ["^\\s*", "^\\s+", "\\s*$", "\\s+$"])
        for (let flags of ["", "i", "m", "y"])
            checkAllLimits(source, flags, subject);
}

let newlineSubjects = [
    "",
    "\n",
    "\r",
    "\r\n",
    "a\nb\r\nc\rd",
    "\n\n\n",
    "a\n",
    "\na",
    "a\r\n\r\nb",
];

for (let subject of newlineSubjects) {
    for (let source of ["\\r\\n?|\\n", "\\n|\\r\\n?"])
        for (let flags of ["", "i", "m", "y"])
            checkAllLimits(source, flags, subject);
}

// Results large enough to cross StringSplitCache's atomStringsArrayLimit (100), and elements that do
// not start with an identifier character, which forces the array's structure downgrade. Only an atom
// subject reaches the cache at all, so each case is run both ways.
{
    let identifiers = [];
    let nonIdentifiers = [];
    for (let i = 0; i < 300; ++i) {
        identifiers.push(`element${i}`);
        nonIdentifiers.push(`${i}-element`);
    }

    function atomize(string)
    {
        let object = {};
        object[string] = 1;
        return Object.keys(object)[0];
    }

    for (let subject of [identifiers.join(","), nonIdentifiers.join(",")]) {
        checkAllLimits(",", "", subject);
        checkAllLimits(",", "", atomize(subject));
    }
    checkAllLimits("--", "", identifiers.join("--"));
    checkAllLimits("--", "", atomize(identifiers.join("--")));
}

// Keep this last: an indexed accessor on Object.prototype makes the global object have a bad time,
// which is irreversible. Results are then uncacheable and the arrays are slow-put.
Object.defineProperty(Object.prototype, 0, { get() { return "getterOnPrototype"; }, configurable: true });

for (let subject of ["", ",", "a,b,c", "  abc  ", "a\nb\nc"]) {
    for (let source of [",", "a", "^\\s*", "^\\s+", "\\s*$", "\\s+$", "\\r\\n?|\\n"])
        checkAllLimits(source, "", subject);
}
