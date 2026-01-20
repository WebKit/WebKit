enable f16;

@group(1) @binding(40) var<storage, read_write> buffer32: array<vec4i>;
@group(1) @binding(126) var<storage, read> buffer36: array<i32>;

fn fn0(a0: ptr<storage, array<vec4i>, read_write>) {
  var a = a0[0];
}

fn fn1() {
  fn0(&buffer32);
}

fn fn2() {
  fn1();
  fn0(&buffer32);
}

fn fn3() {
  fn2();
  fn0(&buffer32);
}

fn fn4(a0: ptr<storage, array<i32>, read>) {
  var a = buffer32[0];
  var b = a0[0];
}

@compute @workgroup_size(3, 1, 1)
fn fn5() {
  fn4(&buffer36);
  fn3();
}
