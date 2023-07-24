// RUN: %not %wgslc | %check

fn testIndexAccess() {
  // CHECK-L: index must be of type 'i32' or 'u32', found: 'f32'
  _ = vec2(0)[1f];

  // CHECK-L: invalid vector swizzle member
  _ = vec2(0).z;
  // CHECK-L: invalid vector swizzle member
  _ = vec2(0).w;
  // CHECK-L: invalid vector swizzle member
  _ = vec2(0).b;
  // CHECK-L: invalid vector swizzle member
  _ = vec2(0).a;

  // CHECK-L: invalid vector swizzle member
  _ = vec3(0).w;
  // CHECK-L: invalid vector swizzle member
  _ = vec3(0).a;

  // CHECK-L: invalid vector swizzle member
  _ = vec2(0).rx;

  // CHECK-L: invalid vector swizzle character
  _ = vec2(0).v;
}
