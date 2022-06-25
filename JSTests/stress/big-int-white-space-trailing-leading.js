function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function assertThrowSyntaxError(input) {
    try {
        eval(input);
        assert(false);
    } catch (e) {
        assert(e instanceof SyntaxError);
    }
}

var d;

assert(eval("d=\u000C5n") === 5n);
assert(d === 5n);

assert(eval("d=\u000915n") === 15n);
assert(d === 15n);

assert(eval("d=\u000B19n\u000B;") === 19n);
assert(d === 19n);

assert(eval("d=\u000C119n;") === 119n);
assert(d === 119n);

assert(eval("d=\u002095n;") === 95n);
assert(d === 95n);

assert(eval("d=\u00A053n;") === 53n);
assert(d === 53n);

assert(eval("d=\uFEFF39n;") === 39n);
assert(d === 39n);

assert(eval("d=5n\u000C") === 5n);
assert(d === 5n);

assert(eval("d=15n\u0009") === 15n);
assert(d === 15n);

assert(eval("d=19n\u000B;") === 19n);
assert(d === 19n);

assert(eval("d=119n\u000C;") === 119n);
assert(d === 119n);

assert(eval("d=95n\u0020;") === 95n);
assert(d === 95n);

assert(eval("d=53n\u00A0;") === 53n);
assert(d === 53n);

assert(eval("d=39n\uFEFF;") === 39n);
assert(d === 39n);

assert(eval("\u000C\u000Cd\u000C\u000C=\u000C\u000C5n\u000C;\u000C") === 5n);
assert(d === 5n);

assert(eval("\u0009\u0009d\u0009\u0009=\u0009\u000915n\u0009;") === 15n);
assert(d === 15n);

assert(eval("\u000B\u000Bd\u000B\u000B=\u000B\u000B19n\u000B;") === 19n);
assert(d === 19n);

assert(eval("\u000C\u000Cd\u000C=\u000C\u000C119n;") === 119n);
assert(d === 119n);

assert(eval("\u0020d\u0020=\u0020\u002095n;") === 95n);
assert(d === 95n);

assert(eval("\u00A0d\u00A0=\u00A0\u00A053n;") === 53n);
assert(d === 53n);

assert(eval("\uFEFFd\uFEFF=\uFEFF\uFEFF39n;") === 39n);
assert(d === 39n);

// Assert errors

assertThrowSyntaxError("0b\u000C2n");
assertThrowSyntaxError("0b\u000B1101n");
assertThrowSyntaxError("0b\u0009111111n");
assertThrowSyntaxError("0b\u002010101n");
assertThrowSyntaxError("0b\u00A01011n");
assertThrowSyntaxError("0b\uFEFF111000n");

assertThrowSyntaxError("0o\u000C2n");
assertThrowSyntaxError("0o\u000B45n");
assertThrowSyntaxError("0o\u000977n");
assertThrowSyntaxError("0o\u0020777n");
assertThrowSyntaxError("0o\u00A01777n");
assertThrowSyntaxError("0o\uFEFF17361n");

assertThrowSyntaxError("0x\u000C2n");
assertThrowSyntaxError("0x\u000B45n");
assertThrowSyntaxError("0x\u000977n");
assertThrowSyntaxError("0x\u0020777n");
assertThrowSyntaxError("0x\u00A01777n");
assertThrowSyntaxError("0x\uFEFF17361n");

assertThrowSyntaxError("2\u000Cn");
assertThrowSyntaxError("45\u000Bn");
assertThrowSyntaxError("77\u0009n");
assertThrowSyntaxError("777\u0020n");
assertThrowSyntaxError("1777\u00A0n");
assertThrowSyntaxError("17361\uFEFFn");

