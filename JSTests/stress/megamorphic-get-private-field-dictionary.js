function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

class Stamper extends (function (object) { return object; }) {
    #state;

    constructor(object, state) {
        super(object);
        this.#state = state;
    }

    static get(object) {
        return object.#state;
    }

    static set(object, state) {
        object.#state = state;
    }
}
noInline(Stamper.get);

let objects = [];
for (let i = 0; i < 16; ++i)
    objects.push(new Stamper({ ['extra' + i]: i }, i));

function check(list) {
    for (let i = 0; i < testLoopCount; ++i) {
        let object = list[i % list.length];
        Stamper.set(object, i);
        shouldBe(Stamper.get(object), i);
    }
}

check(objects);

let dictionaries = [];
for (let i = 0; i < 4; ++i)
    dictionaries.push($vm.toCacheableDictionary(new Stamper({ ['dictionary' + i]: i }, i)));
let all = objects.concat(dictionaries);
check(all);
for (let i = 0; i < 100; ++i) {
    for (let dictionary of dictionaries) {
        dictionary['added' + i] = i;
        Stamper.set(dictionary, i);
        shouldBe(Stamper.get(dictionary), i);
    }
}
check(all);

for (let i = 0; i < dictionaries.length; ++i) {
    $vm.toUncacheableDictionary(dictionaries[i]);
    delete dictionaries[i]['dictionary' + i];
}
check(all);

// The deleted property preceded the private field, so flattening changes the offset of the private field.
for (let dictionary of dictionaries) {
    Stamper.set(dictionary, 42);
    $vm.flattenDictionaryObject(dictionary);
    shouldBe(Stamper.get(dictionary), 42);
}
check(all);

for (let dictionary of dictionaries)
    Object.freeze(dictionary);
check(all);
