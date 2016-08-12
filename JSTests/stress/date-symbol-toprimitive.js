delete Date.prototype[Symbol.toPrimitive]

let date = new Date();

if (typeof (date + 1) !== "number")
    throw "symbol was not deleted";
