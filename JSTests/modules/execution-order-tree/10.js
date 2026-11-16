var global = (Function("return this"))();
if (global.count !== 9)
    throw new Error(`bad value ${global.count}`);
global.count = 10;
