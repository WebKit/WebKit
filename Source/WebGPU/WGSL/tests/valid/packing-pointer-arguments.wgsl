// RUN: %metal-compile main

@group(0) @binding(0) var<storage, read_write> buf: vec3u;

fn bar(a: ptr<storage, vec3u, read_write>) {
}

fn baz(a: vec3u) {
}

fn foo(a: ptr<storage, vec3u, read_write>) {
  bar(a);
  baz(*a);
}

@compute @workgroup_size(1)
fn main() {
  foo(&buf);
}

