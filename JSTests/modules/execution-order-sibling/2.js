var global = (Function("return this"))();
if (global.count !== 1)
    throw new Error(`bad value ${global.count}`);
global.count = 2;
