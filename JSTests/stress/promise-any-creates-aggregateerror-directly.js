let count = 0;
Array.prototype[Symbol.iterator] = function* () {
  if (count++ >= 2)
    throw "done";
  Promise.any([]);
};
Array.from([]);

if (count !== 3)
  throw new Error("Array.prototype[Symbol.iterator] invoked incorrect number of times");
