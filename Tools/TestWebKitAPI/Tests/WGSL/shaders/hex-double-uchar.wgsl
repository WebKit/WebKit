@compute @workgroup_size(1)
fn main() {
  // ⚠️ -- the emoji ensures the file will be parsed as uchar
  let f32min = 0x1p-126;
  let f32max = 0x1.fffffep+127;
}
