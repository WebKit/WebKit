/*
 * Copyright 2017 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

'use strict';

let testNum = (function() {
    let count = 1;
    return function() {
        return `#${count++} `;
    }
})();

// WPT's assert_throw uses a list of predefined, hardcoded known errors. Since
// it is not aware of the WebAssembly error types (yet), implement our own
// version.
function assertThrows(func, err) {
    let caught = false;
    try {
        func();
    } catch(e) {
        assert_true(e instanceof err, `expected ${err.name}, observed ${e.constructor.name}`);
        caught = true;
    }
    assert_true(caught, testNum() + "assertThrows must catch any error.")
}

/******************************************************************************
***************************** WAST HARNESS ************************************
******************************************************************************/

// For assertions internal to our test harness.
function _assert(x) {
    if (!x) {
        throw new Error(`Assertion failure: ${x}`);
    }
}

// A simple sum type that can either be a valid Value or an Error.
function Result(type, maybeValue) {
    this.value = maybeValue;
    this.type = type;
};

Result.VALUE = 'VALUE';
Result.ERROR = 'ERROR';

function ValueResult(val) { return new Result(Result.VALUE, val); }
function ErrorResult(err) { return new Result(Result.ERROR, err); }

Result.prototype.isError = function() { return this.type === Result.ERROR; }

const EXPECT_INVALID = false;

/* DATA **********************************************************************/

let soft_validate = true;

let $$;

// Default imports.
var registry = {};

// Resets the registry between two different WPT tests.
function reinitializeRegistry() {
    if (typeof WebAssembly === 'undefined')
        return;

    registry = {
        spectest: {
            print: print,
            global: 666,
            table: new WebAssembly.Table({initial: 10, maximum: 20, element: 'anyfunc'}),
            memory: new WebAssembly.Memory({initial: 1, maximum: 2})
        }
    };
}

reinitializeRegistry();

/* WAST POLYFILL *************************************************************/

function binary(bytes) {
    let buffer = new ArrayBuffer(bytes.length);
    let view = new Uint8Array(buffer);
    for (let i = 0; i < bytes.length; ++i) {
        view[i] = bytes.charCodeAt(i);
    }
    return buffer;
}

/**
 * Returns a compiled module, or throws if there was an error at compilation.
 */
function module(bytes, valid = true) {
    let buffer = binary(bytes);
    let validated;

    try {
        validated = WebAssembly.validate(buffer);
    } catch (e) {
        throw new Error(`WebAssembly.validate throws: ${e}${e.stack}`);
    }

    if (validated !== valid) {
        // Try to get a more precise error message from the WebAssembly.CompileError.
        let err = '';
        try { new WebAssembly.Module(buffer) } catch(e) { err = e.toString() }
        throw new Error(`WebAssembly.validate error: ${err}\n`);
    }

    let module;
    try {
        module = new WebAssembly.Module(buffer);
    } catch(e) {
        if (valid)
            throw new Error('WebAssembly.Module ctor unexpectedly throws');
        throw e;
    }

    return module;
}

function uniqueTest(func, desc) {
    test(func, testNum() + desc);
}

function assert_invalid(bytes) {
    uniqueTest(() => {
        try {
            module(bytes, /* valid */ false);
            throw new Error('did not fail');
        } catch(e) {
            assert_true(e instanceof WebAssembly.CompileError, "expected invalid failure:");
        }
    }, "A wast module that should be invalid or malformed.");
}

const assert_malformed = assert_invalid;

function assert_soft_invalid(bytes) {
    uniqueTest(() => {
        try {
            module(bytes, /* valid */ soft_validate);
            if (soft_validate)
                throw new Error('did not fail');
        } catch(e) {
            if (soft_validate)
                assert_true(e instanceof WebAssembly.CompileError, "expected soft invalid failure:");
        }
    }, "A wast module that *could* be invalid under certain engines.");
}

