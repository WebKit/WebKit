var global = (Function("return this"))();
if (global.count !== 5)
    throw new Error(`bad value ${global.count}`);
global.count = 6;
