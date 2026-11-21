//@ requireOptions("--useConcurrentJIT=0")

// A default value on the prototype becomes a constant case of MultiGetByOffset. ValueRepReduction must not
// replace a non-number constant with a double, because the DoubleRep it folds has to OSR exit on that value.

let nextId = 0;
function test(body, defaultValue, expected) {
    // Make the source unique so that the tests do not share profiles.
    const run = new Function("array", "outs", `
        let result = 0;
        let sum = 0;
        for (let i = 0; i < array.length; i++) {
            const value = array[i].x;
            ${body}
        }
        return result; // ${nextId++}
    `);

    const proto = { x: defaultValue };
    function create(x) {
        const object = Object.create(proto);
        if (x !== undefined)
            object.x = x;
        return object;
    }

    const many = [];
    for (let i = 0; i < 1000; i++)
        many.push(create(i + 1.5));
    const few = many.slice(0, 100);
    const odd = few.slice();
    odd[50] = create();
    const outs = [{ y: 0.5 }, { z: 0.5, y: 0.5 }];

    // The Baseline inline cache sees the default value once, but the value profile keeps seeing only numbers.
    for (let i = 0; i < 60; i++)
        run(few, outs);
    run(odd, outs);
    for (let i = 0; i < testLoopCount / 100; i++)
        run(many, outs);

    const actual = run(odd, outs);
    if (!Object.is(actual, expected))
        throw new Error(body + " with " + String(defaultValue) + ": expected " + String(expected) + " but got " + String(actual));
}

test("if (value == 0) result++;", null, 0);
test("if (value === 0) result++;", null, 0);
test("if (value === 0) result++;", false, 0);
test("if (value === 1) result++;", true, 0);
test("if (value !== value) result++;", undefined, 0);

for (const value of [null, undefined, false, true, 42.5, -0]) {
    test("if (i === 50) result = value; sum += value * 0.5;", value, value);
    test("outs[i & 1].y = value; sum += value * 0.5; if (i === 50) result = outs[0].y;", value, value);
    test("const phi = (i & 1) ? 0.5 : value; outs[0].y = phi; sum += phi * 0.5; if (i === 50) result = outs[0].y;", value, value);
}
