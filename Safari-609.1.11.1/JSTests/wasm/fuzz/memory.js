import * as assert from '../assert.js';
import Builder from '../Builder.js';

// FIXME: add the following: https://bugs.webkit.org/show_bug.cgi?id=171936
//  - add set() and shadow memory, this requires tracking when memory is shared
//  - Support: empty, exported
//  - Imported memory created through the JS API (both before and after instantiation, to cause recompilation)
//  - recursive calls (randomly call other instance's exports, potentially exhausting stack)
//  - Simplify code by allowing .Code().ExportFunction(...) in builder

const tune = {
    numRandomIterations: 64,
    numRandomInvokes: 4,
    maxInitial: 8,
    maxMax: 8,
    maxGrow: 8,
    memoryGetCount: 1024,
};

const pageSize = 64 * 1024; // As determined by the WebAssembly specification.

let logString = "";
let log = what => logString += '\n' + what;

let instances = [];

const insert = (builder, importObject = undefined) => {
    const location = (Math.random() * instances.length) | 0;
    instances.splice(location, 0, new WebAssembly.Instance(new WebAssembly.Module(builder.WebAssembly().get()), importObject));
}
const initialMaxObject = hasMax => {
    const initial = (Math.random() * tune.maxInitial) | 0;
    if (!hasMax)
        return { initial: initial, max: undefined };
    const max = initial + ((Math.random() * tune.maxMax) | 0);
    return { initial: initial, max: max };
};
const initialMaxObjectFrom = instance => {
    const hasMax = instance.exports["max"] !== undefined;
    const initial = (Math.random() * instance.exports.initial()) | 0;
    if (!hasMax)
        return { initial: initial };
    const max = Math.max(initial, instance.exports.max()) + ((Math.random() * tune.maxMax) | 0);
    return { initial: initial, max: max };
};

