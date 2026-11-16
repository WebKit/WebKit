// Async functions without await returning a primitive should fulfill the result
// promise inline; proving non-thenable-ness in constant folding also removes the
// world clobber around the promise allocation.

async function compute(x) {
    return x * 2 + 1;
}

function kernel(count) {
    let last;
    for (let i = 0; i < count; i++)
        last = compute(i);
    return last;
}
noInline(kernel);

let last;
for (let r = 0; r < 5; r++)
    last = kernel(2e6);

last.then((value) => {
    if (value !== (2e6 - 1) * 2 + 1)
        throw new Error("bad result");
});
drainMicrotasks();
