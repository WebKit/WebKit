diagnostic(off, derivative_uniformity);
diagnostic(off, chromium.unreachable_code);
enable f16;
struct FSOut {
  @location(0) sk_FragColor: vec4<f16>,
};
@group(0) @binding(10000) var t_Sampler: sampler;
@group(0) @binding(10001) var t_Texture: texture_2d<f32>;
fn _skslMain(_stageOut: ptr<function, FSOut>) {
  {
    let dims: vec2<u32> = textureDimensions(t_Texture);
    (*_stageOut).sk_FragColor = vec4<f16>(textureSample(t_Texture, t_Sampler, vec2<f32>(vec2<f16>(dims))));
  }
}
@fragment fn main() -> FSOut {
  var _stageOut: FSOut;
  _skslMain(&_stageOut);
  return _stageOut;
}
