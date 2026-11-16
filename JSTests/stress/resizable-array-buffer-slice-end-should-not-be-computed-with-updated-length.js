let a = new ArrayBuffer(0, {'maxByteLength': 1});

let x = {
  [Symbol.toPrimitive]() {
    a.resize(1);
  },
};

a.slice(x);
