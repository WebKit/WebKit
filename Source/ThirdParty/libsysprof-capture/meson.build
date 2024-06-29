install_static = not meson.is_subproject() and get_option('install-static')

libsysprof_capture_headers = files(
  'sysprof-address.h',
  'sysprof-clock.h',
  'sysprof-capture-condition.h',
  'sysprof-capture-cursor.h',
  'sysprof-capture-reader.h',
  'sysprof-capture-types.h',
  'sysprof-capture-writer.h',
  'sysprof-collector.h',
  'sysprof-macros.h',
  'sysprof-platform.h',
  'sysprof-capture.h',
  'sysprof-version-macros.h',
)

if not meson.is_subproject()
  install_headers(libsysprof_capture_headers, subdir: sysprof_header_subdir)
endif

mapped_ring_buffer_sources = files(
  'mapped-ring-buffer.c',
)

libsysprof_capture_sources = files(
  'sysprof-address.c',
  'sysprof-capture-condition.c',
  'sysprof-capture-cursor.c',
  'sysprof-capture-reader.c',
  'sysprof-capture-util.c',
  'sysprof-capture-writer.c',
  'sysprof-capture-writer-cat.c',
  'sysprof-collector.c',
  'sysprof-clock.c',
  'sysprof-platform.c',
)

configure_file(
          input: 'sysprof-version.h.in',
         output: 'sysprof-version.h',
  configuration: sysprof_version_conf,
    install_dir: join_paths(get_option('includedir'), sysprof_header_subdir),
        install: not meson.is_subproject(),
)

libsysprof_capture_deps = [
  dependency('threads'),
]

libsysprof_capture = static_library(
  'sysprof-capture-@0@'.format(libsysprof_capture_api_version),
  (libsysprof_capture_sources +
   mapped_ring_buffer_sources),

           dependencies: libsysprof_capture_deps,
                 c_args: [ '-DSYSPROF_CAPTURE_COMPILATION' ],
                install: install_static,
  gnu_symbol_visibility: 'hidden',
                    pic: true,
)

libsysprof_capture_include_dirs = include_directories('.')

libsysprof_capture_dep = declare_dependency(
           link_whole: libsysprof_capture,
         dependencies: libsysprof_capture_deps,
  include_directories: libsysprof_capture_include_dirs,
)
meson.override_dependency('sysprof-capture-@0@'.format(libsysprof_capture_api_version), libsysprof_capture_dep)

if install_static
  pkgconfig.generate(
                 libsysprof_capture,
        subdirs: [ sysprof_header_subdir ],
    description: 'The static capture library for tools that generate profiling capture data',
      variables: [ 'datadir=' + datadir_for_pc_file ],
    libraries_private: libsysprof_capture_deps,
  )
endif

if get_option('tests')
  subdir('tests')
endif
