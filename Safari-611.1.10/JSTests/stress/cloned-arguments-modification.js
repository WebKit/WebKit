function foo() {
    "use strict";

    if (arguments[Symbol.iterator] !== Array.prototype.values)
        throw "Symbol.iterator is wrong";

    arguments[Symbol.iterator] = 1;

    if (arguments[Symbol.iterator] !== 1)
        throw "Symbol.iterator did not update";

    let failed = true;
    try {
        arguments.callee;
    } catch (e) {
        failed = false;
    }
    if (failed)
        throw "one property stopped another from showing up";

    delete arguments[Symbol.iterator];

    if (Symbol.iterator in arguments)
        throw "Symbol.iterator did not get deleted";

    failed = true;
    try {
        arguments.callee;
    } catch (e) {
        failed = false;
    }
    if (failed)
        throw "one property stopped another from showing up";
}

for (i = 0; i < 10000; i++)
    foo();
