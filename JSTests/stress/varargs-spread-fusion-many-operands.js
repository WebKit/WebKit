// A fused spread call with many interleaved operands must still compile in the FTL. Each operand
// competes with the code generator's scratch registers, so wide calls exercise the point where the
// register pool runs out.

function assert(b, msg) {
    if (!b)
        throw new Error("Bad assertion: " + msg);
}
noInline(assert);

function collect() { return Array.prototype.slice.call(arguments); }
noInline(collect);

function arrEq(x, y) {
    if (x.length !== y.length)
        return false;
    for (var i = 0; i < x.length; ++i) {
        if (x[i] !== y[i])
            return false;
    }
    return true;
}

var arr = [101, 102];

// Trailing spread takes the inline butterfly fast path, which needs the most scratch registers.
function trailing10(a) { return collect(a, 1, 2, 3, 4, 5, 6, 7, 8, 9, ...arr); }
noInline(trailing10);
function trailing20(a) { return collect(a, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ...arr); }
noInline(trailing20);

// A spread that is not last, and one with a suffix, use the general scratch-buffer path.
function leading20(a) { return collect(...arr, a, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19); }
noInline(leading20);
function mid20(a) { return collect(a, 1, 2, 3, 4, 5, 6, 7, 8, 9, ...arr, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19); }
noInline(mid20);

function range(from, to) {
    var result = [];
    for (var i = from; i <= to; ++i)
        result.push(i);
    return result;
}

var expectedTrailing10 = [0].concat(range(1, 9), arr);
var expectedTrailing20 = [0].concat(range(1, 19), arr);
var expectedLeading20 = arr.concat([0], range(1, 19));
var expectedMid20 = [0].concat(range(1, 9), arr, range(10, 19));

for (var i = 0; i < testLoopCount; ++i) {
    assert(arrEq(trailing10(0), expectedTrailing10), "trailing10");
    assert(arrEq(trailing20(0), expectedTrailing20), "trailing20");
    assert(arrEq(leading20(0), expectedLeading20), "leading20");
    assert(arrEq(mid20(0), expectedMid20), "mid20");
}
