global1 = createGlobalObject();
global2 = createGlobalObject();

function bar() {}
Reflect.construct(global1.Object, {}, bar);
Reflect.construct(global2.Object, {}, bar);
