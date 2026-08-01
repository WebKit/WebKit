function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function repeat(fn) {
    for (let i = 0; i < testLoopCount; ++i)
        fn();
}

// Non-Latin-1 set operations are evaluated in 2048 character chunks starting at
// the lowest character of either operand, so the chunk boundaries below fall at
// 0x8FF/0x900 and 0x10FF/0x1100.

// The right hand side runs out of characters in the very first chunk while the
// left hand side continues to the top of the code point space.
repeat(() => {
    let re = /[[\u0100-\u{10FFFF}]&&[\u0100-\u0101]]/v;
    shouldBe(re.test("\u0100"), true, "first chunk RHS lo");
    shouldBe(re.test("\u0101"), true, "first chunk RHS hi");
    shouldBe(re.test("\u0102"), false, "first chunk just above RHS");
    shouldBe(re.test("\u2000"), false, "first chunk later chunk char");
    shouldBe(re.test("\u{10FFFF}"), false, "first chunk top of code space");
    shouldBe(re.test("a"), false, "first chunk Latin-1");
});

repeat(() => {
    let re = /[\p{L}&&[\u0100-\u0101]]/v;
    shouldBe(re.test("\u0100"), true, "property LHS RHS lo");
    shouldBe(re.test("\u0101"), true, "property LHS RHS hi");
    shouldBe(re.test("\u0102"), false, "property LHS above RHS");
    shouldBe(re.test("\u{10400}"), false, "property LHS non-BMP letter");
    shouldBe(re.test("5"), false, "property LHS digit");
});

// The right hand side runs out in a later chunk.
repeat(() => {
    let re = /[[\u0100-\u{10FFFF}]&&[\u1000-\u1001]]/v;
    shouldBe(re.test("\u1000"), true, "later chunk RHS lo");
    shouldBe(re.test("\u1001"), true, "later chunk RHS hi");
    shouldBe(re.test("\u0fff"), false, "later chunk below RHS");
    shouldBe(re.test("\u1002"), false, "later chunk above RHS");
    shouldBe(re.test("\u0100"), false, "later chunk first chunk char");
});

// The last right hand side range straddles a chunk boundary.
repeat(() => {
    let re = /[[\u0100-\u{10FFFF}]&&[\u08f0-\u0910]]/v;
    shouldBe(re.test("\u08f0"), true, "straddling RHS lo");
    shouldBe(re.test("\u08ff"), true, "straddling RHS last char of chunk");
    shouldBe(re.test("\u0900"), true, "straddling RHS first char of next chunk");
    shouldBe(re.test("\u0910"), true, "straddling RHS hi");
    shouldBe(re.test("\u08ef"), false, "straddling RHS below");
    shouldBe(re.test("\u0911"), false, "straddling RHS above");
});

// The last right hand side entry is a single character rather than a range.
repeat(() => {
    let re = /[[\u0100-\u{10FFFF}]&&[\u0100\u2000]]/v;
    shouldBe(re.test("\u0100"), true, "single char RHS first");
    shouldBe(re.test("\u2000"), true, "single char RHS last");
    shouldBe(re.test("\u0101"), false, "single char RHS gap");
    shouldBe(re.test("\u2001"), false, "single char RHS above");
});

// The whole right hand side sits below everything in the left hand side, so the
// intersection is empty.
repeat(() => {
    let re = /[[\u{10000}-\u{10FFFF}]&&[\u0100-\u0200]]/v;
    shouldBe(re.test("\u0150"), false, "disjoint RHS char");
    shouldBe(re.test("\u{10000}"), false, "disjoint LHS lo");
    shouldBe(re.test("\u{10FFFF}"), false, "disjoint LHS hi");
});

repeat(() => {
    let re = /[[\u0100-\u0200]&&[\u{10000}-\u{10FFFF}]]/v;
    shouldBe(re.test("\u0150"), false, "disjoint reversed LHS char");
    shouldBe(re.test("\u{10000}"), false, "disjoint reversed RHS lo");
});

// Union and subtraction keep the part of the left hand side that lies above the
// end of the right hand side.
repeat(() => {
    let re = /[[\u0100-\u{10FFFF}]--[\u0100-\u0101]]/v;
    shouldBe(re.test("\u0100"), false, "subtraction removed lo");
    shouldBe(re.test("\u0101"), false, "subtraction removed hi");
    shouldBe(re.test("\u0102"), true, "subtraction keeps just above");
    shouldBe(re.test("\u{10FFFF}"), true, "subtraction keeps top of code space");
});

repeat(() => {
    let re = /[[\u0100-\u0101][\u{10000}-\u{10001}]]/v;
    shouldBe(re.test("\u0100"), true, "union low operand");
    shouldBe(re.test("\u{10000}"), true, "union high operand");
    shouldBe(re.test("\u0102"), false, "union gap");
});

repeat(() => {
    let re = /[^[\p{L}&&[\u0100-\u0101]]]/v;
    shouldBe(re.test("\u0100"), false, "inverted intersection member");
    shouldBe(re.test("\u0102"), true, "inverted intersection non-member");
    shouldBe(re.test("a"), true, "inverted intersection Latin-1");
});

repeat(() => {
    let re = /[[[\u0100-\u{10FFFF}]&&[\u0100-\u2000]]&&[\u0101-\u0102]]/v;
    shouldBe(re.test("\u0101"), true, "chained intersection lo");
    shouldBe(re.test("\u0102"), true, "chained intersection hi");
    shouldBe(re.test("\u0100"), false, "chained intersection below");
    shouldBe(re.test("\u0103"), false, "chained intersection above");
});

// Latin-1 and non-Latin-1 members of the same intersection.
repeat(() => {
    let re = /[[\u0080-\u{10FFFF}]&&[\u0090\u0100]]/v;
    shouldBe(re.test("\u0090"), true, "mixed width Latin-1 member");
    shouldBe(re.test("\u0100"), true, "mixed width non-Latin-1 member");
    shouldBe(re.test("\u0091"), false, "mixed width Latin-1 non-member");
    shouldBe(re.test("\u0101"), false, "mixed width non-Latin-1 non-member");
});

repeat(() => {
    let re = /[[\u0100-\u{10FFFF}]&&[\u0100-\u0101]]+/v;
    let m = re.exec("\u00ff\u0100\u0101\u0102");
    shouldBe(m !== null, true, "exec found a match");
    shouldBe(m[0], "\u0100\u0101", "exec match");
    shouldBe(m.index, 1, "exec index");
});

repeat(() => {
    let re = new RegExp("[[\\u0100-\\u{10FFFF}]&&[\\u0100-\\u0101]]", "v");
    shouldBe(re.test("\u0101"), true, "dynamic pattern member");
    shouldBe(re.test("\u{10FFFF}"), false, "dynamic pattern non-member");
});
