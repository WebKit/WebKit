// RUN: %metal-compile main
// RUN: %metal main 2>&1 | %check


@compute @workgroup_size(1)
fn main()
{
    // CHECK-L: 8680
    let x = determinant(mat4x4(
      15,2,34,4,
      18,2,3,4,
      1,72,32,4,
      17,2,3,4,
    ));

    // CHECK-L: 8680
    let y = determinant(mat4x4(
        vec4(15,2,34,4),
        vec4(18,2,3,4),
        vec4(1,72,32,4),
        vec4(17,2,3,4),
    ));

    const m2 = mat2x2(
      1,2,
      3,4,
    );

    // CHECK-L: 1., 3., 2., 4.
    let tm2 = transpose(m2);

    // CHECK-L: 1., 4., 7., 2., 5., 8., 3., 6., 9.
    let tm3 = transpose(mat3x3(
      1,2,3,
      4,5,6,
      7,8,9,
    ));

    // CHECK-L: 2., 4., 6., 8.
    let x0 = m2 * 2;

    const m2x3 = mat2x3(
      1,2,3,
      4,5,6
    );

    // CHECK-L: 1., 4., 2., 5., 3., 6.
    let x1 = transpose(m2x3);

    // CHECK-L: 10., 14., 18.
    let x2 = m2x3 * vec2(2);

    // CHECK-L: 12., 30.
    let x3 = vec3(2) * m2x3;

    // CHECK-L: 32
    let x4 = dot(vec3(1,2,3), vec3(4,5,6));

    // CHECK-L: 95., 128., 68., 92., 41., 56., 14., 20.
    let x5 = mat3x2(1,2,3,4,5,6) * mat4x3(12,11,10,9,8,7,6,5,4,3,2,1);
}
