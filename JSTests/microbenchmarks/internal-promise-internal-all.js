
const internalPromise = $vm.createBuiltin("(function (executor) { return new @InternalPromise(executor); })")
const internalPromiseAll = $vm.createBuiltin("(function (promises) { return @InternalPromise.internalAll(promises); })");

const promises = [];
for (let i = 0; i < 12; i++)
  promises.push(internalPromise((resolve) => resolve(i)));

for (var i = 0; i < 1e4; ++i)
    internalPromiseAll(promises);

drainMicrotasks();
