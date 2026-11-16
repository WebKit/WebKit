// TLA module driving a pristine async generator via the for-await fast-consumer path with NO
// observable `then` (promise-then watchpoint valid). This is the path where the async generator
// producer reuses its per-generator cached iterator-result object across yields (317587@main),
// with the module record itself as the driver. Reuse is non-observable here, so we can only assert
// that many yields are delivered with correct values/order.
const log = [];

async function* g() {
    for (let i = 0; i < 500; ++i)
        yield i;
}

for await (const x of g())
    log.push(x);

globalThis.__tlaCachedFast = log;
export { };