const action = {
    '- delete': () => {
        for (let count = 1 + (Math.random() * instances.length) | 0; instances.length && count; --count) {
            const which = (Math.random() * instances.length) | 0;
            log(`    delete ${which} / ${instances.length}`);
            instances.splice(which, 1);
        }
    },
    '^ invoke': () => {
        if (!instances.length) {
            log(`    nothing to invoke`);
            return;
        }
        for (let iterations = 0; iterations < tune.numRandomInvokes; ++iterations) {
            const whichInstance = (Math.random() * instances.length) | 0;
            const instance = instances[whichInstance];
            const whichExport = (Math.random() * Object.keys(instance.exports).length) | 0;
            log(`    instances[${whichInstance}].${Object.keys(instance.exports)[whichExport]}()`);
            switch (Object.keys(instance.exports)[whichExport]) {
            default:
                throw new Error(`Implementation problem for key '${Object.keys(instance.exports)[whichExport]}'`);
            case 'func':
                assert.eq(instance.exports.func(), 42);
                break;
            case 'throws': {
                let threw = false;
                let address = 0;
                if (instance.exports["current"]) {
                    // The instance has a memory.
                    const current = instance.exports.current();
                    const max = instance.exports["max"] ? instance.exports.max() : (1 << 15);
                    address = pageSize * (current + ((Math.random() * (max - current)) | 0));
                }
                // No memory in this instance.
                try { instance.exports.throws(address); }
                catch (e) { threw = true; }
                assert.truthy(threw);
            } break;
            case 'get': {
                const current = instance.exports.current();
                for (let access = tune.memoryGetCount; current && access; --access) {
                    const address = (Math.random() * (current * pageSize - 4)) | 0;
                    assert.eq(instance.exports.get(address), 0);
                }
            } break;
            case 'current':
                assert.le(instance.exports.initial(), instance.exports.current());
                break;
            case 'initial':
                assert.le(0, instance.exports.initial());
                break;
            case 'max':
                assert.le(instance.exports.initial(), instance.exports.max());
                assert.le(instance.exports.current(), instance.exports.max());
                break;
            case 'memory': {
                const current = instance.exports.current();
                const buffer = new Uint32Array(instance.exports.memory.buffer);
                assert.eq(buffer.byteLength, current * pageSize);
                for (let access = tune.memoryGetCount; current && access; --access) {
                    const address = (Math.random() * (current * pageSize - 4)) | 0;
                    assert.eq(instance.exports.get(address), 0);
                }
            } break;
            case 'grow': {
                const current = instance.exports.current();
                const by = (Math.random() * tune.maxGrow) | 0;
                const hasMax = instance.exports["max"] !== undefined;
                const result = instance.exports.grow(current + by);
                log(`        Grow from ${current} (max ${hasMax ? instance.exports.max() : "none"}) to ${current + by} returned ${result}, current now ${instance.exports.current()}`);
                assert.le(current, instance.exports.current());
                if (result === -1)
                    assert.eq(current, instance.exports.current());
                if (hasMax)
                    assert.le(instance.exports.current(), instance.exports.max());
            } break;
            }
        }
    },
    '+ memory: none': () => {
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Export().Function("func").Function("throws").End()
              .Code()
                  .Function("func", { params: [], ret: "i32" }).I32Const(42).Return().End()
                  .Function("throws", { params: ["i32"] }).Unreachable().Return().End()
              .End();
        insert(builder);
    },
    '+ memory: empty, section': () => {
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Memory().InitialMaxPages(0, 0).End()
              .Export().Function("func").Function("throws").Function("current").Function("initial").Function("grow").Function("max").End()
              .Code()
                  .Function("func", { params: [], ret: "i32" }).I32Const(42).Return().End()
                  .Function("throws", { params: ["i32"] }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
                  .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
                  .Function("initial", { params: [], ret: "i32" }).I32Const(0).Return().End()
                  .Function("max", { params: [], ret: "i32" }).I32Const(0).Return().End()
              .End();
        insert(builder);
    },
    '+ memory: section': () => {
        const initialMax = initialMaxObject(false);
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Memory().InitialMaxPages(initialMax.initial, initialMax.max).End()
              .Export().Function("func").Function("throws").Function("get").Function("current").Function("initial").Function("grow").End()
              .Code()
                  .Function("func", { params: [], ret: "i32" }).I32Const(42).Return().End()
                  .Function("throws", { params: ["i32"] }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("get", { params: ["i32"], ret: "i32" }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
                  .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
                  .Function("initial", { params: [], ret: "i32" }).I32Const(initialMax.initial).Return().End()
              .End();
        insert(builder);
    },
    '+ memory: section, max': () => {
        const initialMax = initialMaxObject(true);
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Memory().InitialMaxPages(initialMax.initial, initialMax.max).End()
              .Export().Function("func").Function("throws").Function("get").Function("current").Function("initial").Function("grow").Function("max").End()
              .Code()
                  .Function("func", { params: [], ret: "i32" }).I32Const(42).Return().End()
                  .Function("throws", { params: ["i32"] }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("get", { params: ["i32"], ret: "i32" }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
                  .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
                  .Function("initial", { params: [], ret: "i32" }).I32Const(initialMax.initial).Return().End()
                  .Function("max", { params: [], ret: "i32" }).I32Const(initialMax.max).Return().End()
              .End();
        insert(builder);
    },
    '+ memory: section, exported': () => {
        const initialMax = initialMaxObject(false);
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Memory().InitialMaxPages(initialMax.initial, initialMax.max).End()
              .Export()
                  .Function("func").Function("throws").Function("get").Function("current").Function("initial")
                  .Memory("memory", 0)
              .End()
              .Code()
                  .Function("func", { params: [], ret: "i32" }).I32Const(42).Return().End()
                  .Function("throws", { params: ["i32"] }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("get", { params: ["i32"], ret: "i32" }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
                  .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
                  .Function("initial", { params: [], ret: "i32" }).I32Const(initialMax.initial).Return().End()
              .End();
        insert(builder);
    },
    '+ memory: section, exported, max': () => {
        const initialMax = initialMaxObject(true);
        const builder = (new Builder())
              .Type().End()
              .Function().End()
              .Memory().InitialMaxPages(initialMax.initial, initialMax.max).End()
              .Export()
                  .Function("func").Function("throws").Function("get").Function("current").Function("initial").Function("grow").Function("max")
                  .Memory("memory", 0)
              .End()
              .Code()
                  .Function("func", { params: [], ret: "i32" }).I32Const(42).Return().End()
                  .Function("throws", { params: ["i32"] }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("get", { params: ["i32"], ret: "i32" }).GetLocal(0).I32Load(2, 0).Return().End()
                  .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
                  .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
                  .Function("initial", { params: [], ret: "i32" }).I32Const(initialMax.initial).Return().End()
                  .Function("max", { params: [], ret: "i32" }).I32Const(initialMax.max).Return().End()
              .End();
        insert(builder);
    },
    '+ memory: imported, exported': () => {
        let importFrom;
        for (let idx = 0; idx < instances.length; ++idx)
            if (instances[idx].exports.memory) {
                importFrom = instances[idx];
                break;
            }
        if (!importFrom)
            return; // No memory could be imported.
        const initialMax = initialMaxObjectFrom(importFrom);
        if (importFrom.exports["max"] === undefined) {
            const builder = (new Builder())
                  .Type().End()
                  .Import().Memory("imp", "memory", initialMax).End()
                  .Function().End()
                  .Export()
                      .Function("func").Function("throws").Function("get").Function("current").Function("initial")
                      .Memory("memory", 0)
                  .End()
                  .Code()
                      .Function("func", { params: [], ret: "i32" }).I32Const(42).Return().End()
                      .Function("throws", { params: ["i32"] }).GetLocal(0).I32Load(2, 0).Return().End()
                      .Function("get", { params: ["i32"], ret: "i32" }).GetLocal(0).I32Load(2, 0).Return().End()
                      .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
                      .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
                      .Function("initial", { params: [], ret: "i32" }).I32Const(initialMax.initial).Return().End()
                  .End();
            insert(builder, { imp: { memory: importFrom.exports.memory } });
        } else {
            const builder = (new Builder())
                  .Type().End()
                  .Import().Memory("imp", "memory", initialMax).End()
                  .Function().End()
                  .Export()
                      .Function("func").Function("throws").Function("get").Function("current").Function("initial").Function("grow").Function("max")
                      .Memory("memory", 0)
                  .End()
                  .Code()
                      .Function("func", { params: [], ret: "i32" }).I32Const(42).Return().End()
                      .Function("throws", { params: ["i32"] }).GetLocal(0).I32Load(2, 0).Return().End()
                      .Function("get", { params: ["i32"], ret: "i32" }).GetLocal(0).I32Load(2, 0).Return().End()
                      .Function("current", { params: [], ret: "i32" }).CurrentMemory(0).Return().End()
                      .Function("grow", { params: ["i32"], ret: "i32" }).GetLocal(0).GrowMemory(0).Return().End()
                      .Function("initial", { params: [], ret: "i32" }).I32Const(initialMax.initial).Return().End()
                      .Function("max", { params: [], ret: "i32" }).I32Const(initialMax.max).Return().End()
                  .End();
            insert(builder, { imp: { memory: importFrom.exports.memory } });
        }
    },
};
const numActions = Object.keys(action).length;
const performAction = () => {
    const which = (Math.random() * numActions) | 0;
    const key = Object.keys(action)[which];
    log(`${key}:`);
    action[key]();
};

try {
    for (let iteration = 0; iteration < tune.numRandomIterations; ++iteration)
        performAction();
    log(`Finalizing:`);
    // Finish by deleting the instances in a random order.
    while (instances.length)
        action["- delete"]();
} catch (e) {
    // Ignore OOM while fuzzing. It can just happen.
    if (e.message !== "Out of memory") {
        print(`Failed: ${e}`);
        print(logString);
        throw e;
    }
}
