let o1 = JSON.parse(`{ "a": 1 }`);
let o2 = { __proto__: o1 };
let o3 = JSON.parse(`{ "a": 1 }`);
