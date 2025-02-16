// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Tests that newer-type functions (i.e. anything not defined by regular function declarations and
// expressions) throw when accessing their 'arguments' and 'caller' properties.

// 9.2.7 (AddRestrictedFunctionProperties) defines accessors on Function.prototype which throw on
// every 'get' and 'set' of 'caller' and 'arguments'.
// Additionally, 16.2 (Forbidden Extensions) forbids adding properties to all non-"legacy" function
// declarations and expressions. This creates the effect that all newer-style functions act like
// strict-mode functions when accessing their 'caller' and 'arguments' properties.

const container = {
    async asyncMethod() {},
    *genMethod() {},
    method() {},
    get getterMethod() {},
    set setterMethod(x) {},
};

function* genDecl(){}
async function asyncDecl(){}
class classDecl {}
class extClassDecl extends Object {}
class classDeclExplicitCtor { constructor(){} }
class extClassDeclExplicitCtor extends Object { constructor(){} }

const functions = [
    // Declarations
    genDecl,
    asyncDecl,
    classDecl,
    extClassDecl,
    classDeclExplicitCtor,
    extClassDeclExplicitCtor,

    // Expressions
    async function(){},
    function*(){},
    () => {},
    async () => {},
    class {},
    class extends Object {},
    class { constructor(){} },
    class extends Object { constructor(){} },

    // Methods
    container.asyncMethod,
    container.genMethod,
    container.method,
    Object.getOwnPropertyDescriptor(container, "getterMethod").get,
    Object.getOwnPropertyDescriptor(container, "setterMethod").set,

    // Bound functions
    function(){}.bind(),

    // Built-ins (native and self-hosted)
    Function,
    Function.prototype.bind,
];

const supportsAsyncGenerator = (function() {
    try {
        eval("async function* f(){}");
        return true;
    } catch (e) {
        return false;
    }
})();

if (supportsAsyncGenerator) {
eval(`
    async function* asyncGenDecl(){}

    functions.push(asyncGenDecl);
    functions.push(async function*(){});
    functions.push({async* asyncGenerator(){}}.asyncGenerator);
`);
}

functions.forEach(f => {
    checkArgumentsAccess(f);
    checkCallerAccess(f);
});

function checkArgumentsAccess(f) {
    assertThrowsInstanceOf(() => f.arguments, TypeError,
                           `Expected 'arguments' property access to throw on ${f}`);
}

function checkCallerAccess(f) {
    assertThrowsInstanceOf(() => f.caller, TypeError,
                           `Expected 'caller' property access to throw on ${f}`);
}

