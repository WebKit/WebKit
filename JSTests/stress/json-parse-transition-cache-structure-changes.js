function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function structureID(object) {
    let list = $vm.getStructureTransitionList(object);
    return list[list.length - 5];
}

// Several different first keys give the empty object's Structure several transitions, which is the case
// where JSON.parse consults the transition cache.
const firstKeys = ["alpha", "beta", "gamma", "delta"];

function makeSource(count) {
    let parts = [];
    for (let i = 0; i < count; ++i)
        parts.push(`{"${firstKeys[i % firstKeys.length]}":${i},"x":${i},"y":"${i}"}`);
    return `[${parts.join(",")}]`;
}
const source = makeSource(64);

// Parsed right after a full GC, which empties the cache, so these Structures come from the transition
// tables. They stay alive, so their Structures do too.
fullGC();
const references = new Map;
for (let key of firstKeys)
    references.set(key, JSON.parse(`{"${key}":0,"x":0,"y":"0"}`));

function checkObject(object, i, global = globalThis) {
    let first = firstKeys[i % firstKeys.length];
    shouldBe(JSON.stringify(Object.keys(object)), JSON.stringify([first, "x", "y"]));
    shouldBe(object[first], i);
    shouldBe(object.x, i);
    shouldBe(object.y, String(i));
    for (let key of [first, "x", "y"]) {
        let descriptor = Object.getOwnPropertyDescriptor(object, key);
        shouldBe(descriptor.writable, true);
        shouldBe(descriptor.enumerable, true);
        shouldBe(descriptor.configurable, true);
    }
    shouldBe(Object.isExtensible(object), true);
    shouldBe(Object.getPrototypeOf(object), global.Object.prototype);
    if (global === globalThis)
        shouldBe(structureID(object), structureID(references.get(first)));
}

function parseAndCheck() {
    let result = JSON.parse(source);
    shouldBe(result.length, 64);
    for (let i = 0; i < result.length; ++i)
        checkObject(result[i], i);
    return result;
}

// The objects handed to each step share their Structures with everything parsed afterwards.
function step(mutate) {
    let victims = parseAndCheck();
    mutate(victims);
    parseAndCheck();
    // The mutated objects themselves must keep working.
    for (let object of victims) {
        if (!Object.isExtensible(object))
            continue;
        object.added = 1;
        shouldBe(object.added, 1);
    }
    parseAndCheck();
}

step(() => { });

step(victims => {
    $vm.toCacheableDictionary(victims[0]);
    $vm.toUncacheableDictionary(victims[1]);
    delete victims[2].x;
    delete victims[3].alpha;
    $vm.toUncacheableDictionary(victims[4]);
    victims[4].z = 1;
    delete victims[4].y;
    $vm.flattenDictionaryObject(victims[4]);
    $vm.toCacheableDictionary(victims[5]);
    $vm.flattenDictionaryObject(victims[5]);
});

step(victims => {
    Object.defineProperty(victims[0], "x", { writable: false });
    Object.defineProperty(victims[1], "x", { enumerable: false });
    Object.defineProperty(victims[2], "x", { configurable: false });
    Object.defineProperty(victims[3], "x", { get() { return 42; } });
    Object.defineProperty(victims[4], "y", { value: "changed" });
});

step(victims => {
    Object.preventExtensions(victims[0]);
    Object.seal(victims[1]);
    Object.freeze(victims[2]);
    Object.preventExtensions(victims[3]);
    Object.freeze(victims[3]);
    shouldBe(Reflect.defineProperty(victims[0], "added", { value: 0 }), false);
    shouldBe(Reflect.set(victims[2], "x", -1), false);
});

step(victims => {
    let children = [];
    for (let i = 0; i < 8; ++i) {
        children.push(Object.create(victims[i]));
        Object.setPrototypeOf(victims[i + 8], victims[i]);
    }
    Object.setPrototypeOf(victims[16], null);
    Object.setPrototypeOf(victims[17], Array.prototype);
    let sum = 0;
    for (let n = 0; n < testLoopCount; ++n) {
        let child = children[n % children.length];
        sum += child.x + child[firstKeys[n % firstKeys.length]] | 0;
    }
    shouldBe(typeof sum, "number");
    Object.setPrototypeOf(victims[16], Object.prototype);
    Object.setPrototypeOf(victims[17], Object.prototype);
});

