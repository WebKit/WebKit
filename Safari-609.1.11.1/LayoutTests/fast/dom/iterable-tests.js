// Modifying Array prototype to ensure this does not impact iterable methods.
var entriesFunction = Array.prototype.entries;
Array.prototype.entries = function() {
    console.log("Array.prototype.entries called");
  return entriesFunction.apply(this, arguments);
}
var forEachFunction = Array.prototype.forEach;
Array.prototype.forEach = function() {
    console.log("Array.prototype.forEach called");
  return forEachFunction.apply(this, arguments);
}
var keysFunction = Array.prototype.keys;
Array.prototype.keys = function() {
    console.log("Array.prototype.keys called");
  return keysFunction.apply(this, arguments);
}
var valuesFunction = Array.prototype.values;
Array.prototype.values = function() {
    console.log("Array.prototype.values called");
  return valuesFunction.apply(this, arguments);
}

var end;
function checkEndIterator(iteratorValue) {
  end = iteratorValue;
  shouldBeTrue('end.done');
  shouldBeUndefined('end.value');
}

// Should create an iterable with two items, put in children array
var children = [];
var testedIterable = createIterable(children);

shouldBe('testedIterable.entries', 'entriesFunction');
shouldBe('testedIterable.forEach', 'forEachFunction');
shouldBe('testedIterable.keys', 'keysFunction');
shouldBe('testedIterable.values', 'valuesFunction');

shouldBe("testedIterable.length", "2");

var index = 0;
for (var item of testedIterable)
    shouldBe('item', 'children[index++]');

pair = Array.from(testedIterable);
shouldBe('pair[0]', 'children[0]');
shouldBe('pair[1]', 'children[1]');

index = 0;
var node;
var forEachIndex;
var forEachContainer;
var thisValue;
testedIterable.forEach(function(n, i, c) {
    node = n;
    forEachIndex = i;
    forEachContainer = c;
    thisValue = this;
    shouldBe('forEachContainer', 'testedIterable');
    shouldBe('forEachIndex', 'index');
    shouldBe('node', 'children[index++]');
    shouldBe('thisValue', 'window');
});

testedIterable.forEach(function() {
    thisValue = this;
    shouldBe('thisValue', 'window');
}, undefined);

var givenThisValue = testedIterable;
testedIterable.forEach(function() {
    thisValue = this;
    shouldBe('thisValue', 'givenThisValue');
}, givenThisValue);

var iterator = testedIterable.keys();
shouldBe('iterator.next().value', '0');
shouldBe('iterator.next().value', '1');
checkEndIterator(iterator.next());

var iterator = testedIterable.values();
shouldBe('iterator.next().value', 'children[0]');
shouldBe('iterator.next().value', 'children[1]');
checkEndIterator(iterator.next());

var iterator = testedIterable.entries();
var pair = iterator.next().value;
shouldBe('pair.length', '2');
shouldBe('pair[0]', '0');
shouldBe('pair[1]', 'children[0]');
pair = iterator.next().value;
shouldBe('pair.length', '2');
shouldBe('pair[0]', '1');
shouldBe('pair[1]', 'children[1]');
checkEndIterator(iterator.next());

// Should add 2 new items.
updateIterable();

checkEndIterator(iterator.next());

var testedIterablePrototype = Object.getPrototypeOf(testedIterable)
var descriptor = Object.getOwnPropertyDescriptor(testedIterablePrototype, Symbol.iterator);
shouldBeTrue('descriptor.configurable');
shouldBeTrue('descriptor.writable');
shouldBeFalse('descriptor.enumerable');

shouldNotThrow('testedIterablePrototype[Symbol.iterator] = valuesFunction;');
var counter = 0;
for (var a of testedIterable) {
    shouldBeTrue('checkItemType(a)');
    counter++;
}
shouldBe('counter', '4');
counter = 0;
for (var v of testedIterable.values()) {
    shouldBeTrue('checkItemType(v)');
    counter++;
}
shouldBe('counter', '4');

counter = 0;
for (var k of testedIterable.keys()) {
    shouldBe('typeof k', '"number"');
    counter++;
}
shouldBe('counter', '4');

counter = 0;
for (var e of testedIterable.entries()) {
    shouldBe('typeof e[0]', '"number"');
    shouldBe('e[1]', 'testedIterable[e[0]]');
    counter++;
}

shouldBe('counter', '4');
