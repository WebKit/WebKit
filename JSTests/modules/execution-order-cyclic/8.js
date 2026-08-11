import "./7.js"

var global = (Function("return this"))();
if (global.count !== 7)
    throw new Error(`bad value ${global.count}`);
global.count = 8;
