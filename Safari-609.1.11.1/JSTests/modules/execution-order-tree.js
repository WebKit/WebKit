import "./execution-order-tree/5.js"
import "./execution-order-tree/7.js"
import "./execution-order-tree/11.js"

var global = (Function("return this"))();
if (global.count !== 11)
    throw new Error(`bad value ${global.count}`);
