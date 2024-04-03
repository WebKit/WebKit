// RUN: %not %wgslc | %check

fn testIndexAccess() {
  // CHECK-L: index must be of type 'i32' or 'u32', found: 'f32'
  _ = vec2(0)[1f];
}

fn testSwizzleAccess()
{
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

  // CHECK-L: invalid vector swizzle character
    var z: mat3x3<f32>;
    _ = z[1].e;
}

fn testConcretizationIfIndexIsNotConstant()
{
    // The restriction below can be found in the spec for the following types:
    // - Vectors: https://www.w3.org/TR/WGSL/#vector-single-component
    // - Matrices: https://www.w3.org/TR/WGSL/#matrix-access-expr
    // - Arrays: https://www.w3.org/TR/WGSL/#array-access-expr
    //
    // NOTE: When an abstract vector value e is indexed by an expression that is not
    // a const-expression, then the vector is concretized before the index is applied.

    let i = 0;

    // CHECK-NOT-L: cannot initialize var of type 'f32' with value of type 'i32'
    { let x: f32 = vec2(0)[0]; }
    // CHECK-L: cannot initialize var of type 'u32' with value of type 'i32'
    { let x: u32 = vec2(0)[i]; }

    // FIXME: in order to test this we need to implement f16
    // { let x: vec2<f16> = mat2x2(0, 0, 0, 0)[i]; }

    // CHECK-NOT-L: cannot initialize var of type 'f32' with value of type 'i32'
    { let x: f32 = array(0)[0]; }
    // CHECK-L: cannot initialize var of type 'u32' with value of type 'i32'
    { let x: u32 = array(0)[i]; }
}

fn testCannotWriteToMultipleVectorLocations()
{
    var v = vec2(0);
    // CHECK-L: cannot assign to a value of type 'vec2<i32>'
    v.xy = vec2(1);
}
