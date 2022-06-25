import "./3.js"
import "./4.js"

var global = (Function("return this"))();
if (global.count !== 4)
    throw new Error(`bad value ${global.count}`);
global.count = 5;
