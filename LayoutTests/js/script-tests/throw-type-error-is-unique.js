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
        new ThrowTypeErrorSource("Strict arguments", strictArguments(), ["callee"]),
        new ThrowTypeErrorSource("Sloppy arguments", sloppyArguments(), ["callee"]),
    ];

    let errors = 0;

    for (let source of sources)
        errors += source.checkTypeErrorFunctions(baseThrowTypeErrorFunction);

    if (!errors)
        testPassed("%ThrowTypeError% intrinsic is unique");
}

test();
