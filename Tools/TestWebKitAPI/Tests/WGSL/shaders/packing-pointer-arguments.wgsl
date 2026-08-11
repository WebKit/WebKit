@group(0) @binding(0) var<storage, read_write> buf: vec3u;
var<private> buf2: vec3u;

fn bar(a: ptr<storage, vec3u, read_write>) {
}

fn baz(a: vec3u) {
}

fn foo(a: ptr<storage, vec3u, read_write>) {
  bar(a);
  baz(*a);
}

fn f(b: ptr<function, vec3u>) {
  var b2 = *b;
}

fn f2(b: ptr<private, vec3u>) {
  var b2 = *b;
}

@compute @workgroup_size(1)
fn main() {
  foo(&buf);
  var b = buf;
  f(&b);
  f2(&buf2);
}

