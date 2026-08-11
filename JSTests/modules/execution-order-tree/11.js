import "./9.js";
import "./10.js";

var global = (Function("return this"))();
if (global.count !== 10)
    throw new Error(`bad value ${global.count}`);
global.count = 11;
