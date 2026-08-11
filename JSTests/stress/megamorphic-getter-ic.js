// Exercises the megamorphic getter inline cache (LoadMegamorphicGetter): a GetById site that is
// megamorphic on structure but always resolves to a plain JS getter. Each shape has its own getter
// returning a value derived from a shape-specific constant and the receiver, so a mis-resolved
// holder/offset produces an observably wrong value rather than just a slow path.

function shouldBe(a, b) {
    if (a !== b)
        throw new Error("expected " + b + " got " + a);
}

const shapeCount = 100;

// Build many distinct structures, all exposing a getter named "value": half as an OWN property
// (holder == base), half on a per-shape PROTOTYPE (holder != base). Padding fields vary the offset.
function makeShapes() {
    const shapes = [];
    for (let i = 0; i < shapeCount; i++) {
        const shapeConst = (i + 1) * 7;
        const getter = function () { return this.id * 1000 + shapeConst; };

        if (i % 2 === 0) {
            const o = {};
            for (let p = 0; p < (i % 5); p++)
                o["pad" + i + "_" + p] = p;
            o.id = i;
            Object.defineProperty(o, "value", { get: getter, configurable: true });
            shapes.push({ object: o, expected: i * 1000 + shapeConst });
        } else {
            const proto = {};
            for (let p = 0; p < (i % 5); p++)
                proto["ppad" + i + "_" + p] = p;
            Object.defineProperty(proto, "value", { get: getter, configurable: true });
            const o = Object.create(proto);
            for (let p = 0; p < (i % 3); p++)
                o["opad" + i + "_" + p] = p;
            o.id = i;
            shapes.push({ object: o, expected: i * 1000 + shapeConst });
        }
    }
    return shapes;
}

const shapes = makeShapes();

function access(base) {
    return base.value;
}
noInline(access);

// Warm up across every shape to make the site megamorphic and tier up.
for (let i = 0; i < 200; i++) {
    for (let j = 0; j < shapes.length; j++)
        access(shapes[j].object);
}

// Hammer the IC and verify every shape returns its own getter's value.
for (let i = 0; i < testLoopCount; i++) {
    const shape = shapes[i % shapes.length];
    shouldBe(access(shape.object), shape.expected);
}

// The getter must actually be invoked on every access: a getter with a side effect must observe
// every call. If the IC ever short-circuited to a value load, the counter would be wrong.
{
    let calls = 0;
    const sideProto = {};
    Object.defineProperty(sideProto, "value", { get: function () { calls++; return this.id; }, configurable: true });
    const sideShapes = [];
    for (let i = 0; i < shapeCount; i++) {
        const o = Object.create(sideProto);
        o["s" + i] = i; // distinct structure per shape
        o.id = i;
        sideShapes.push(o);
    }
    function accessSide(base) { return base.value; }
    noInline(accessSide);
    for (let i = 0; i < 200; i++)
        for (let j = 0; j < sideShapes.length; j++)
            accessSide(sideShapes[j]);

    calls = 0;
    const iterations = testLoopCount;
    for (let i = 0; i < iterations; i++) {
        const o = sideShapes[i % sideShapes.length];
        shouldBe(accessSide(o), o.id);
    }
    shouldBe(calls, iterations);
}

// After the site is megamorphic, redefining a getter on a live shape must be reflected (epoch
// invalidation), not served stale from the getter cache.
{
    const proto = {};
    Object.defineProperty(proto, "value", { get: function () { return this.id + 1; }, configurable: true });
    const objects = [];
    for (let i = 0; i < shapeCount; i++) {
        const o = Object.create(proto);
        o["r" + i] = i;
        o.id = i;
        objects.push(o);
    }
    function accessReconf(base) { return base.value; }
    noInline(accessReconf);
    for (let i = 0; i < 200; i++)
        for (let j = 0; j < objects.length; j++)
            accessReconf(objects[j]);
    for (let i = 0; i < testLoopCount; i++) {
        const o = objects[i % objects.length];
        shouldBe(accessReconf(o), o.id + 1);
    }

    // Redefine the shared getter; every subsequent read must use the new getter.
    Object.defineProperty(proto, "value", { get: function () { return this.id + 100000; }, configurable: true });
    for (let i = 0; i < testLoopCount; i++) {
        const o = objects[i % objects.length];
        shouldBe(accessReconf(o), o.id + 100000);
    }
}

// A getter that throws must have its exception propagated on every access through the megamorphic
// getter IC. Each shape's getter throws a shape-specific error object, so a mis-resolved holder/offset
// would surface as the wrong thrown value rather than merely a slow path.
{
    const errors = [];
    const throwShapes = [];
    for (let i = 0; i < shapeCount; i++) {
        const error = new Error("throw" + i);
        errors.push(error);
        const getter = function () { throw error; };

        let object;
        if (i % 2 === 0) {
            object = {};
            for (let p = 0; p < (i % 5); p++)
                object["tpad" + i + "_" + p] = p;
            object.id = i;
            Object.defineProperty(object, "value", { get: getter, configurable: true });
        } else {
            const proto = {};
            for (let p = 0; p < (i % 5); p++)
                proto["tppad" + i + "_" + p] = p;
            Object.defineProperty(proto, "value", { get: getter, configurable: true });
            object = Object.create(proto);
            object["t" + i] = i; // distinct structure per shape
            object.id = i;
        }
        throwShapes.push({ object, error });
    }
    function accessThrow(base) { return base.value; }
    noInline(accessThrow);

    // Warm up across every shape to make the site megamorphic and tier up.
    for (let i = 0; i < 200; i++) {
        for (let j = 0; j < throwShapes.length; j++) {
            try {
                accessThrow(throwShapes[j].object);
                throw new Error("getter did not throw");
            } catch (e) {
                shouldBe(e, throwShapes[j].error);
            }
        }
    }

    // Hammer the IC and verify every shape throws its own getter's error.
    for (let i = 0; i < testLoopCount; i++) {
        const shape = throwShapes[i % throwShapes.length];
        let thrown = null;
        try {
            accessThrow(shape.object);
        } catch (e) {
            thrown = e;
        }
        shouldBe(thrown, shape.error);
    }
}
