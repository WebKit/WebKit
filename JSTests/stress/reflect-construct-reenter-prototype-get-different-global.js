var buffer = new ArrayBuffer(3);

var newTarget = function () {
}.bind(null)

let i = 0

let newGlobal = $vm.createGlobalObject()

Object.defineProperty(newTarget, 'prototype', {
  get: function () {
    let iter = i
    ++i;
    // print(iter)
    if (iter >= 1)
      return { }
    // print("Construct B", iter)
    Reflect.construct(newGlobal.Object, [], newTarget);
    // print("Construct B Done: ", iter)
    return { };
  }
});

// print("Construct A")
var result = Reflect.construct(Object, [], newTarget);