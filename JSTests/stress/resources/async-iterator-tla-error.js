// TLA module: a producer that throws mid-iteration must reject this module's top-level capability
// (the rejection is routed through asyncGeneratorDriverResume -> asyncModuleExecutionResume ->
// capability->rejectWithCaughtException on the fast path).
const collected = [];
async function* g() { yield 1; throw new Error("boom"); }
for await (const x of g())
    collected.push(x);
export { collected }; // unreachable: the throw aborts module evaluation
