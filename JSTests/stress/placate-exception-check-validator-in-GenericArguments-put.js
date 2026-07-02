function foo() {
    const o = new Proxy(arguments, {});
    o.x = null;
}
foo();
