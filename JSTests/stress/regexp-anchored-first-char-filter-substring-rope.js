// Anchored first-character filter on substring-rope inputs (slice()/substring()), including
// String.prototype.match with a RegExp known only after inlining.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function match(re, s) {
    var m = s.match(re);
    return m === null ? null : m[0];
}

var base = "";
for (var i = 0; i < 2000; ++i)
    base += "foo 0x1F ~\n";
base.charCodeAt(0); // Resolve it, so slices are substring ropes.
var base16 = "foo ☃\n".repeat(2000);

for (var i = 0; i < testLoopCount; ++i) {
    var offset = (i % 1000) * 11;
    shouldBe(match(/^foo/, base.slice(offset)), "foo");
    shouldBe(match(/^0x\w+/, base.substring(offset + 4)), "0x1F");
    shouldBe(match(/^#/, base.slice(offset + 9)), null);     // filtered
    shouldBe(/^~/.test(base.slice(offset + 9)), true);
    shouldBe(/^0x/.exec(base.slice(offset)), null);          // filtered

    // Concatenation ropes and 16-bit substrings are not read in place, but must still be right.
    shouldBe(match(/^~/, base.slice(offset + 9) + base), "~");
    shouldBe(match(/^foo/, base16.slice((i % 1000) * 6)), "foo");
    shouldBe(match(/^foo/, base16.slice((i % 1000) * 6 + 4)), null);
}

// The match filter must leave a non-int32 lastIndex to the operation (reading it is observable).
// The fresh literal reaches s.match() through an inlined helper with no branch in between, so DFG
// can only choose its filter at codegen time.
var valueOfCalls = 0;
var lastIndexes = [];
for (var k = 0; k < 100; ++k)
    lastIndexes.push(k ? 0 : { valueOf() { ++valueOfCalls; return 0; } });
for (var i = 0; i < testLoopCount; ++i) {
    var r = /^a/;
    r.lastIndex = lastIndexes[i % 100];
    shouldBe(match(r, "b"), null);
}
shouldBe(valueOfCalls, Math.ceil(testLoopCount / 100));

// A substring rope's base can change width after the rope is created: atomizing an 8-bit base can swap in an
// existing 16-bit atom with the same (Latin-1) content. The filter must use the base's current width.
(function () {
    var length = 3000; // Longer than maxLengthForOnStackResolve, so the 16-bit slice keys a 16-bit atom.
    var wide = "☃" + "o".repeat(length);
    wide.charCodeAt(0); // Resolve it, so the slice shares its 16-bit buffer instead of copying and narrowing.
    var key16 = wide.slice(1);
    var object = { };
    object[key16] = 1;
    var base8 = "o".repeat(length);
    base8.charCodeAt(0);
    var slice = base8.slice(1);
    function exec(s) { return /^o/.exec(s) !== null; }
    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(exec(base8.slice(1 + (i % 7))), true);
        shouldBe(match(/^o/, base8.slice(1 + (i % 7))), "o");
    }
    object[base8] = 2; // base8 now holds the 16-bit atom; `slice` still says 8-bit.
    shouldBe(exec(slice), true);
    shouldBe(match(/^o/, slice), "o");
})();

// The filter must read at the substring's offset: here base[offset] can begin a match but base[0] cannot.
var hashed = "#" + "foo bar\n".repeat(500);
hashed.charCodeAt(0);
for (var i = 0; i < testLoopCount; ++i) {
    var offset = 1 + (i % 400) * 8;
    shouldBe(match(/^foo/, hashed.slice(offset)), "foo");
    shouldBe(/^#/.test(hashed.slice(offset)), false);
}

// String.prototype.match on substring-rope input, through a lexer-style chain of anchored literals passed to a
// nested helper. (Here DFG strength-reduces the match to the filtered exec form; the codegen-time RegExpMatchFast
// filter is covered by the lastIndex test above.)
function classify(str) {
    function scan(s, re) { return s.match(re); }
    var m;
    if (m = scan(str, /^#/))
        return "comment";
    else if (m = scan(str, /^0x[0-9a-fA-F]+/))
        return m[0];
    else if (m = scan(str, /^[a-z]+/))
        return m[0];
    return null;
}
for (var i = 0; i < testLoopCount; ++i) {
    var offset = (i % 1000) * 11;
    shouldBe(classify(base.slice(offset)), "foo");
    shouldBe(classify(base.slice(offset + 4)), "0x1F");
    shouldBe(classify(base.slice(offset + 9)), null);
}

// The filter must not outlive a .compile() of the RegExp it baked in. Keep this last: a .compile() fires the
// realm's RegExp-recompiled watchpoint, after which newly compiled code gets no filter at all.
var re = /^a/;
function check(s) { return s.match(re) !== null; }
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(check("b"), false);
re.compile("^b");
shouldBe(check("b"), true);
