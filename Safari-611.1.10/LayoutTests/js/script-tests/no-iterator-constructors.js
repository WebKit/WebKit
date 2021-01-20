description("This test makes sure we aren't putting the iterator constructors on the global object.");

var global = this;
shouldBeFalse("'ArrayIterator' in this");
shouldBeFalse("'ArgumentsIterator' in this");
shouldBeFalse("'MapIterator' in this");
shouldBeFalse("'SetIterator' in this");
