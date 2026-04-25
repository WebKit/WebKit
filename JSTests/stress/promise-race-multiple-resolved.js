const arr = [];
const result = Promise.race([arr, {}]);
arr.__proto__ = result;
drainMicrotasks();
