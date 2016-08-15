import "./execution-order-self.js"

var global = (Function("return this"))();
if (typeof global.count !== 'undefined')
    throw new Error(`bad value ${global.count}`);
global.count = 1;
