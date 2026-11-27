function assert(b, m) { if (!b) throw new Error("bad: " + m); }
function idx(a, s) { return a.indexOf(s); }
function inc(a, s) { return a.includes(s); }
noInline(idx); noInline(inc);
// Returns a resolved string that is neither a rope nor an atom.
function nonAtom(s) { return s.split("").join(""); }
const names = Object.getOwnPropertyNames(Array.prototype).concat(Object.getOwnPropertyNames(String.prototype));
const arr = [];
for (const n of names) arr.push(n);
const emptyContiguous = ["x"];
emptyContiguous.pop();
for (let iter = 0; iter < testLoopCount; ++iter) {
    // atom search, atom elements
    assert(idx(arr, "slice") >= 0 && arr[idx(arr, "slice")] === "slice", 1);
    assert(inc(arr, "slice") === true, 2);
    assert(idx(arr, "notThere" + (iter & 1)) === -1, 3);
    assert(inc(arr, "notThere") === false, 4);
    // non-atom element equal to atom search, placed before the atom
    const arr2 = [1, null, {}, nonAtom("concat"), "concat"];
    assert(idx(arr2, "concat") === 3, 5);
    assert(inc(arr2, "concat") === true, 6);
    // rope element
    const rope = "con" + (iter % 3 === 5 ? "x" : "cat");
    const arr3 = [{}, 2.5, rope, "concat"];
    assert(idx(arr3, "concat") === 2, 7);
    // non-atom search against atom elements
    const joinIndex = idx(arr, "join");
    assert(joinIndex >= 0 && idx(arr, nonAtom("join")) === joinIndex, 8);
    assert(idx(arr, nonAtom("joinx")) === -1, 9);
    assert(inc(arr, nonAtom("join")) === true, 10);
    // 16-bit strings
    const arr5 = [{}, "\u3042\u3044", "\u3046\u3048"];
    assert(idx(arr5, "\u3046\u3048") === 2, 11);
    assert(inc(arr5, "\u3046\u3048") === true, 12);
    assert(idx(arr5, "\u3046\u3049") === -1, 13);
    const arr6 = ["\u3042\u3044", nonAtom("\u3046\u3048"), "\u3046\u3048"];
    assert(idx(arr6, "\u3046\u3048") === 1, 14);
    assert(inc(arr6, "\u3046\u3048") === true, 15);
    assert(idx(arr6, "\u3042\u3044") === 0, 20);
    const arr7 = ["\u3042\u3044", nonAtom("\u3042\u3045"), "\u3046\u3048"];
    assert(idx(arr7, "\u3046\u3048") === 2, 21);
    assert(inc(arr7, "\u3046\u3048") === true, 22);
    assert(idx(arr7, "\u3046\u3049") === -1, 23);
    assert(inc(arr7, "\u3046\u3049") === false, 24);
    assert(arr7.indexOf("\u3042\u3044", 1) === -1, 25);
    assert(arr7.indexOf("\u3046\u3048", 1) === 2, 26);
    // holes and non-string cells (including negative BigInt, which sets TypeInfoPerCellBit)
    const sym = Symbol();
    const arr4 = ["a", , {}, sym, -1n, 1n, "length"];
    assert(idx(arr4, "length") === 6, 16);
    assert(inc(arr4, "nothing") === false, 17);
    assert(idx(emptyContiguous, "x") === -1, 18);
    assert([].indexOf("x") === -1, 29);
    // 8-bit atom search resuming generic loop then hitting slow case on 16-bit / rope element
    const arr8 = ["a", nonAtom("bb"), "\u3042\u3044", "target"];
    assert(idx(arr8, "target") === 3, 27);
    assert(idx(arr8, "ab") === -1, 28);
    // start index
    assert(arr.indexOf("push", idx(arr, "push") + 1) === -1, 19);
}
