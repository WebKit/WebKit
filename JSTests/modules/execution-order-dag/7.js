import "./6.js";

var global = (Function("return this"))();
if (global.count !== 6)
    throw new Error(`bad value ${global.count}`);
global.count = 7;
