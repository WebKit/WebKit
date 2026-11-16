const x = {};
Object.defineProperty(x, '__proto__', {get: ()=>{}});
Object.assign(x, { get: ()=> {} });
