function foo() {
    let target = {
      __proto__: null
    };
    let proxy = new Proxy(target, {
        get(...args) {
            return Reflect.get(...args);
        }
    });
    Object.prototype.__proto__ = {
       __proto__: proxy,
    };

    let a = {};
    a.test;
}
foo();
