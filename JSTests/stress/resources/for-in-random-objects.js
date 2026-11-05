function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected + ", " + message);
}

function doNotOptimize(func) {
    noDFG(func);
    noInline(func);
    return func;
}

var state = 0x9e3779b9;
function random(limit) {
    state ^= state << 13;
    state ^= state >>> 17;
    state ^= state << 5;
    return (state >>> 0) % limit;
}

var symbols = [Symbol("x"), Symbol("y")];
var proxies = new WeakSet;

class WithPrivateField {
    #field = 1;
}

function Constructor() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}

class ReturnArgument {
    constructor(object) {
        return object;
    }
}

class AddPrivateField extends ReturnArgument {
    #added = 1;
}

function createArguments(x, y) {
    return arguments;
}

function createStrictArguments(x, y) {
    "use strict";
    return arguments;
}

function* generator() {
}

function createWithPolyProto() {
    class Base {
        a = 1;
    }
    class Derived extends Base {
        #field = 2;
        b = 3;
    }
    return new Derived;
}

var ordinaryBases = [
    () => ({}),
    () => ({ a: 1, b: 2 }),
    () => ({ a: 1, [symbols[0]]: 2, b: 3, c: 4 }),
    () => new WithPrivateField,
    () => new Constructor,
    createWithPolyProto,
    () => {
        var object = {};
        for (var i = 0; i < 12; ++i)
            object["p" + i] = i;
        return object;
    },
];

var exoticBases = [
    () => createArguments(1, 2),
    () => createStrictArguments(1, 2),
    () => function (x) { },
    () => (function (x) { }).bind(null),
    () => class { static a = 1; },
    () => new String("ab"),
    () => new Number(1),
    () => new Uint8Array(2),
    () => Array.of(1, 2),
    () => [1.5, , 2.5],
    () => /x/,
    () => new Error("error"),
    () => new Date(0),
    () => new Map,
    () => generator(),
    () => {
        var proxy = new Proxy({ a: 1, b: 2 }, {});
        proxies.add(proxy);
        return proxy;
    },
    () => {
        var object = $vm.createImpureGetter(Object.defineProperties({}, { a: { value: 100, enumerable: true }, b: { value: 101, enumerable: false }, z: { value: 102, enumerable: true } }));
        object[symbols[0]] = 1;
        return object;
    },
    () => $vm.createStaticCustomValue(),
    () => $vm.createStaticCustomAccessor(),
    () => $vm.createStaticDontDeleteDontEnum(),
    () => $vm.createCustomGetterObject(),
    () => $vm.createDOMJITNodeObject(),
];

function createWorld(description) {
    var grandparent = {};
    var parent = Object.create(grandparent);
    var object = description.bases[description.base]();
    if (!("hasOwnProperty" in object))
        Object.setPrototypeOf(object, Object.prototype);
    var world = { log: [], targets: [object, parent, grandparent] };
    world.keys = ["a", "b", "c", "d", "e", "f", ...symbols];
    for (var key of Object.keys(object)) {
        if (key.length > 1)
            world.keys.push(key);
    }
    for (var operation of description.setup)
        apply(world, operation);
    if (description.other) {
        var otherWorld = createWorld(description.other);
        otherWorld.log = world.log;
        world.other = otherWorld.targets[0];
        world.targets.push(world.other);
    }
    if (description.whileGettingNames.length) {
        var pending = description.whileGettingNames.slice();
        var proxy = new Proxy({}, {
            ownKeys: doNotOptimize(function (target) {
                for (var operation of pending.splice(0))
                    apply(world, operation);
                return Reflect.ownKeys(target);
            })
        });
        proxies.add(proxy);
        Reflect.setPrototypeOf(grandparent, proxy);
        Reflect.setPrototypeOf(object, parent);
    }
    return world;
}

function apply(world, operation) {
    var object = world.targets[operation.target];
    var key = operation.indexed ? [0, 1, 2, 100000][operation.key % 4] : world.keys[operation.key % world.keys.length];
    var value = operation.value;
    switch (operation.kind) {
    case "set":
        Reflect.set(object, key, value);
        break;
    case "delete":
        Reflect.deleteProperty(object, key);
        break;
    case "deleteAndAdd":
        if (Reflect.deleteProperty(object, key))
            Reflect.set(object, key, value);
        break;
    case "define":
        Reflect.defineProperty(object, key, [{ enumerable: false }, { enumerable: true }, { writable: false }, { value, enumerable: true, writable: true, configurable: true }][value % 4]);
        break;
    case "accessor":
        Reflect.defineProperty(object, key, {
            get: doNotOptimize(function () {
                world.log.push("get " + String(key));
                if (operation.nested)
                    apply(world, operation.nested);
                return value;
            }),
            set: value % 3 ? doNotOptimize(function (newValue) { world.log.push("set " + String(key) + " " + newValue); }) : undefined,
            enumerable: true,
            configurable: true,
        });
        break;
    case "integrity":
        try {
            [Reflect.preventExtensions, Object.seal, Object.freeze][value % 3](object);
        } catch { }
        break;
    case "privateField":
        try {
            new AddPrivateField(object);
        } catch { }
        break;
    case "prototype":
        Reflect.setPrototypeOf(world.targets[0], [Object.prototype, world.targets[1], world.targets[2]][value % 3]);
        break;
    case "dictionary":
        [$vm.toCacheableDictionary, $vm.toUncacheableDictionary, $vm.flattenDictionaryObject][value % 3](object);
        break;
    }
}

var ownKinds = ["set", "set", "set", "set", "delete", "deleteAndAdd", "define", "define", "accessor", "integrity", "privateField"];
var allKinds = ownKinds.concat(["set", "delete", "define", "prototype", "prototype", "dictionary"]);

