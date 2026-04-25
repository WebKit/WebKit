var buffer = new ArrayBuffer(3);

var newTarget = function () {
}.bind(null)

let i = 0

Object.defineProperty(newTarget, 'prototype', {
  get: function () {
    let iter = i
    ++i;
    // print(iter)
    if (iter >= 1)
        return { }
    // print("Construct B", iter)
    Reflect.construct(Object, [], newTarget);
    // print("Construct B Done: ", iter)
    return { };
  }
});

// print("Construct A")
var result = Reflect.construct(Object, [], newTarget);