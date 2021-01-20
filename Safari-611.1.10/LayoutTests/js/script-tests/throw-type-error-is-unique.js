"use strict";

description('Tests ES6 %ThrowTypeError% intrinsic is unique');

class ThrowTypeErrorSource {
    constructor(context, base, names)
    {
        this.context = context;
        this.base = base;
        this.names = names;
    }

    checkTypeErrorFunctions(throwTypeErrorFunction)
    {
        let errors = 0;
        for (let name of this.names) {
            let desc = Object.getOwnPropertyDescriptor(this.base, name);

            if (!desc)
                return 0;

            for (let accessorType of ["get", "set"]) {
                let accessor = desc[accessorType];
                if (accessor && accessor !== throwTypeErrorFunction) {
                    testFailed(this.context + " " + accessorType + "ter for \"" + name + "\" is not the same %ThrowTypeError% intrinsic");
                    errors++;
                }
            }
        }

        return errors;
    }
}

class A { };
let arrayProtoPush = Array.prototype.push;

function strictArguments()
{
    return arguments;
}

let sloppyArguments = Function("return arguments;");

function test()
{
    let baseThrowTypeErrorFunction = Object.getOwnPropertyDescriptor(arguments, "callee").get;

    let sources = [
        new ThrowTypeErrorSource("Function.prototype", Function.prototype, ["arguments", "caller"]),
        new ThrowTypeErrorSource("Array.prototype.push (builtin)", arrayProtoPush, ["arguments", "caller"]),
        new ThrowTypeErrorSource("Strict function arguments", strictArguments, ["arguments", "caller"]),
        new ThrowTypeErrorSource("Sloppy function arguments", sloppyArguments, ["arguments", "caller"]),
        new ThrowTypeErrorSource("Strict arguments", strictArguments(), ["callee", "caller"]),
        new ThrowTypeErrorSource("Sloppy arguments", sloppyArguments(), ["callee", "caller"]),
        new ThrowTypeErrorSource("Class constructor", (new A()).constructor, ["arguments", "caller"])
    ];

    let errors = 0;

    for (let source of sources)
        errors += source.checkTypeErrorFunctions(baseThrowTypeErrorFunction);

    if (!errors)
        testPassed("%ThrowTypeError% intrinsic is unique");
}

test();
