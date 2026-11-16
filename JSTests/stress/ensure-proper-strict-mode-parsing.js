
function foo() {
    "hello world i'm not use strict.";
    function bar() {
        return 25;
    }
    bar();
    "use strict";
    return this;
}
if (foo.call(undefined) !== this)
    throw new Error("Bad parsing strict mode.");

function bar() {
    "hello world i'm not use strict.";
    function foo() {
        return this;
    }
    "use strict";
    return foo.call(undefined);
}
if (bar.call(undefined) !== this)
    throw new Error("Bad parsing strict mode.")

function baz() {
    "foo";
    "bar";
    "baz";
    "foo";
    "bar";
    "baz";
    "foo";
    "bar";
    "baz";
    "use strict";
    return this;
}
if (baz.call(undefined) !== undefined)
    throw new Error("Bad parsing strict mode.")

function jaz() {
    "foo";
    `bar`;
    "use strict";
    return this;
}
if (jaz.call(undefined) !== this)
    throw new Error("Bad parsing strict mode.")

function vaz() {
    "foo";
    "use strict";
    `bar`;
    return this;
}
if (vaz.call(undefined) !== undefined)
    throw new Error("Bad parsing strict mode.")

function hello() {
    "foo";
    2 + 2
    "use strict";
    return this;
}
if (hello.call(undefined) !== this)
    throw new Error("Bad parsing strict mode.")

function world() {
    "foo";
    let x;
    "use strict";
    return this;
}
if (world.call(undefined) !== this)
    throw new Error("Bad parsing strict mode.")

function a() {
    "foo";
    world;
    "use strict";
    return this;
}
if (a.call(undefined) !== this)
    throw new Error("Bad parsing strict mode.")

if (eval("'foo'; 'use strict'; 'bar';") !== "bar")
    throw new Error("Bad parsing strict mode.");

if (eval("'foo'; 'use strict'; 'bar'; this;") !== this)
    throw new Error("Bad parsing strict mode.");