function instance(bytes, imports = registry, valid = true) {
    if (imports instanceof Result) {
        if (imports.isError())
            return imports;
        imports = imports.value;
    }

    let err = null;

    let m, i;
    try {
        let m = module(bytes);
        i = new WebAssembly.Instance(m, imports);
    } catch(e) {
        err = e;
    }

    if (valid) {
        uniqueTest(() => {
            let instantiated = err === null;
            assert_true(instantiated, err);
        }, "module successfully instantiated");
    }

    return err !== null ? ErrorResult(err) : ValueResult(i);
}

function register(name, instance) {
    _assert(instance instanceof Result);

    if (instance.isError())
        return;

    registry[name] = instance.value.exports;
}

function call(instance, name, args) {
    _assert(instance instanceof Result);

    if (instance.isError())
        return instance;

    let err = null;
    let result;
    try {
        result = instance.value.exports[name](...args);
    } catch(e) {
        err = e;
    }

    return err !== null ? ErrorResult(err) : ValueResult(result);
};

function get(instance, name) {
    _assert(instance instanceof Result);

    if (instance.isError())
        return instance;

    return ValueResult(instance.value.exports[name]);
}

function exports(name, instance) {
    _assert(instance instanceof Result);

    if (instance.isError())
        return instance;

    return ValueResult({ [name]: instance.value.exports });
}

function run(action) {
    let result = action();

    _assert(result instanceof Result);

    uniqueTest(() => {
        if (result.isError())
            throw result.value;
    }, "A wast test that runs without any special assertion.");
}

function assert_unlinkable(bytes) {
    let result = instance(bytes, registry, EXPECT_INVALID);

    _assert(result instanceof Result);

    uniqueTest(() => {
        assert_true(result.isError(), 'expected error result');
        if (result.isError()) {
            let e = result.value;
            assert_true(e instanceof WebAssembly.LinkError, `expected link error, observed ${e}:`);
        }
    }, "A wast module that is unlinkable.");
}

function assert_uninstantiable(bytes) {
    let result = instance(bytes, registry, EXPECT_INVALID);

    _assert(result instanceof Result);

    uniqueTest(() => {
        assert_true(result.isError(), 'expected error result');
        if (result.isError()) {
            let e = result.value;
            assert_true(e instanceof WebAssembly.RuntimeError, `expected runtime error, observed ${e}:`);
        }
    }, "A wast module that is uninstantiable.");
}

function assert_trap(action) {
    let result = action();

    _assert(result instanceof Result);

    uniqueTest(() => {
        assert_true(result.isError(), 'expected error result');
        if (result.isError()) {
            let e = result.value;
            assert_true(e instanceof WebAssembly.RuntimeError, `expected runtime error, observed ${e}:`);
        }
    }, "A wast module that must trap at runtime.");
}

let StackOverflow;
try { (function f() { 1 + f() })() } catch (e) { StackOverflow = e.constructor }

function assert_exhaustion(action) {
    let result = action();

    _assert(result instanceof Result);

    uniqueTest(() => {
        assert_true(result.isError(), 'expected error result');
        if (result.isError()) {
            let e = result.value;
            assert_true(e instanceof StackOverflow, `expected stack overflow error, observed ${e}:`);
        }
    }, "A wast module that must exhaust the stack space.");
}

function assert_return(action, expected) {
    if (expected instanceof Result) {
        if (expected.isError())
            return;
        expected = expected.value;
    }

    let result = action();

    _assert(result instanceof Result);

    uniqueTest(() => {
        assert_true(!result.isError(), `expected success result, got: ${result.value}.`);
        if (!result.isError()) {
            assert_equals(result.value, expected);
        };
    }, "A wast module that must return a particular value.");
};

function assert_return_nan(action) {
    let result = action();

    _assert(result instanceof Result);

    uniqueTest(() => {
        assert_true(!result.isError(), 'expected success result');
        if (!result.isError()) {
            assert_true(Number.isNaN(result.value), `expected NaN, observed ${result.value}.`);
        };
    }, "A wast module that must return NaN.");
}
