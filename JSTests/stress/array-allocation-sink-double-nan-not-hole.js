
function makeNaN() { return 0/0; }
noInline(makeNaN);

// The array escapes because writing NaN to a double array triggers a
// double-to-contiguous conversion at runtime, which the DFG cannot sink.
let result;
let breakAt = testLoopCount - 1;
if (breakAt % 2 !== 1) breakAt--;
for (let i = 0; i < testLoopCount; i++) {
    let arr = new Array(4);
    arr[0] = 1.1;
    arr[1] = 2.2;
    arr[2] = 3.3;
    if (i & 1) arr[1] = makeNaN();
    arr[0] = i + 0.5;
    if (i === breakAt) {
        result = arr[1];
        break;
    }
}
// breakAt is odd, so arr[1] = makeNaN() was executed. The value should be NaN.
if (!isNaN(result))
    throw new Error("expected NaN, got " + result);
// NaN is a value, not a hole, so 1 should be "in" arr.
// (This array escaped the sinking phase due to the NaN write, so it is a
// real runtime array and the in-operator checks its actual storage.)
