// Test changing the value of toStringTag

// Test adding toStringTag to the base with miss.

// SuperPrototype can't be an empty object since its transition
// watchpoint will be clobbered when assigning it to the prototype.
var SuperPrototype = { bar: 1 }
var BasePrototype = { }
Object.setPrototypeOf(BasePrototype, SuperPrototype);

function Base() { }
Base.prototype = BasePrototype;

var value = new Base();

if (value.toString() !== "[object Object]")
    throw "bad miss toStringTag";

value[Symbol.toStringTag] = "hello";

if (value.toString() !== "[object hello]")
    throw "bad swap on base value with miss";

// Test adding toStringTag to the prototype with miss.

value = new Base();

if (value.toString() !== "[object Object]")
    throw "bad miss toStringTag";

SuperPrototype[Symbol.toStringTag] = "superprototype";

if (value.toString() !== "[object superprototype]")
    throw "bad prototype toStringTag change with miss";

// Test adding toStringTag to the base with a hit.

value[Symbol.toStringTag] = "hello2";

if (value.toString() !== "[object hello2]")
    throw "bad swap on base value with hit";

// Test toStringTag on the prototype.

if (Object.getPrototypeOf(value).toString() !== "[object superprototype]")
    throw "bad prototype toStringTag access";

// Test adding to string to the prototype with hit.

value = new Base();

BasePrototype[Symbol.toStringTag] = "baseprototype";

if (value.toString() !== "[object baseprototype]")
    throw "bad prototype toStringTag interception with hit";

// Test replacing the string on prototype.

BasePrototype[Symbol.toStringTag] = "not-baseprototype!";

if (value.toString() !== "[object not-baseprototype!]")
    throw "bad prototype toStringTag interception with hit";
