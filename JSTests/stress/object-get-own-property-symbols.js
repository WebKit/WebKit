// This tests Object.getOwnPropertySymbols.

var global = (Function("return this")());

// private names for privileged code should not be exposed.
if (Object.getOwnPropertySymbols(global).length !== 0)
    throw "Error: bad value " + Object.getOwnPropertySymbols(global).length;

var object = {};
var symbol = Symbol("Cocoa");
object[symbol] = "Cappuccino";
if (Object.getOwnPropertyNames(object).length !== 0)
    throw "Error: bad value " + Object.getOwnPropertyNames(object).length;
if (Object.getOwnPropertySymbols(object).length !== 1)
    throw "Error: bad value " + Object.getOwnPropertySymbols(object).length;
if (Object.getOwnPropertySymbols(object)[0] !== symbol)
    throw "Error: bad value " + String(Object.getOwnPropertySymbols(object)[0]);

function forIn(obj) {
    var array = [];
    // Symbol should not be enumerated.
    for (var key in obj) array.push(key);
    return array;
}

if (forIn(object).length !== 0)
    throw "Error: bad value " + forIn(object).length;
if (Object.keys(object).length !== 0)
    throw "Error: bad value " + Object.keys(object).length;

delete object[symbol];
if (Object.getOwnPropertyNames(object).length !== 0)
    throw "Error: bad value " + Object.getOwnPropertyNames(object).length;
if (Object.getOwnPropertySymbols(object).length !== 0)
    throw "Error: bad value " + Object.getOwnPropertySymbols(object).length;
