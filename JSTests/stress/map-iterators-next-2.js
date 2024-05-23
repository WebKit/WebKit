function assert(b) {
    if (!b)
        throw new Error("Bad result!");
}
noInline(assert);

let item1 = [1, 2];
let item2 = [3, 4];

{
    let map = new Map();
    let iterator = map[Symbol.iterator]();
    map.set(item1[0], item1[1]);
    let element = iterator.next();

    assert(element.done == false);
    assert(element.value[0] == item1[0]);
}

{
    let map = new Map([item1]);
    let iterator = map[Symbol.iterator]();
    map.set(item2[0], item2[1]);
    let element = iterator.next();
    assert(element.done == false);
    assert(element.value[0] == item1[0]);

    element = iterator.next();
    assert(element.done == false);
    assert(element.value[0] == item2[0]);
}

{
    let map = new Map([item1]);
    let iterator = map[Symbol.iterator]();
    map.delete(item1[0]);

    let element = iterator.next();
    assert(element.done == true);
    assert(element.value == undefined);

    element = iterator.next();
    assert(element.done == true);
    assert(element.value == undefined);
}

{
    let map = new Map([item1, item2]);
    let iterator = map[Symbol.iterator]();
    map.delete(item2[0]);

    let element = iterator.next();
    assert(element.done == false);
    assert(element.value[0] == item1[0]);

    element = iterator.next();
    assert(element.done == true);
    assert(element.value == undefined);

    element = iterator.next();
    assert(element.done == true);
    assert(element.value == undefined);
}

{
    let map = new Map([item1, item2]);
    let iterator = map[Symbol.iterator]();

    let element = iterator.next();
    assert(element.done == false);
    assert(element.value[0] == item1[0]);

    element = iterator.next();
    assert(element.done == false);
    assert(element.value[0] == item2[0]);

    element = iterator.next();
    assert(element.done == true);
    assert(element.value == undefined);
}

{
    let map = new Map([item1]);
    let iterator = map[Symbol.iterator]();
    map.clear();

    let element = iterator.next();
    assert(element.done == true);
    assert(element.value == undefined);

    element = iterator.next();
    assert(element.done == true);
    assert(element.value == undefined);

    map.set(item1[0], item1[1]);
    element = iterator.next();
    assert(element.done == true);
    assert(element.value == undefined);
}

{
    let map = new Map([item1]);
    let iterator = map[Symbol.iterator]();
    map.clear();

    map.set(item1[0], item1[1]);
    element = iterator.next();
    assert(element.done == false);
    assert(element.value[0] == item1[0]);
}
