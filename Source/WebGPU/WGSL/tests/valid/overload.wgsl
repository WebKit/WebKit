fn testAdd() {
  {
    let x1 = 1 + 2;
    let x2 = 1i + 1;
    let x3 = 1 + 1i;
    let x4 = 1u + 1;
    let x5 = 1 + 1u;
    let x6 = 1.0 + 2;
    let x7 = 1 + 2.0;
    let x8 = 1.0 + 2.0;
  }


  {
    let v1 = vec2<f32>(0, 0) + 1;
    let v2 = vec3<f32>(0, 0, 0) + 1;
    let v3 = vec4<f32>(0, 0, 0, 0) + 1;
  }

  {
    let v1 = 1 + vec2<f32>(0, 0);
    let v2 = 1 + vec3<f32>(0, 0, 0);
    let v3 = 1 + vec4<f32>(0, 0, 0, 0);
  }

  {
    let v1 = vec2<f32>(0, 0) + vec2<f32>(1, 1);
    let v2 = vec3<f32>(0, 0, 0) + vec3<f32>(1, 1, 1);
    let v3 = vec4<f32>(0, 0, 0, 0) + vec4<f32>(1, 1, 1, 1);
  }

  {
    let m1 = mat2x2<f32>(0, 0, 0, 0) + mat2x2<f32>(1, 1, 1, 1);
    let m2 = mat2x3<f32>(0, 0, 0, 0, 0, 0) + mat2x3<f32>(1, 1, 1, 1, 1, 1);
    let m3 = mat2x4<f32>(0, 0, 0, 0, 0, 0, 0, 0) + mat2x4<f32>(1, 1, 1, 1, 1, 1, 1, 1);
  }
}

fn testMultiply() {
  let v2 = vec2<f32>(0, 0);
  let v4 = vec4<f32>(0, 0, 0, 0);
  let m = mat2x4<f32>(0, 0, 0, 0);
  let r1 = m * v2;
  let r2 = v4 * m;
}

fn testTextureSample() {
  {
    let t: texture_1d<f32>;
    let s: sampler;
    let r = textureSample(t, s, 1);
  }

  {
    let t: texture_2d<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec2<f32>(0, 0));
  }

  {
    let t: texture_2d<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec2<f32>(0, 0), vec2<i32>(1, 1));
  }

  {
    let t: texture_2d_array<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec2<f32>(0, 0), 0);
  }

  {
    let t: texture_2d_array<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec2<f32>(0, 0), 0, vec2<i32>(1, 1));
  }

  {
    let t: texture_3d<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec3<f32>(0, 0, 0));
  }

  {
    let t: texture_cube<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec3<f32>(0, 0, 0));
  }

  {
    let t: texture_3d<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec3<f32>(0, 0, 0), vec3<i32>(0, 0, 0));
  }

  {
    let t: texture_cube_array<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec3<f32>(0, 0, 0), 0);
  }
}
