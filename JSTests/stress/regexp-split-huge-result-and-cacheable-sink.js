//@ $skipModes << :lockdown

// The "absurdly large result" path, taken when a split collects more spans than the span cap
// (100000). It materializes what the span loop collected, does a dry run to bound the size, and then
// finishes the split directly into the array. Which pass produces the match the legacy RegExp
// statics end up holding depends on whether the finishing pass matches anything at all, so both
// cases are covered below.
//
// Also the choice between collecting a specific-pattern split as spans and building it straight into
// an array. Only a result collected as spans can become a cached butterfly, so a cacheable split has
// to take the span route; building it directly is invisible in the elements and shows up only as the
// result no longer being a copy-on-write array served by the StringSplitCache.

function shouldBe(actual, expected, name) {
    if (actual !== expected)
        throw new Error("FAIL " + name + ": got=" + JSON.stringify(actual) + " expected=" + JSON.stringify(expected));
}

// Gives a string an atom StringImpl, which is what the split cache requires of a subject.
function atomize(string) {
    let object = {};
    object[string] = 1;
    return Object.keys(object)[0];
}

// 120000 matches of /e(\d)/ produce 240001 elements, well past the 100000 span cap.
const hugeSeparatorCount = 120000;
let hugeParts = [];
for (let i = 0; i < hugeSeparatorCount; ++i)
    hugeParts.push("e" + (i % 10));
const hugeSubject = hugeParts.join("");

// Points the statics at an unrelated match, so a path that fails to update them is visible.
function clobber() {
    /seed(s)/.exec("hello seeds world");
}

// The final match of splitting hugeSubject by /e(\d)/ is the trailing "e9".
{
    clobber();
    let result = hugeSubject.split(/e(\d)/);
    shouldBe(result.length, 2 * hugeSeparatorCount + 1, "huge length");
    shouldBe(result[0], "", "huge [0]");
    shouldBe(result[1], "0", "huge [1]");
    shouldBe(result[result.length - 1], "", "huge last");

    shouldBe(RegExp.input, hugeSubject, "huge input");
    shouldBe(RegExp.lastMatch, "e9", "huge lastMatch");
    shouldBe(RegExp.$1, "9", "huge $1");
    // The last match is the final two characters, so nothing follows it.
    shouldBe(RegExp.leftContext.length, hugeSubject.length - 2, "huge leftContext length");
    shouldBe(RegExp.rightContext, "", "huge rightContext");
}

// A limit exactly at the span cap must still be honored: the split has more elements than the cap,
// so it is only the limit that stops it, and the trailing element must not be appended on top.
for (let limit of [99999, 100000, 100001]) {
    let result = hugeSubject.split(/e(\d)/, limit);
    shouldBe(result.length, limit, "huge limit=" + limit + " length");
    // Elements alternate between a separated piece and the capture, starting with a piece.
    let last = result[result.length - 1];
    shouldBe(last, limit % 2 ? "" : String((limit / 2 - 1) % 10), "huge limit=" + limit + " last");
}

// A limit above the element count leaves the result complete, still through the huge path.
{
    clobber();
    let result = hugeSubject.split(/e(\d)/, 0xFFFFFFFF);
    shouldBe(result.length, 2 * hugeSeparatorCount + 1, "huge unlimited length");
    shouldBe(RegExp.lastMatch, "e9", "huge unlimited lastMatch");
    shouldBe(RegExp.rightContext, "", "huge unlimited rightContext");
}

// A separator ending exactly at the end of the subject leaves the finishing pass with nothing to
// match, so the statics have to come from the match the span loop stopped on instead. Sizes around
// the cap cover the boundary in both directions.
for (let separatorCount of [99999, 100000, 100001]) {
    clobber();
    let subject = ",".repeat(separatorCount);
    let result = subject.split(/,|,/);
    shouldBe(result.length, separatorCount + 1, "trailing separator count=" + separatorCount + " length");
    shouldBe(result[0], "", "trailing separator count=" + separatorCount + " [0]");
    shouldBe(result[result.length - 1], "", "trailing separator count=" + separatorCount + " last");
    shouldBe(RegExp.lastMatch, ",", "trailing separator count=" + separatorCount + " lastMatch");
    shouldBe(RegExp.leftContext.length, separatorCount - 1, "trailing separator count=" + separatorCount + " leftContext length");
    shouldBe(RegExp.rightContext, "", "trailing separator count=" + separatorCount + " rightContext");
}

