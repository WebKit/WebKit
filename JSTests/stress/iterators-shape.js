// This test checks the shape of builtin iterators.

function iteratorShape(iter) {
    if (iter.hasOwnProperty('next'))
        throw "Error: iterator should not have next method.";
    if (!iter.__proto__.hasOwnProperty('next'))
        throw "Error: iterator prototype should have next method.";
    if (typeof iter.__proto__.next !== "function")
        throw "Error: iterator prototype should have next method.";
}

function sameNextMethods(iterators) {
    var iterator = iterators[0];
    for (var i = 1; i < iterators.length; ++i) {
        if (iterator.next !== iterators[i].next)
            throw "Error: next method is not the same.";
    }
}

var array = ['Cocoa', 'Cappuccino', 'The des Alizes', 'Matcha', 'Kilimanjaro'];
var iterator = array[Symbol.iterator]();
iteratorShape(iterator);

var keyIterator = array.keys();
iteratorShape(keyIterator);

var keyValueIterator = array.entries();
iteratorShape(keyValueIterator);

sameNextMethods([array[Symbol.iterator](), array.keys(), array.entries()]);

var set = new Set(['Cocoa', 'Cappuccino', 'The des Alizes', 'Matcha', 'Kilimanjaro']);
var iterator = set[Symbol.iterator]();
iteratorShape(iterator);

var keyIterator = set.keys();
iteratorShape(keyIterator);

var keyValueIterator = set.entries();
iteratorShape(keyValueIterator);

sameNextMethods([set[Symbol.iterator](), set.keys(), set.entries()]);

var map = new Map();
[
    [ 'Cocoa', 2, ],
    [ 'Cappuccino', 0 ],
    [ 'The des Alizes', 3 ],
    [ 'Matcha', 2 ],
    [ 'Kilimanjaro', 1]
].forEach(function ([ key, value ]) {
    map.set(key, value);
});
var iterator = map[Symbol.iterator]();
iteratorShape(iterator);

var keyIterator = map.keys();
iteratorShape(keyIterator);

var keyValueIterator = map.entries();
iteratorShape(keyValueIterator);

sameNextMethods([map[Symbol.iterator](), map.keys(), map.entries()]);
