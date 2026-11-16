function resolveWithItself(i) {
    let resolve;
    let promise = new Promise((r) => { resolve = r; });
    Object.setPrototypeOf(promise, null);
    promise.foo = i;
    resolve(promise);
    return promise;
}
noInline(resolveWithItself);

let fulfilled = 0;
let rejectedWithTypeError = 0;
for (let i = 0; i < testLoopCount; ++i) {
    Promise.prototype.then.call(resolveWithItself(i), () => { fulfilled++; }, (error) => {
        if (error instanceof TypeError)
            rejectedWithTypeError++;
    });
}
drainMicrotasks();

if (fulfilled || rejectedWithTypeError !== testLoopCount)
    throw new Error("fulfilled " + fulfilled + " times, rejected with TypeError " + rejectedWithTypeError + " times");
