libdex_sources = [
  'dex-aio.c',
  'dex-aio-backend.c',
  'dex-async-pair.c',
  'dex-async-result.c',
  'dex-block.c',
  'dex-cancellable.c',
  'dex-channel.c',
  'dex-delayed.c',
  'dex-enums.c',
  'dex-error.c',
  'dex-fiber.c',
  'dex-future.c',
  'dex-future-set.c',
  'dex-gio.c',
  'dex-init.c',
  'dex-infinite.c',
  'dex-main-scheduler.c',
  'dex-object.c',
  'dex-platform.c',
  'dex-posix-aio-backend.c',
  'dex-posix-aio-future.c',
  'dex-promise.c',
  'dex-scheduler.c',
  'dex-semaphore.c',
  'dex-stack.c',
  'dex-static-future.c',
  'dex-thread-pool-scheduler.c',
  'dex-thread-pool-worker.c',
  'dex-thread-storage.c',
  'dex-timeout.c',
  'dex-work-queue.c',
  'dex-work-stealing-queue.c',
]

libdex_headers = [
  'dex-aio.h',
  'dex-async-pair.h',
  'dex-async-result.h',
  'dex-block.h',
  'dex-cancellable.h',
  'dex-channel.h',
  'dex-delayed.h',
  'dex-enums.h',
  'dex-error.h',
  'dex-fiber.h',
  'dex-future.h',
  'dex-future-set.h',
  'dex-gio.h',
  'dex-init.h',
  'dex-main-scheduler.h',
  'dex-object.h',
  'dex-platform.h',
  'dex-promise.h',
  'dex-scheduler.h',
  'dex-static-future.h',
  'dex-thread-pool-scheduler.h',
  'dex-timeout.h',
  'dex-version-macros.h',
  'libdex.h',
]

libdex_deps = [
  cc.find_library('atomic', required: false),
  glib_dep,
]


if liburing_dep.found()
  libdex_sources += [
    'dex-uring-aio-backend.c',
    'dex-uring-future.c',
  ]

  libdex_deps += [liburing_dep]

  liburing_version_split = liburing_dep.version().split('.')
  liburing_version_conf = configuration_data()
  liburing_version_conf.set('DEX_URING_VERSION', meson.project_version())
  liburing_version_conf.set('DEX_URING_MAJOR_VERSION', liburing_version_split[0])
  liburing_version_conf.set('DEX_URING_MINOR_VERSION', liburing_version_split[1])

  configure_file(
            input: 'dex-uring-version.h.in',
           output: 'dex-uring-version.h',
    configuration: liburing_version_conf,
  )
endif

if host_machine.system() != 'windows'
  # Not really used on Windows now, see also
  # https://github.com/mesonbuild/meson/issues/4366
  libdex_sources += [
    'dex-unix-signal.c',
    'dex-ucontext.c',
  ]

  # If we're on Linux and mips we might still need asm.S.
  # But otherwise linux can do it all without any custom
  # assembler. Failure to do this might result in CET being
  # disabled for the process by GCC.
  if ((host_machine.system() != 'linux' or host_machine.cpu_family() == 'mips') and
      host_machine.system() != 'darwin')
    libdex_sources += ['asm.S']
  endif

  libdex_headers += ['dex-unix-signal.h']
endif

version_split = meson.project_version().split('.')
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', version_split[0])
version_conf.set('MINOR_VERSION', version_split[1])
version_conf.set('MICRO_VERSION', version_split[2])

configure_file(
          input: 'dex-version.h.in',
         output: 'dex-version.h',
  configuration: version_conf,
        install: true,
    install_dir: join_paths(get_option('includedir'), 'libdex-@0@'.format(api_version))
)

if get_option('sysprof')
  libdex_deps += [libsysprof_capture_dep]
endif

libdex_c_args = [
  deprecated_c_args,
  release_args,
  '-DG_LOG_DOMAIN="Dex"',
  '-DDEX_COMPILATION',
]

if host_machine.system() == 'darwin'
  libdex_c_args += [
    # needed for setcontext/makecontext/swapcontext
    '-D_XOPEN_SOURCE',
  ]
endif

libdex_static = static_library('dex-@0@'.format(api_version), libdex_sources,
           dependencies: libdex_deps,
  gnu_symbol_visibility: 'hidden',
                 c_args: libdex_c_args,
)

libdex_static_dep = declare_dependency(
           link_whole: libdex_static,
  include_directories: include_directories('.'),
         dependencies: libdex_deps,
)

libdex = shared_library('dex-@0@'.format(api_version),
           dependencies: libdex_deps + [libdex_static_dep],
                 c_args: libdex_c_args,
                install: true,
  gnu_symbol_visibility: 'hidden',
                version: '@0@.0.0'.format(api_version),
        darwin_versions: '1.0',
)

libdex_dep_sources = []

install_headers(libdex_headers, subdir: 'libdex-@0@'.format(api_version))

pkg.generate(
  description: 'Future-based programming for GLib-based applications and libraries',
    libraries: libdex,
         name: 'libdex',
     filebase: 'libdex-' + api_version,
      subdirs: 'libdex-@0@'.format(api_version),
     requires: ['gio-2.0'],
)

if get_option('introspection').enabled()
  libdex_gir = gnome.generate_gir(libdex,
                sources: [libdex_sources, libdex_headers],
              nsversion: api_version,
              namespace: 'Dex',
        export_packages: 'libdex-@0@'.format(api_version),
          symbol_prefix: 'dex',
      identifier_prefix: 'Dex',
        install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
               includes: ['Gio-2.0'],
                install: true,
                 header: 'libdex.h',
             extra_args: ['-DDEX_COMPILATION'],
  )

  libdex_dep_sources += [libdex_gir]

  if get_option('vapi')
    dex_vapi = gnome.generate_vapi('libdex-@0@'.format(api_version),
            sources: libdex_gir.get(0),
            install: true,
        install_dir: join_paths(datadir, 'vala', 'vapi'),
           packages: ['gio-2.0'],
    )
  endif
endif

libdex_dep = declare_dependency(
            link_with: libdex,
  include_directories: include_directories('.'),
         dependencies: libdex_deps,
              sources: [libdex_dep_sources],
)

meson.override_dependency('libdex-' + api_version, libdex_dep)
