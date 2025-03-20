// RUN: %metal-compile main

@group(0) @binding(2) var<storage, read> x: array<vec4f>;

fn f1(p: ptr<storage, array<vec4f>, read>) {
  _ = p[0];
};

fn f0() {
  f1(&x);
}

@compute @workgroup_size(1)
fn main() {
    f0();
}
