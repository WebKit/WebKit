function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function countUntilNull(list) {
    let count = 0;
    let node = list;
    while (node != null) {
        ++count;
        node = node.next;
    }
    return count;
}

function countWhileNull(values) {
    let count = 0;
    let index = 0;
    let value;
    do {
        value = values[index++];
        ++count;
    } while (value == null);
    return count;
}

function countNodes(list) {
    let count = 0;
    let node = list;
    do {
        ++count;
        node = node.next;
    } while (node != null);
    return count;
}

function countUntilNullNegated(list) {
    let count = 0;
    let node = list;
    while (!(node == null)) {
        ++count;
        node = node.next;
    }
    return count;
}

function countWhileNullNegated(values) {
    let count = 0;
    let index = 0;
    let value;
    do {
        value = values[index++];
        ++count;
    } while (!(value != null));
    return count;
}

function countNodesNegated(list) {
    let count = 0;
    let node = list;
    do {
        ++count;
        node = node.next;
    } while (!(node == null));
    return count;
}

let masquerader = makeMasquerader();
let list = { next: { next: { next: null } } };
let undefinedTerminated = { next: { next: { } } };
let masqueraderTerminated = { next: { next: masquerader } };

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(countUntilNull(list), 3);
    shouldBe(countUntilNull(undefinedTerminated), 3);
    shouldBe(countUntilNull(masquerader), 0);
    shouldBe(countWhileNull([null, undefined, masquerader, 0]), 4);
    shouldBe(countWhileNull([1]), 1);
    shouldBe(countNodes(list), 3);
    shouldBe(countNodes(undefinedTerminated), 3);
    shouldBe(countNodes(masqueraderTerminated), 2);
    shouldBe(countUntilNullNegated(list), 3);
    shouldBe(countUntilNullNegated(undefinedTerminated), 3);
    shouldBe(countUntilNullNegated(masquerader), 0);
    shouldBe(countWhileNullNegated([null, undefined, masquerader, 0]), 4);
    shouldBe(countWhileNullNegated([1]), 1);
    shouldBe(countNodesNegated(list), 3);
    shouldBe(countNodesNegated(undefinedTerminated), 3);
    shouldBe(countNodesNegated(masqueraderTerminated), 2);
}