// Seal, freeze, prototype changes, and brands create pinned transitions, which have no property name,
// out of Structures that are also the "from" or "to" of cached transitions.
step(victims => {
    let partial = JSON.parse(`[{"alpha":0},{"beta":1},{"gamma":2},{"delta":3},{"alpha":4,"x":4}]`);
    Object.freeze(partial[0]);
    Object.seal(partial[1]);
    Object.setPrototypeOf(partial[2], { inherited: 1 });
    Object.preventExtensions(partial[3]);
    Object.freeze(partial[4]);

    class ReturnsArgument {
        constructor(object) { return object; }
    }
    class Branded extends ReturnsArgument {
        #method() { return 1; }
        static call(object) { return object.#method(); }
    }
    class WithField extends ReturnsArgument {
        #f = 7;
        static get(object) { return object.#f; }
    }
    for (let i = 0; i < 8; ++i) {
        new Branded(victims[i]);
        shouldBe(Branded.call(victims[i]), 1);
        new WithField(victims[i + 8]);
        shouldBe(WithField.get(victims[i + 8]), 7);
    }

    for (let iteration = 0; iteration < 3; ++iteration) {
        let result = JSON.parse(`[{"alpha":0,"x":0,"y":"0","#f":1},{"beta":1,"x":1,"y":"1","#method":2}]`);
        shouldBe(JSON.stringify(Object.keys(result[0])), JSON.stringify(["alpha", "x", "y", "#f"]));
        shouldBe(result[0]["#f"], 1);
        shouldBe(JSON.stringify(Object.keys(result[1])), JSON.stringify(["beta", "x", "y", "#method"]));
        shouldBe(result[1]["#method"], 2);
        let threw = false;
        try {
            WithField.get(result[0]);
        } catch {
            threw = true;
        }
        shouldBe(threw, true);
    }
});

// Many more transitions out of one Structure than the cache has entries, and an object longer than a
// Structure chain may grow, which leaves the parser with a dictionary partway through the object.
step(() => {
    let parts = [];
    for (let i = 0; i < 600; ++i)
        parts.push(`{"k${i}":${i},"x":${i}}`);
    let wide = JSON.parse(`[${parts.join(",")}]`);
    for (let i = 0; i < wide.length; ++i) {
        shouldBe(JSON.stringify(Object.keys(wide[i])), JSON.stringify([`k${i}`, "x"]));
        shouldBe(wide[i].x, i);
    }

    let fields = [];
    for (let i = 0; i < 600; ++i)
        fields.push(`"alpha${i}":${i}`);
    let longSource = `[{${fields.join(",")}},{${fields.join(",")}}]`;
    for (let iteration = 0; iteration < 2; ++iteration) {
        for (let object of JSON.parse(longSource)) {
            let keys = Object.keys(object);
            shouldBe(keys.length, 600);
            for (let i = 0; i < 600; i += 97) {
                shouldBe(keys[i], `alpha${i}`);
                shouldBe(object[`alpha${i}`], i);
            }
        }
    }
});

step(() => {
    for (let iteration = 0; iteration < 3; ++iteration) {
        let object = JSON.parse(`{"alpha":1,"__proto__":{"x":2},"beta":3}`);
        shouldBe(Object.getPrototypeOf(object), Object.prototype);
        shouldBe(JSON.stringify(Object.keys(object)), JSON.stringify(["alpha", "__proto__", "beta"]));
        shouldBe(Object.getOwnPropertyDescriptor(object, "__proto__").value.x, 2);
        shouldBe(object.x, undefined);
    }
});

// Another realm's empty object Structure is a different Structure with the same key names.
step(() => {
    let other = createGlobalObject();
    for (let iteration = 0; iteration < 3; ++iteration) {
        let result = other.JSON.parse(source);
        for (let i = 0; i < result.length; ++i)
            checkObject(result[i], i, other);
        parseAndCheck();
    }
});

step(() => {
    $vm.haveABadTime();
    shouldBe($vm.isHavingABadTime(), true);
    let result = JSON.parse(`[{"alpha":[1,2,3],"x":{"0":1,"1":2},"y":"0"},{"beta":[4],"x":{"0":3},"y":"1"}]`);
    shouldBe(result[0].alpha[2], 3);
    shouldBe(result[0].x[1], 2);
    shouldBe(result[1].beta[0], 4);
    shouldBe(result[1].x[0], 3);
});

step(() => {
    edenGC();
    parseAndCheck();
    fullGC();
});
