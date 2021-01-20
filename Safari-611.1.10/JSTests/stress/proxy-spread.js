function foo() {}
let p = new Proxy(foo, {});
let a = {...p};