// A cacheable specific-pattern split whose result outgrows the span cap cannot be cached, so it
// abandons the spans it collected and rebuilds the result straight into an array.
for (let separatorCount of [99999, 100000, 100001, 150000]) {
    clobber();
    let subject = atomize("a," .repeat(separatorCount));
    let result = subject.split(/,/);
    shouldBe(result.length, separatorCount + 1, "uncacheably large count=" + separatorCount + " length");
    shouldBe(result[0], "a", "uncacheably large count=" + separatorCount + " [0]");
    shouldBe(result[result.length - 1], "", "uncacheably large count=" + separatorCount + " last");
    shouldBe(RegExp.lastMatch, ",", "uncacheably large count=" + separatorCount + " lastMatch");
    shouldBe(RegExp.rightContext, "", "uncacheably large count=" + separatorCount + " rightContext");
    // Only a result small enough for the cache is served as a copy-on-write butterfly.
    let expectedMode = separatorCount + 1 < 100000 ? "CopyOnWriteArrayWithContiguous" : "ArrayWithContiguous";
    shouldBe($vm.indexingMode(result), expectedMode, "uncacheably large count=" + separatorCount + " indexingMode");
}

// A cacheable split must be collected as spans so that it can be cached, which makes the result a
// copy-on-write array. Building it directly into an array instead is otherwise unobservable.
function shouldBeCached(regexp, subject, name) {
    // The subject has to be an atom string for the split cache to accept it, which a literal is.
    let first = subject.split(regexp);
    shouldBe($vm.indexingMode(first), "CopyOnWriteArrayWithContiguous", name + " first indexingMode");

    let expectedElements = first.join("|");
    let expectedLastMatch = RegExp.lastMatch;
    let expectedLeftContext = RegExp.leftContext;
    let expectedRightContext = RegExp.rightContext;

    // A split served out of the cache skips the match loop, so it has to republish the statics
    // itself. Clobbering them first makes a missing replay visible, and repeating enough times
    // covers the cache being dropped by a collection partway through.
    for (let i = 0; i < 2e3; ++i) {
        clobber();
        let repeat = subject.split(regexp);
        shouldBe($vm.indexingMode(repeat), "CopyOnWriteArrayWithContiguous", name + " repeat indexingMode");
        shouldBe(repeat.join("|"), expectedElements, name + " repeat elements");
        shouldBe(RegExp.lastMatch, expectedLastMatch, name + " repeat lastMatch");
        shouldBe(RegExp.leftContext, expectedLeftContext, name + " repeat leftContext");
        shouldBe(RegExp.rightContext, expectedRightContext, name + " repeat rightContext");
        // Writing to one result must not be visible in the next split of the same subject.
        repeat[0] = "MUTATED";
    }

    // After the cache is dropped the split is recomputed, and must still be cacheable.
    $vm.clearStringSplitCache();
    let recomputed = subject.split(regexp);
    shouldBe($vm.indexingMode(recomputed), "CopyOnWriteArrayWithContiguous", name + " recomputed indexingMode");
    shouldBe(recomputed.join("|"), expectedElements, name + " recomputed elements");
}

shouldBeCached(/,/, "a,b,c", "atom");
shouldBeCached(/--/, "a--b--c", "atom multi character");
shouldBeCached(/\r\n?|\n/, "a\nb\r\nc\rd", "newlines");
shouldBeCached(/^\s*/, "  abc", "leading spaces star");
shouldBeCached(/^\s+/, "  abc", "leading spaces plus");
shouldBeCached(/\s*$/, "abc  ", "trailing spaces star");
shouldBeCached(/\s+$/, "abc  ", "trailing spaces plus");
// The generic path is cacheable on the same terms.
shouldBeCached(/,|,/, "a,b,c", "generic");

// A result of 100 elements or more is cached without the atom-strings structure, so it crosses the
// cache's own size split as well as staying under the span cap.
{
    let manyElements = atomize("a,".repeat(300));
    shouldBeCached(/,/, manyElements, "atom many elements");
    let nonIdentifierElements = atomize("0-a,".repeat(300));
    shouldBeCached(/,/, nonIdentifierElements, "atom many non identifier elements");
}

// A limit makes a split uncacheable, so those results are built directly and are not copy-on-write.
{
    let limited = "a,b,c".split(/,/, 2);
    shouldBe($vm.indexingMode(limited), "ArrayWithContiguous", "limited indexingMode");
    shouldBe(limited.join("|"), "a|b", "limited elements");
}

// The shortest subjects still have to reach the direct-array sink. A rope of a one character string
// is that same string, so a limit is what takes those cases off the cacheable path.
for (let subject of ["", ",", "a", " ", "\n"]) {
    for (let regexp of [/,/, /\r\n?|\n/, /^\s*/, /^\s+/, /\s*$/, /\s+$/]) {
        let direct = subject.split(regexp, 0xFFFFFFFE);
        let oracle = subject.split(new RegExp(regexp.source + "|" + regexp.source), 0xFFFFFFFE);
        let name = "short " + JSON.stringify(subject) + " " + regexp;
        shouldBe(direct.length, oracle.length, name + " length");
        shouldBe(direct.join("|"), oracle.join("|"), name + " elements");
        if (subject.length)
            shouldBe($vm.indexingMode(direct), "ArrayWithContiguous", name + " indexingMode");
    }
}
