// RUN: %not %wgslc | %check

fn main() {
  // CHECK-L: Expected a ;, but got a Identifier
  var x = 1;
  *&*&x=3 x=1;
}
