// Regression test: op_call_varargs_with_spread must still materialize an eliminable non-spread literal
// argument even when interleaved spreads are eliminable and the call is inlined+statically folded.

function assert(b, msg) {
    if (!b)
        throw new Error("Bad assertion: " + msg);
}
noInline(assert);

// Serialize args: array-likes as "[len]", else the string value.
function shape() {

    var parts = [];
    for (var i = 0; i < arguments.length; ++i) {
        var a = arguments[i];
        if (a && typeof a === "object" && typeof a.length === "number")
            parts.push("[" + a.length + "]");
        else
            parts.push(String(a));
    }
    return parts.join(",");
}
// shape must NOT be inlined so the call lowers to a first-class CallVarargsWithSpread node (whose escape
// analysis has the bug). Callers ARE inlined so the rest spread is statically expandable and folds.
noInline(shape);

function leadConstArray(...rest) { return shape([10, 20, 30], ...rest); }
function hLead() { return leadConstArray(1, 2); }
noInline(hLead);

function trailConstArray(...rest) { return shape(...rest, [4, 5]); }
function hTrail() { return trailConstArray(1, 2, 3); }
noInline(hTrail);

function midConstArray(...rest) { return shape("a", ...rest, [7, 8, 9], "z"); }
function hMid() { return midConstArray(1, 2); }
noInline(hMid);

for (var i = 0; i < testLoopCount; ++i) {
    assert(hLead() === "[3],1,2", "leadConstArray " + hLead());
    assert(hTrail() === "1,2,3,[2]", "trailConstArray " + hTrail());
    assert(hMid() === "a,1,2,[3],z", "midConstArray " + hMid());
}

// `arguments` object as a non-spread literal.
function trailArguments(...rest) { return shape(...rest, arguments); }
function hArgs() { return trailArguments(7, 8, 9); }
noInline(hArgs);

function sloppyTrailArguments() { return shape(...arguments, arguments); }
noInline(sloppyTrailArguments);

for (var i = 0; i < testLoopCount; ++i) {
    assert(hArgs() === "7,8,9,[3]", "trailArguments " + hArgs());
    assert(sloppyTrailArguments(1, 2) === "1,2,[2]", "sloppyTrailArguments " + sloppyTrailArguments(1, 2));
}

// NewArrayWithSpread literal ([...x]) interleaved with a spread.
function nestedSpreadLiteral(x, ...rest) { return shape([...x, 0], ...rest); }
function hNested() { return nestedSpreadLiteral([1, 1], 2, 3); }
noInline(hNested);
for (var i = 0; i < testLoopCount; ++i)
    assert(hNested() === "[3],2,3", "nestedSpreadLiteral " + hNested());

// Un-folded path: spread of a heap array (not statically expandable) with an eliminable literal, at FTL.
var heapArr = [100, 200];
function heapSpreadWithArgsLiteral() { return shape(...heapArr, arguments); }
noInline(heapSpreadWithArgsLiteral);
for (var i = 0; i < testLoopCount; ++i)
    assert(heapSpreadWithArgsLiteral("q") === "100,200,[1]", "heapSpreadWithArgsLiteral " + heapSpreadWithArgsLiteral("q"));
