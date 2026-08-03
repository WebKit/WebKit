// Verify that the anchored (^, non-multiline) RegExp first-character fast-fail filter now covers
// inverted leading character classes ([^...], \D, \S, \W) and patterns that begin with a
// quantified capturing group ((...){n}). These must be filtered when input[0] cannot begin a match
// and must never wrongly fast-fail a subject the pattern would actually match. Empty-matchable
// leading atoms (e.g. [...]*, (...)?) must still fall back and never be filtered.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function step() {
    // Inverted leading class: [^X] matches byte c iff c not in X.
    shouldBe(/^[^\d\W]\w*/.test("hello"), true);   // 'h' passes filter, matches
    shouldBe(/^[^\d\W]\w*/.test("1abc"), false);   // '1' filtered ('1' in \d)
    shouldBe(/^[^\d\W]\w*/.test("!x"), false);     // '!' filtered ('!' in \W)
    shouldBe(/^[^\d\W]\w*/.test("_id"), true);     // '_' is a word char, not a digit

    shouldBe(/^[^a-z]/.test("A"), true);           // 'A' not in a-z
    shouldBe(/^[^a-z]/.test("a"), false);          // 'a' filtered
    shouldBe(/^[^a-z]/.test("9"), true);
    shouldBe(/^[^a-z]/.test("z"), false);          // 'z' filtered

    // Inverted builtin classes.
    shouldBe(/^\D/.test("x"), true);
    shouldBe(/^\D/.test("5"), false);              // digit filtered
    shouldBe(/^\S/.test("y"), true);
    shouldBe(/^\S/.test(" "), false);              // space filtered
    shouldBe(/^\S/.test("\t"), false);             // tab filtered
    shouldBe(/^\W/.test("."), true);
    shouldBe(/^\W/.test("a"), false);              // word char filtered
    shouldBe(/^\W/.test("_"), false);              // '_' is a word char

    // Inverted high range: complement over Latin-1 must include all bytes < 0x100 that are not in
    // the (entirely non-Latin1) range, so ASCII inputs must pass the filter. Note the resulting
    // bitmap is FULL here, so no filter is installed at all - the partial cases below are the ones
    // that actually exercise the complement arithmetic.
    shouldBe(/^[^\u0100-\uffff]/.test("a"), true);
    shouldBe(/^[^\u0100-\uffff]/.test("\xff"), true);

    // Partial complements: exactly one byte survives, and exactly the low half survives.
    shouldBe(/^[^\x00-\xfe]/.test("\xff"), true);
    shouldBe(/^[^\x00-\xfe]/.test("a"), false);      // 'a' filtered
    shouldBe(/^[^\x00-\xfe]/.test("\xfe"), false);   // 0xfe filtered
    shouldBe(/^[^\u0080-\uffff]/.test("a"), true);
    shouldBe(/^[^\u0080-\uffff]/.test("\x7f"), true);
    shouldBe(/^[^\u0080-\uffff]/.test("\x80"), false);  // 0x80 filtered
    shouldBe(/^[^\u0080-\uffff]/.test("\xff"), false);  // 0xff filtered

    // Inverted classes under the u and v flags reach the same builder.
    shouldBe(/^[^\d]/u.test("x"), true);
    shouldBe(/^[^\d]/u.test("7"), false);            // digit filtered
    shouldBe(/^[^\p{L}]/u.test("!"), true);
    shouldBe(/^[^\p{L}]/u.test("q"), false);         // letter filtered
    shouldBe(/^[^a-z]/v.test("A"), true);
    shouldBe(/^[^a-z]/v.test("m"), false);           // 'm' filtered

    // Case-insensitive inverted class.
    shouldBe(/^[^a-f]/i.test("G"), true);
    shouldBe(/^[^a-f]/i.test("A"), false);         // 'A' folds into a-f, filtered
    shouldBe(/^[^a-f]/i.test("f"), false);

    // Quantified leading capturing group ((...){n}) is now filterable: the group's fixed repeat
    // guarantees a first character drawn from the inner class.
    shouldBe(/^([0-9a-fA-F]){16}$/.test("0123456789abcdef"), true);
    shouldBe(/^([0-9a-fA-F]){16}$/.test("zzzzzzzzzzzzzzzz"), false);  // 'z' filtered
    shouldBe(/^([0-9a-fA-F]){12}$/.test("0123456789ab"), true);
    shouldBe(/^([0-9a-fA-F]){12}$/.test("!23456789abc"), false);      // '!' filtered
    shouldBe(/^(ab)/.test("abc"), true);
    shouldBe(/^(ab)/.test("xyz"), false);                            // 'x' filtered
    shouldBe(/^(([0-9])){3}/.test("123"), true);
    shouldBe(/^(([0-9])){3}/.test("abc"), false);                    // 'a' filtered

    // Empty-matchable leading atoms must NOT be filtered (they can match empty at position 0).
    shouldBe(/^[gmsiyu]*$/.test(""), true);
    shouldBe(/^[gmsiyu]*$/.test("gmi"), true);
    shouldBe(/^[gmsiyu]*$/.test("xyz"), false);   // correctly false, but must not be fast-failed wrongly
    shouldBe(/^(abc)?/.test("abc"), true);
    shouldBe(/^(abc)?/.test("zzz"), true);        // optional group matches empty -> true
    shouldBe(/^\s*$/.test(""), true);
    shouldBe(/^\s*$/.test("   "), true);
    shouldBe(/^\s*$/.test("x"), false);
    // ... including the ones whose minimum size is 0 for a reason other than a quantifier.
    shouldBe(/^a{0}/.test("b"), true);
    shouldBe(/^\b/.test("b"), true);
    shouldBe(/^\b/.test(" b"), false);            // no word boundary at index 0 of " b"
    shouldBe(/^(?:)/.test("z"), true);
    shouldBe(/^$/.test(""), true);
    shouldBe(/^$/.test("z"), false);
    shouldBe(/^(?=a)/.test("ab"), true);
    shouldBe(/^(?=a)/.test("b"), false);
    shouldBe(/^(a*)/.test("zzz"), true);
    shouldBe(/^a|/.test("zzz"), true);

    // Alternation where one alternative starts with an inverted class.
    shouldBe(/^(?:[^0-9]|abc)/.test("x"), true);
    shouldBe(/^(?:[^0-9]|abc)/.test("5"), false); // '5' filtered (both alts reject leading '5')
}

for (var i = 0; i < testLoopCount; ++i)
    step();
