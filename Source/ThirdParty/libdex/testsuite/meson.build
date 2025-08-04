test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'GSETTINGS_BACKEND=memory',
  'MALLOC_CHECK_=2',
  'NO_AT_BRIDGE=1',
]

testsuite_c_args = [
  '-DDEX_COMPILATION',
  '-DG_LOG_DOMAIN="libdex"',
  '-DG_ENABLE_DEBUG',
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
]

testsuite = {
  'test-async-result': {},
  'test-channel': {},
  'test-object': {},
  'test-fiber': {},
  'test-future': {},
  'test-scheduler': {},
  'test-semaphore': {},
  'test-stream': {},
}

testsuite_deps = [
  libdex_static_dep,
]

if get_option('sysprof')
  testsuite_deps += [libsysprof_capture_dep]
endif

foreach test, params: testsuite
  test_exe = executable(test, '@0@.c'.format(test),
          c_args: testsuite_c_args + deprecated_c_args,
    dependencies: testsuite_deps,
  )

  test(test, test_exe, env: test_env)
endforeach
