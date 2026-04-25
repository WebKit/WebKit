var data = [{
  a: 1n << 63n,
  b: 2n,
  r: 0x4000000000000000n,
}, {
  a: -(1n << 63n),
  b: 2n,
  r: -0x4000000000000000n,
},{
  a: 1n << 127n,
  b: 1n << 64n,
  r: 0x8000000000000000n,
}, {
  a: -(1n << 127n),
  b: 1n << 64n,
  r: -0x8000000000000000n,
},{
  a: (1n << 128n) - 1n,
  b: 1n << 64n,
  r: 0xffffffffffffffffn,
}, {
  a: -((1n << 128n) - 1n),
  b: 1n << 64n,
  r: -0xffffffffffffffffn,
},{
  a: (1n << 192n) - 1n,
  b: 1n << 128n,
  r: 0xffffffffffffffffn,
}, {
  a: -((1n << 192n) - 1n),
  b: 1n << 128n,
  r: -0xffffffffffffffffn,
}];

var error_count = 0;
for (var i = 0; i < data.length; i++) {
  var d = data[i];
  var r = d.a / d.b;
  if (d.r !== r) {
    print("Input A:  " + d.a.toString(16));
    print("Input B:  " + d.b.toString(16));
    print("Result:   " + r.toString(16));
    print("Expected: " + d.r);
    print("Op: /");
    error_count++;
  }
}
if (error_count !== 0)
  throw new Error("Finished with " + error_count + " errors.")

