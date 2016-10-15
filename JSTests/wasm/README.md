# `wasmjs`: JavaScript tooling for WebAssembly

`wasmjs` is a self-contained collection of JavaScript tools which can manipulate
WebAssembly. At its core is `wasm.json`, a JSON decription of the WebAssembly
format and other interesting facts about WebAssembly as used by the Webkit
project (such as the names of associated JavaScriptCore opcodes).

`wasmjs` requires modern JavaScript features such as ES6 modules, which is
acceptable because WebAssembly is itself contemporary to these other features.


# `Builder` API

The current core API of `wasmjs` is the `Builder` API from `Builder.js`. It is
used to build WebAssembly modules.

A simple example:

```javascript
import Builder from 'Builder.js';

const builder = new Builder();

// Construct the equivalent of: (module (func (nop) (nop)))
builder
    .Code()
        .Function()
            .Nop()
            .Nop()
        .End()
    .End();

// Create an ArrayBuffer which is a valid WebAssembly `.wasm` file.
const binary = builder.WebAssembly();
```

Code such as the above can then be used with JavaScript's `WebAssembly` global
object.


# Testing

Tests can be executed using:

```bash
JSSHELL=/path/to/my/js-shell test.sh
```

They can also be executed by using WebKit's `run-javascriptcore-tests` tool:

```bash
./Tools/Scripts/run-javascriptcore-tests --release --filter wasm -arch x86_64
```

The `self-test` folder contains tests for `wasmjs` itself. Future additions will
also test the JavaScript engine's WebAssembly implementation (both JavaScript
API and usage of that API to compile and execute WebAssembly modules).
