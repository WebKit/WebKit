diagnostic(off, derivative_uniformity);
diagnostic(off, chromium.unreachable_code);
struct FSOut {
  @location(0) sk_FragColor: vec4<f32>,
};
struct _GlobalUniforms {
  colorXform: mat4x4<f32>,
};
@binding(0) @group(0) var<uniform> _globalUniforms: _GlobalUniforms;
@group(0) @binding(10000) var s_Sampler: sampler;
@group(0) @binding(10001) var s_Texture: texture_2d<f32>;
fn _skslMain(_stageOut: ptr<function, FSOut>) {
  {
    var tmpColor: vec4<f32>;
    tmpColor = vec4<f32>(textureSample(s_Texture, s_Sampler, vec2<f32>(1.0)));
    var _skTemp2: vec4<f32>;
    let _skTemp3 = mat4x4<f32>(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    if (any(_globalUniforms.colorXform[0] != _skTemp3[0]) || any(_globalUniforms.colorXform[1] != _skTemp3[1]) || any(_globalUniforms.colorXform[2] != _skTemp3[2]) || any(_globalUniforms.colorXform[3] != _skTemp3[3])) {
      let _skTemp4 = clamp((_globalUniforms.colorXform * vec4<f32>(tmpColor.xyz, 1.0)).xyz, vec3<f32>(0.0), vec3<f32>(tmpColor.w));
      _skTemp2 = vec4<f32>(_skTemp4, tmpColor.w);
    } else {
      _skTemp2 = tmpColor;
    }
    (*_stageOut).sk_FragColor = vec4<f32>(_skTemp2);
  }
}
@fragment fn main() -> FSOut {
  var _stageOut: FSOut;
  _skslMain(&_stageOut);
  return _stageOut;
}
