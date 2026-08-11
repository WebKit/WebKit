override a_val:i32 = 1;
override b_val:i32 = 1;
override bad_size = (a_val - 10);
override good_size = (b_val + 10);
struct S { x: f32 };
override v = S(pow(2.f, vec2f(array(modf(f32(b_val)).whole)[0]).xy[0])).x;
var<workgroup> zero_array:array<i32, select(bad_size, good_size, a_val == 1 && b_val == 1 )>;

@workgroup_size(1)
@compute fn main() {
    var x = v;
    let foo = zero_array[0];
}
