var global = (Function("return this"))();
if (global.count !== 3)
    throw new Error(`bad value ${global.count}`);
global.count = 4;
