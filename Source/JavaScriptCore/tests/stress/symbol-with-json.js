// This tests JSON correctly behaves with Symbol.

if (JSON.stringify(Symbol('Cocoa')) !== undefined)
    throw "Error: bad value " + JSON.stringify(Symbol('Cocoa'));

var object = {};
var symbol = Symbol("Cocoa");
object[symbol] = 42;
object['Cappuccino'] = 42;
if (JSON.stringify(object) !== '{"Cappuccino":42}')
    throw "Error: bad value " + JSON.stringify(object);

if (JSON.stringify(object, [ Symbol('Cocoa') ]) !== "{}")
    throw "Error: bad value " + JSON.stringify(object, [ Symbol('Cocoa') ]);
