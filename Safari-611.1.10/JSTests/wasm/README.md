# `wasmjs`: JavaScript tooling for WebAssembly

`wasmjs` is a self-contained collection of JavaScript tools which can create and
manipulate WebAssembly representations and binaries. At its core is `wasm.json`,
a JSON decription of the WebAssembly format and other interesting facts about
WebAssembly as used by the Webkit project (such as the names of associated
JavaScriptCore B3 opcodes).

`wasmjs` requires modern JavaScript features such as ES6 modules, which is
acceptable because WebAssembly is itself contemporary to these other features.


# `Builder` API

The current core API of `wasmjs` is the `Builder` API from `Builder.js`. It is
used to build WebAssembly modules, and assemble them to valid `.wasm` binaries
(held in an `ArrayBuffer`). The `Builder` is a small DSL which looks similar to
the WebAssembly [binary format][]. Each section is declared through a property
on the `Builder` object, and each declaration returns a proxy object which has
properties specific to that section. Proxies are "popped" back by invoking their
`.End()` property.

The `Code` section has properties for each [WebAssembly opcode][]. Opcode which
create a scope can be nested using a lambda.

  [binary format]: https://github.com/WebAssembly/design/blob/master/BinaryEncoding.md#high-level-structure
  [WebAssembly opcode]: https://github.com/WebAssembly/design/blob/master/Semantics.md

A simple example:

```javascript
import Builder from './Builder.js';

const builder = new Builder();

// Construct the equivalent of: (module (func "answer" (i32.const 42) (return)))
builder
    // Declare a Type section, which the builder will auto-fill as functions are defined.
    .Type().End()
    // Declare a Function section, which the builder will auto-fill as functions are defined.
    .Function().End()
    .Export()
        // Export the "answer" function (defined below) to JavaScript.
        .Function("answer")
    .End()
    .Code()
        // The "answer" function takes an i32 parameters, and returns an i32.
        .Function("answer", { params: ["i32"], ret: "i32" })
            // Create a block returning an i32, whose body is the enclosed lambda.
            .Block("i32", b => b
                .GetLocal(0) // Parameters are in the same index space as locals.
                .I32Const(0) // Generate an i32 constant.
                .I32Eq()     // A comparison, using the two values currently on the stack.
                .If("i32")   // Consume the comparison result, returning an i32.
                    .I32Const(42)
                .Else()
                    .I32Const(1)
                .End()
            )
            .Return() // Return the top of the stack: the value returned by the block.
        .End() // End the current function.
    .End(); // End the Code section.

// Create an ArrayBuffer which is a valid WebAssembly `.wasm` file.
const bin = builder.WebAssembly().get();

// Use the standard WebAssembly JavaScript API to compile the module, and instantiate it.
const module = new WebAssembly.Module(bin);
const instance = new WebAssembly.Instance(module);

// Invoke the compiled WebAssembly function.
const result0 = instance.exports.answer(0);
if (result0 !== 42)
    throw new Error(`Expected 42, got ${result0}.`);

const result1 = instance.exports.answer(1);
if (result1 !== 1)
    throw new Error(`Expected 1, got ${result1}.`);
```


# Testing

* `self-test` tests `wasmjs` itself.
* `js-api` tests the [WebAssembly JavaScript API](https://github.com/WebAssembly/design/blob/master/JS.md).
* `function-tests` tests the WebAssembly compiler's implementation.

All tests can be executed using:

```bash
JSSHELL=/path/to/my/js-shell test.sh
```

They can also be executed by using WebKit's `run-javascriptcore-tests` tool:

```bash
./Tools/Scripts/run-javascriptcore-tests --release --filter wasm -arch x86_64
```
