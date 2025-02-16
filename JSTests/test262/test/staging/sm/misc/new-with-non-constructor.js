/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
function checkConstruct(thing) {
    try {
        new thing();
        assert.sameValue(0, 1, "not reached " + thing);
    } catch (e) {
        assert.sameValue(e.message.includes(" is not a constructor") ||
                 e.message === "Function.prototype.toString called on incompatible object", true);
    }
}

var re = /aaa/
checkConstruct(re);

var boundFunctionPrototype = Function.prototype.bind();
checkConstruct(boundFunctionPrototype);

var boundBuiltin = Math.sin.bind();
checkConstruct(boundBuiltin);

var proxiedFunctionPrototype = new Proxy(Function.prototype, {});
checkConstruct(proxiedFunctionPrototype);

var proxiedBuiltin = new Proxy(parseInt, {});
checkConstruct(proxiedBuiltin);

