function shouldThrow(func, errorConstructor)
{
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorConstructor))
        throw new Error(`bad error: ${String(error)}`);
}

function count()
{
    return arguments.length;
}

function makeArguments()
{
    "use strict";
    arguments.length = 2 ** 32 + 1;
    return arguments;
}

function apply(args)
{
    return count.apply(null, args);
}
noInline(apply);

function reflectApply(args)
{
    return Reflect.apply(count, null, args);
}
noInline(reflectApply);

for (let i = 0; i < testLoopCount; ++i) {
    if (apply([1, 2]) !== 2 || reflectApply([1, 2]) !== 2)
        throw new Error("bad count");
}
for (let i = 0; i < 10; ++i) {
    shouldThrow(() => apply(makeArguments(1, 2, 3)), RangeError);
    shouldThrow(() => reflectApply(makeArguments(1, 2, 3)), RangeError);
}
