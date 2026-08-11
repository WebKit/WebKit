//@ requireOptions("--useRegExpBufferBoundaries=1")

function assertEqual(a, b) {
    if (a !== b)
        throw new Error(`Assertion ${a} === ${b} failed`);
}

const re1 = /\Afoo\z/u;
assertEqual(re1.test("foo"), true);
assertEqual(re1.test("foo\nbar"), false);

const re2 = /\Afoo\z/um;
assertEqual(re2.test("foo"), true);
assertEqual(re2.test("foo\nbar"), false);

// mixing buffer boundaries and anchors
const re3 = /\Afoo|^bar$|baz\z/um;
assertEqual(re3.test("foo"), true);
assertEqual(re3.test("foo\n"), true);
assertEqual(re3.test("\nfoo"), false);
assertEqual(re3.test("bar"), true);
assertEqual(re3.test("bar\n"), true);
assertEqual(re3.test("\nbar"), true);
assertEqual(re3.test("baz"), true);
assertEqual(re3.test("baz\n"), false);
assertEqual(re3.test("\nbaz"), true);

// matching at line terminator sequence at end of buffer
const re4 = /end\Z/u;
assertEqual(re4.test("The end"), true);
assertEqual(re4.test("The end\n"), true);
assertEqual(re4.test("The end\r\n"), true);
assertEqual(re4.test("The end\u2028"), true);
assertEqual(re4.test("The end\n...or is it?"), false);

const re5 = /\Aa/;
assertEqual(re5.test("Aa"), true);
assertEqual(re5.test("bAab"), true);
assertEqual(re5.test("a"), false);
