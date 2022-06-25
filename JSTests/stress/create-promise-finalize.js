function foo() {
    class P extends Promise {}
    new P(()=>{});
}

foo();
drainMicrotasks();
gc();
foo();
