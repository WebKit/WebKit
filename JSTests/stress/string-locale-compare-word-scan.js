function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// A 16-bit string never takes the 8-bit inline path, so widening both sides yields the reference
// answer for the same code points.
function reference(lhs, rhs) {
    return $vm.make16BitStringIfPossible(lhs).localeCompare($vm.make16BitStringIfPossible(rhs));
}

// Concatenation yields a rope, which the inline path declines, so force resolution first.
function resolve(string) {
    string.charCodeAt(0);
    return string;
}

var cases = [];
function addCase(lhs, rhs) {
    cases.push([resolve(lhs), resolve(rhs), reference(lhs, rhs)]);
}

var alphabet = "abcdefghijklmnopqrstuvwxyz";
function make(length) {
    var string = "";
    for (var i = 0; i < length; ++i)
        string += alphabet[i % alphabet.length];
    return string;
}

// The inline path scans 8 bytes at a time and finishes the remainder with a single overlapping
// 8-byte load, so the lengths that matter are those straddling a multiple of 8. 24 and 25 also
// run the word loop three times.
var lengths = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 24, 25];

for (var lengthIndex = 0; lengthIndex < lengths.length; ++lengthIndex) {
    var length = lengths[lengthIndex];
    var base = make(length);

    // Equal content in distinct StringImpls: the whole scan runs, including the overlapping load.
    addCase(base, make(length));

    // A difference at every position covers every byte lane of both the word loop and that
    // final overlapping load.
    for (var position = 0; position < length; ++position) {
        var head = base.slice(0, position);
        var tail = base.slice(position + 1);

        addCase(base, head + "Z" + tail);
        // Case differences match at DUCET level 1, so the scan has to continue past them.
        addCase(base, head + base[position].toUpperCase() + tail);
        // One side non-ASCII forces a byte-by-byte rescan of the word pair.
        addCase(base, head + "\u00FF" + tail);
        // Equal but non-ASCII needs full collation.
        addCase(head + "\u00FF" + tail, head + "\u00FF" + tail);
        // A difference in the common part of strings of unequal length: resolving it at the wrong
        // position would silently fall through to the shorter-string answer instead.
        addCase(base, head + "Z" + tail + "q");

        // Swapping this position with the last one makes the first and last differences point in
        // opposite directions, so identifying the wrong differing position inverts the result.
        if (position < length - 1) {
            var swapped = head + base[length - 1] + base.slice(position + 1, length - 1) + base[position];
            addCase(base, swapped);
        }
    }

    // A shared prefix with differing lengths, with the extra tail falling short of, landing on,
    // and reaching past a word boundary.
    var extras = [1, 7, 8, 9];
    for (var extraIndex = 0; extraIndex < extras.length; ++extraIndex) {
        var longer = base + make(extras[extraIndex]);
        addCase(base, longer);
        addCase(longer, base);
    }
}

function check(lhs, rhs, expected) {
    shouldBe(lhs.localeCompare(rhs), expected);
}
noInline(check);

// The path under test is emitted by the FTL, and the DFG-to-FTL threshold is scaled per code
// block, so a function this small needs tens of thousands of calls before it is compiled there.
// A smaller count leaves the compiled code untested.
var iterations = Math.max(testLoopCount, 150000);
for (var i = 0; i < iterations; ++i) {
    var testCase = cases[i % cases.length];
    check(testCase[0], testCase[1], testCase[2]);
}
