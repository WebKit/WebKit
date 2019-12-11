Array.prototype[Symbol.iterator] = function() {
    throw new Error("Bad, this should not be called!");
}

let regexp = /(foo)/;
regexp[Symbol.replace]("foo", "bar");
