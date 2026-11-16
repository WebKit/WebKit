import "./11.js";
import "./8.js";
import "./6.js";

var global = (Function("return this"))();
if (global.count !== 8)
    throw new Error(`bad value ${global.count}`);
global.count = 9;
