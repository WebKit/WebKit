const validButUnsupportedConfigs = [
  {
    comment: 'Possible future HEVC codec string',
    config: {
      codec: 'hvc1.C99.6FFFFFF.L93',
      width: 640,
      height: 480,
    },
  },
];

validButUnsupportedConfigs.forEach(entry => {
  let config = entry.config;
  promise_test(
      async t => {
        let support = await VideoEncoder.isConfigSupported(config);
        assert_false(support.supported);

        let new_config = support.config;
        assert_equals(new_config.codec, config.codec);
        assert_equals(new_config.width, config.width);
        assert_equals(new_config.height, config.height);
        if (config.bitrate)
          assert_equals(new_config.bitrate, config.bitrate);
        if (config.framerate)
          assert_equals(new_config.framerate, config.framerate);
      },
      'Test that VideoEncoder.isConfigSupported() doesn\'t support config: ' +
          entry.comment);
});

validButUnsupportedConfigs.forEach(entry => {
  async_test(
      t => {
        let codec = new VideoEncoder({
          output: t.unreached_func('unexpected output'),
          error: t.step_func_done(e => {
            assert_true(e instanceof DOMException);
            assert_equals(e.name, 'NotSupportedError');
            assert_equals(codec.state, 'closed', 'state');
          })
        });
        codec.configure(entry.config);
      },
      'Test that VideoEncoder.configure() doesn\'t support config: ' +
          entry.comment);
});
