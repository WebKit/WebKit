import "./execution-order-sibling/1.js"
import "./execution-order-sibling/2.js"
import "./execution-order-sibling/3.js"

var global = (Function("return this"))();
if (global.count !== 3)
    throw new Error(`bad value ${global.count}`);
