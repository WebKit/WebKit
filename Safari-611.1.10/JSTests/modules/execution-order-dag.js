// This tests the DAG.
import "./execution-order-dag/9.js"
import "./execution-order-dag/10.js"

var global = (Function("return this"))();
if (global.count !== 10)
    throw new Error(`bad value ${global.count}`);
