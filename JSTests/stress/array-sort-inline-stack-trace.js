// Verifies that when a comparator inlined by the DFG ArraySortIntrinsic throws
// after the function has been JIT-compiled, the error stack contains frames for
// `test`, `sort`, and `inlined`.
//
// This exercises frame reconstruction on exception: even though the DFG inlined
// sort's body and the comparator call, the stack unwinder must still report a
// `sort` frame (the generic host function) between `inlined` and `test`.

let counter = 0;
const throwAt = testLoopCount;

function inlined(a, b) {
    counter++;
    if (counter >= throwAt)
        throw new Error("thrown from inlined");
    return a - b;
}

function test() {
    let a = [3, 1, 2, 5, 4];
    a.sort(inlined);
}

let caught = null;
try {
    while (true)
        test();
} catch (e) {
    caught = e;
}

if (!caught)
    throw new Error("no exception was caught");

const stack = caught.stack;

const expected = ["test", "sort", "inlined"];
for (const name of expected) {
    if (!stack.includes(name))
        throw new Error("error.stack is missing '" + name + "' frame:\n" + stack);
}
