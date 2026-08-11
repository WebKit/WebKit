//@ runDefault("--slowPathAllocsBetweenGCs=13")
function foo(object) {
  'hello' in object;
}

let handler = {
  has(_, keyArg) {
    keyArg in targetObject;
  }
};
let targetObject = {};
let proxy = new Proxy(targetObject, handler);
for (let i = 0; i < testLoopCount; ++i) {
  foo(proxy);
}
targetObject.hello = undefined;
gc();
for (let i = 0; i < testLoopCount; ++i) {
  foo(proxy);
}
delete targetObject?.hello;
for (let i = 0; i < testLoopCount; ++i) {
  foo(proxy);
}
