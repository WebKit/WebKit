function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function thrownBy(func)
{
    try {
        func();
    } catch (error) {
        return error;
    }
    throw new Error("did not throw");
}

function Collect()
{
    this.args = Array.prototype.slice.call(arguments);
    this.newTarget = new.target;
}

function Other() { }

const rareStride = Math.max(1, Math.floor(testLoopCount / 100));

let log = [];

function record(name, value)
{
    log.push(name);
    return value;
}

function test(target, newTarget, a)
{
    return Reflect.construct(target, [record("element0", a), record("element1", 1)], newTarget);
}
noInline(test);

function withoutNewTarget(target, a)
{
    return Reflect.construct(target, [record("element", a)]);
}
noInline(withoutNewTarget);

function throwingElement(target, a)
{
    return Reflect.construct(target, [a, (() => { throw new RangeError("element"); })()], record("newTarget", Other));
}
noInline(throwingElement);

const notConstructors = [undefined, null, 1, "string", { }, () => { }, Math.max, async function () { }, function* () { }, { method() { } }.method];

for (let i = 0; i < testLoopCount; ++i) {
    log = [];
    let object = test(Collect, Other, i);
    shouldBe(object.args.join(), i + ",1");
    shouldBe(object.newTarget, Other);
    shouldBe(withoutNewTarget(Collect, i).args[0], i);
    shouldBe(log.join(), "element0,element1,element");

    if (i % rareStride)
        continue;
    for (let notConstructor of notConstructors) {
        log = [];
        let error = thrownBy(() => test(notConstructor, Other, i));
        shouldBe(error instanceof TypeError, true);
        shouldBe(error.message, "Reflect.construct requires the first argument be a constructor");
        error = thrownBy(() => test(Collect, notConstructor, i));
        shouldBe(error instanceof TypeError, true);
        shouldBe(error.message, "Reflect.construct requires the third argument be a constructor if present");
        error = thrownBy(() => withoutNewTarget(notConstructor, i));
        shouldBe(error instanceof TypeError, true);
        shouldBe(error.message, "Reflect.construct requires the first argument be a constructor");
        shouldBe(log.join(), "element0,element1,element0,element1,element");
    }

    log = [];
    let error = thrownBy(() => throwingElement(Collect, i));
    shouldBe(error instanceof RangeError, true);
    error = thrownBy(() => throwingElement(undefined, i));
    shouldBe(error instanceof RangeError, true);
    shouldBe(log.join(), "");
}
