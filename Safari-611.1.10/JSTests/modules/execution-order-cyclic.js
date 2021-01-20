import "./execution-order-cyclic/11.js"

var global = (Function("return this"))();
if (global.count !== 11)
    throw new Error(`bad value ${global.count}`);
