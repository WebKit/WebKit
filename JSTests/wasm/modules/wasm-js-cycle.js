import * as assert from '../assert.js'
import { return42 } from "./wasm-js-cycle/entry.wasm"

assert.eq(return42(), 42);

import { getFromJSGlobal, g } from "./wasm-js-cycle/entry-global.wasm"
import { globalFromJS, incrementGlobal } from "./wasm-js-cycle/global.js"

assert.instanceof(g, WebAssembly.Global);
assert.eq(g.valueOf(), 42);
incrementGlobal();
assert.eq(g.valueOf(), 43);

assert.isFunction(getFromJSGlobal);
assert.eq(getFromJSGlobal(), globalFromJS.valueOf());
globalFromJS.value = 84;
assert.eq(getFromJSGlobal(), globalFromJS.valueOf());

import { getFromJSTable, t } from "./wasm-js-cycle/entry-table.wasm"
import { tableFromJS, setTable } from "./wasm-js-cycle/table.js"

assert.instanceof(t, WebAssembly.Table);
assert.eq(t.get(0), null);
setTable(0, "foo");
assert.eq(t.get(0), "foo");

assert.isFunction(getFromJSTable);
assert.eq(getFromJSTable(), tableFromJS.get(0));
tableFromJS.set(0, "foo");
assert.eq(getFromJSTable(), tableFromJS.get(0));

import { m } from "./wasm-js-cycle/entry-memory.wasm"
import { setMemory } from "./wasm-js-cycle/memory.js"

assert.instanceof(m, WebAssembly.Memory);
const view = new Int32Array(m.buffer);
assert.eq(view[0], 0);
setMemory(0, 42);
assert.eq(view[0], 42);
