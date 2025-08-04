muprof_sources = [
  'mapped-ring-buffer-source.c',
  'sysprof-controlfd-instrument.c',
  'sysprof-diagnostic.c',
  'sysprof-instrument.c',
  'sysprof-profiler.c',
  'sysprof-recording.c',
  'sysprof-spawnable.c',

  'main.c',
]

libdex_proj = subproject(
  'libdex',
  default_options: [
    'examples=false',
    'introspection=disabled',
    'sysprof=false',
    'tests=false',
    'vapi=false',
  ]
)
libdex_dep = libdex_proj.get_variable('libdex_static_dep')

#sysprof_capture_proj = subproject(
#  'sysprof-capture',
#  default_options: [
#    'default_library=static',
#    'debuginfod=disabled',
#    'examples=false',
#    'gtk=false',
#    'help=false',
#    'install-static=false',
#    'libsysprof=false',
#    'polkit-agent=disabled',
#    'sysprofd=none',
#    'tests=false',
#  ]
#)
#sysprof_capture_dep = sysprof_capture_proj.get_variable('libsysprof_capture_dep')

sysprof_capture_dep = dependency('sysprof-capture-4')

muprof_deps = [
  dependency('glib-2.0'),
  dependency('gio-unix-2.0'),
  sysprof_capture_dep,
  libdex_dep,
]

executable(
  'muprof',
  muprof_sources,
  dependencies: muprof_deps,
  install: true,
)