function createOperation(anything, nesting, other) {
    var kinds = anything ? allKinds : ownKinds;
    var operation = {
        kind: kinds[random(kinds.length)],
        target: other && !random(3) ? 3 : anything && !random(3) ? 1 + random(2) : 0,
        key: random(64),
        indexed: anything && !random(5),
        value: random(1000),
    };
    if (operation.target === 1 || operation.target === 2)
        operation.indexed = false;
    if (operation.kind === "integrity" && random(4))
        operation.kind = "set";
    if (operation.kind === "accessor" && nesting && !random(3))
        operation.nested = createOperation(anything, false, other);
    return operation;
}

function createDescription(options) {
    var bases = options.anything && random(2) ? exoticBases : ordinaryBases;
    var description = { bases, base: random(bases.length), setup: [], during: [], whileGettingNames: [] };
    for (var i = random(10); i--;)
        description.setup.push(createOperation(options.anything, options.during < Infinity, false));
    if (options.other) {
        if (random(3)) {
            description.other = { ...description, setup: [] };
            for (var operation of description.setup)
                description.other.setup.push({ ...operation, value: operation.value + 12000 });
            if (random(2))
                description.other.setup.push(createOperation(options.anything, false, false));
        } else
            description.other = createDescription({ anything: options.anything });
    }
    if (options.during) {
        for (var i = 0; i < 8; ++i)
            description.during.push(random(options.during) ? { kind: "none", target: 0, key: 0, value: 0 } : createOperation(options.anythingLater, true, options.other));
    }
    if (options.whileGettingNames) {
        for (var i = 1 + random(3); i--;)
            description.whileGettingNames.push(createOperation(options.anythingLater, false, options.other));
    }
    return description;
}

function format(value) {
    return typeof value === "object" || typeof value === "function" || typeof value === "string" ? typeof value : String(value);
}

function describe(world) {
    var result = "";
    for (var object of world.targets) {
        var properties = [];
        for (var key of Reflect.ownKeys(object)) {
            var descriptor = Reflect.getOwnPropertyDescriptor(object, key);
            properties.push(String(key) + ":" + ("value" in descriptor ? format(descriptor.value) + (descriptor.writable ? "w" : "") : "accessor") + (descriptor.enumerable ? "e" : "") + (descriptor.configurable ? "c" : ""));
        }
        result += properties.sort().join() + (Reflect.isExtensible(object) ? "|" : "!");
    }
    return result + world.log.join();
}

function enumerableNames(object) {
    var names = [];
    var seen = new Set;
    for (; object !== null; object = Reflect.getPrototypeOf(object)) {
        for (var key of Reflect.ownKeys(object)) {
            if (typeof key === "symbol" || seen.has(key))
                continue;
            if (!proxies.has(object) && !Reflect.getOwnPropertyDescriptor(object, key).enumerable)
                continue;
            seen.add(key);
            names.push(key);
        }
    }
    return names;
}

function isEnumerable(object, key) {
    for (; object !== null; object = Reflect.getPrototypeOf(object)) {
        var descriptor = Reflect.getOwnPropertyDescriptor(object, key);
        if (descriptor)
            return descriptor.enumerable;
    }
    return false;
}

function expected(world, during, access) {
    var object = world.targets[0];
    var result = "";
    var step = 0;
    for (var key of enumerableNames(object)) {
        if (!isEnumerable(object, key))
            continue;
        if (step < during.length)
            apply(world, during[step]);
        ++step;
        result += key + ":" + access(world.other ?? object, key, step) + ",";
    }
    return result;
}

var accesses = {
    get: (object, key) => format(Reflect.get(object, key)),
    put: (object, key, step) => (Reflect.set(object, key, step), ""),
    putStrict: (object, key, step) => Reflect.set(object, key, step) ? "" : "TypeError",
    in: (object, key) => Reflect.has(object, key),
    hasOwnProperty: (object, key) => Reflect.getOwnPropertyDescriptor(object, key) !== undefined,
};

var accessSources = {
    get: "result += format(base[key]);",
    put: "base[key] = step;",
    putStrict: "try { base[key] = step; } catch (error) { result += error.name; }",
    in: "result += key in base;",
    hasOwnProperty: "result += base.hasOwnProperty(key);",
};

function createVisitor(name, base) {
    return (0, eval)(`(function (world, during) {
        ${name === "putStrict" ? '"use strict";' : ""}
        var object = world.targets[0];
        var other = world.other;
        var result = "";
        var step = 0;
        for (var key in object) {
            if (step < during.length)
                apply(world, during[step]);
            ++step;
            result += key + ":";
            ${accessSources[name].replace("base", base)}
            result += ",";
        }
        return result;
    })`);
}

function test(limit, createOptions) {
    var count = Math.min(testLoopCount, limit);
    var visitors = {};
    for (var name in accessSources) {
        visitors[name] = createVisitor(name, createOptions(0, count).other ? "other" : "object");
        noInline(visitors[name]);
    }
    for (var i = 0; i < count; ++i) {
        var description = createDescription(createOptions(i, count));
        for (var name in visitors) {
            var actualWorld = createWorld(description);
            var expectedWorld = createWorld(description);
            var message = i + " " + name;
            shouldBe(visitors[name](actualWorld, description.during), expected(expectedWorld, description.during, accesses[name]), message);
            shouldBe(describe(actualWorld), describe(expectedWorld), message);
        }
    }
}

[
    shouldBe, random, WithPrivateField, Constructor, ReturnArgument, AddPrivateField, createArguments, createStrictArguments, generator, createWithPolyProto,
    ...ordinaryBases, ...exoticBases, createWorld, apply, createOperation, createDescription, format, describe, enumerableNames, isEnumerable, expected,
    ...Object.values(accesses), createVisitor, test,
].forEach(doNotOptimize);
