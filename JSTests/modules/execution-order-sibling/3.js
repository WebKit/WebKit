var global = (Function("return this"))();
if (global.count !== 2)
    throw new Error(`bad value ${global.count}`);
global.count = 3;
