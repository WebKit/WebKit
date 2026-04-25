function shouldThrow(func) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
}


class C31 {
  constructor(a33) {
    a33[536870912] = 2;
    for (const v38 in a33) {
      print(v38);
    }
  }
}
Object.defineProperty(C31.__proto__, 536870912, { enumerable: true, value: {}});
shouldThrow(() => { new C31(C31); });
shouldThrow(() => { new C31(C31); });
shouldThrow(() => { new C31(C31); });
shouldThrow(() => { new C31(Map); });
