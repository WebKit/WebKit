// RUN: %not %wgslc | %check

@compute
fn main() {
  {
    // CHECK-L: no matching overload for operator +(ref<function, i32, read_write>, f32)
    var x = 1;
    x += 1f;
  }

  {
    // CHECK-L: cannot assign to a value of type 'i32'
    let x = 1;
    x += 1;
  }
}
