function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected: " + String(expected));
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof TypeError))
        throw new Error("expected TypeError but got: " + String(error));
}

class Identity {
    constructor(object) {
        return object;
    }
}

// Every factory has its own code so that a test does not inherit the inline cache of another test.
// Classes from the same factory share the inline cache while they have different private names.
let factoryCount = 0;
function createFactory() {
    let name = "Stamper" + factoryCount++;
    return new Function("Identity", `
        class ${name} extends Identity {
            #field = 42;

            static has(object) {
                return #field in object;
            }
        }
        noInline(${name}.has);
        return ${name};
    `).bind(null, Identity);
}

function createStamper() {
    return createFactory()();
}

function warmUp(Stamper) {
    let objects = [];
    for (let i = 0; i < 64; ++i) {
        let object = { ["property" + i]: i };
        if (i & 1)
            new Stamper(object);
        objects.push(object);
    }
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(Stamper.has(objects[i & 63]), !!(i & 1));
    return objects;
}

// Objects get the private name after their absence is cached.
{
    let Stamper = createStamper();
    let objects = warmUp(Stamper);
    for (let i = 0; i < 64; i += 2)
        new Stamper(objects[i]);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(Stamper.has(objects[i & 63]), true);
}

// The private name of a prototype is not visible from its children.
{
    let Stamper = createStamper();
    warmUp(Stamper);
    let prototype = { prototypeProperty: 42 };
    let child = Object.create(prototype);
    child.childProperty = 42;
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(Stamper.has(child), false);
        shouldBe(Stamper.has(prototype), false);
    }
    new Stamper(prototype);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(Stamper.has(child), false);
        shouldBe(Stamper.has(prototype), true);
    }
    new Stamper(child);
    Object.setPrototypeOf(child, null);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(Stamper.has(child), true);
        shouldBe(Stamper.has(prototype), true);
    }
}

// Everything is a child of an object which has the private name.
{
    let Stamper = createStamper();
    let prototype = { };
    new Stamper(prototype);
    let objects = [];
    for (let i = 0; i < 64; ++i) {
        let object = Object.create(prototype);
        object["inherited" + i] = i;
        if (i & 1)
            new Stamper(object);
        objects.push(object);
    }
    warmUp(Stamper);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(Stamper.has(objects[i & 63]), !!(i & 1));
}

// Classes have different private names.
{
    let factory = createFactory();
    let stampers = [];
    let objects = [];
    for (let i = 0; i < 16; ++i) {
        let Stamper = factory();
        stampers.push(Stamper);
        let object = { };
        new Stamper(object);
        objects.push(object);
    }
    for (let i = 0; i < testLoopCount; ++i) {
        for (let j = 0; j < 4; ++j)
            shouldBe(stampers[i & 15].has(objects[(i + j) & 15]), !j);
    }
}

// Primitives always throw.
{
    let Stamper = createStamper();
    let objects = warmUp(Stamper);
    let primitives = [undefined, null, true, 42, 42.5, "string", Symbol("symbol"), 42n];
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(Stamper.has(objects[i & 63]), !!(i & 1));
        if (!(i & 0xf))
            shouldThrowTypeError(() => Stamper.has(primitives[(i >> 4) & 7]));
    }
}

function testReceivers(create) {
    let Stamper = createStamper();
    let objects = warmUp(Stamper);
    let receivers = [];
    for (let i = 0; i < create.length * 2; ++i) {
        let object = create[i >> 1]();
        if (i & 1)
            new Stamper(object);
        receivers.push(object);
    }
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(Stamper.has(objects[i & 63]), !!(i & 1));
        let index = i % receivers.length;
        shouldBe(Stamper.has(receivers[index]), !!(index & 1));
    }
    for (let i = 0; i < receivers.length; i += 2)
        new Stamper(receivers[i]);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(Stamper.has(receivers[i % receivers.length]), true);
}

// Receivers which are not plain objects.
testReceivers([
    () => [],
    () => function () { },
    () => new Error("error"),
    () => (function () { return arguments; })(),
    () => new Uint8Array(4),
    () => new String("string"),
    () => /regexp/,
    () => new Map,
    () => Object.create(null),
]);

// Receivers which are not cacheable.
{
    let traps = 0;
    let handler = new Proxy({ }, {
        get(target, key)
        {
            // Adding a private field checks whether the proxy is extensible.
            if (key !== "isExtensible")
                ++traps;
            return undefined;
        }
    });
    testReceivers([
        () => new Proxy({ }, handler),
        () => new Proxy([], handler),
    ]);
    shouldBe(traps, 0);
}

// Receivers with poly proto. They share their structure while only some of their prototypes have the private name.
{
    let Stamper = createStamper();
    let factories = [];
    for (let i = 0; i < 16; ++i) {
        factories.push(new Function(`
            class PolyProto {
                constructor() {
                    this.property${i} = 42;
                }
            }
            return new PolyProto;
        `));
    }

    // This makes the structures poly proto.
    function load(object, index) {
        return object["property" + index];
    }
    noInline(load);
    for (let i = 0; i < 16; ++i) {
        for (let j = 0; j < 100; ++j)
            shouldBe(load(factories[i](), i), 42);
    }

    for (let i = 0; i < testLoopCount; ++i) {
        let object = factories[i & 15]();
        shouldBe(Stamper.has(object), false);
        new Stamper(object);
        shouldBe(Stamper.has(object), true);
    }
    for (let i = 0; i < testLoopCount; ++i) {
        let object = factories[i & 15]();
        new Stamper(Object.getPrototypeOf(object));
        shouldBe(Stamper.has(object), false);
        shouldBe(Stamper.has(Object.getPrototypeOf(object)), true);
        new Stamper(object);
        shouldBe(Stamper.has(object), true);
    }
}
