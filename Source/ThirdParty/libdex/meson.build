project('libdex', 'c',
          version: '0.8.1',
    meson_version: '>= 0.62.0',
  default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11', ],
)

api_version = '1'

cc = meson.get_compiler('c')
gnome = import('gnome')
pkg = import('pkgconfig')

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
libdir = join_paths(prefix, get_option('libdir'))
girdir = join_paths(datadir, 'gir-1.0')
vapidir = join_paths(datadir, 'vala', 'vapi')
typelibdir = join_paths(libdir, 'girepository-1.0')

package_name = meson.project_name()
package_string = '@0@-@1@'.format(package_name, api_version)

glib_req_version = '2.68'
glib_req = '>= @0@'.format(glib_req_version)
glib_dep = dependency('gio-2.0', version: glib_req)

if host_machine.system() == 'linux' and not get_option('liburing').disabled()
  liburing_req_version = '0.7'
  liburing_req = '>= @0@'.format(liburing_req_version)
  liburing_dep = dependency('liburing', version: liburing_req, required: get_option('liburing').enabled())

  if liburing_dep.found()
    config_h.set10('HAVE_LIBURING', true)
  endif
else
  liburing_dep = disabler()
endif

if get_option('sysprof')
  libsysprof_capture_dep = dependency('sysprof-capture-4')
  config_h.set10('HAVE_SYSPROF', true)
endif

check_headers = [
  'ucontext.h',
]
foreach h : check_headers
  if cc.has_header(h)
    config_h.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach

if host_machine.system() == 'darwin'
  # known alignment for darwin where we're using helpers
  if host_machine.cpu_family() == 'aarch64'
    config_h.set('ALIGN_OF_UCONTEXT', 16)
  else
    config_h.set('ALIGN_OF_UCONTEXT', 8)
  endif
elif host_machine.system() == 'windows'
  # Unset
else
  # Check alignment of ucontext_t
  config_h.set('ALIGN_OF_UCONTEXT', cc.alignment('ucontext_t', prefix: '#include <ucontext.h>'))
endif

project_c_args = []
test_c_args = [
  '-Watomic-alignment',
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wtrampolines',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fstrict-flex-arrays=3',
  '-fno-strict-aliasing',
  ['-Werror=format-security', '-Werror=format=2'],
  '-FImsvc_recommended_pragmas.h',
]
if get_option('buildtype') != 'plain' and get_option('stack-protector')
  if host_machine.system() != 'windows'
    test_c_args += '-fstack-protector-strong'
  endif
endif
foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    project_c_args += arg
  endif
endforeach
add_project_arguments(project_c_args, language: 'c')

# Detect and set symbol visibility
if get_option('default_library') != 'static'
  if host_machine.system() == 'windows'
    config_h.set('DLL_EXPORT', true)
    if cc.get_id() == 'msvc'
      config_h.set('_DEX_EXTERN', '__declspec(dllexport) extern')
    elif cc.has_argument('-fvisibility=hidden')
      config_h.set('_DEX_EXTERN', '__attribute__((visibility("default"))) __declspec(dllexport) extern')
    endif
  elif cc.has_argument('-fvisibility=hidden')
    config_h.set('_DEX_EXTERN', '__attribute__((visibility("default"))) extern')
  endif
endif

release_args = []
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
  '-Wl,-z,noexecstack',
]
if not get_option('buildtype').startswith('debug')
  add_global_arguments('-DG_DISABLE_CAST_CHECKS', language: 'c')
  release_args += [ '-DG_DISABLE_ASSERT' ]
  test_link_args += [
    '-Wl,-Bsymbolic',
    '-fno-plt',
  ]
endif
foreach link_arg: test_link_args
  if cc.has_link_argument(link_arg)
    global_link_args += link_arg
  endif
endforeach
add_project_link_arguments(global_link_args, language: 'c')

glib_major_version = glib_req_version.split('.')[0].to_int()
glib_minor_version = glib_req_version.split('.')[1].to_int()
if glib_minor_version % 2 == 1
  glib_minor_version = glib_minor_version + 1
endif
deprecated_c_args = [
  '-DG_DISABLE_DEPRECATED',
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(glib_major_version, glib_minor_version),
]

add_project_arguments('-I' + meson.project_build_root(), language: 'c')

functions = [
  'posix_fadvise',
  'madvise',
  'mprotect',
]

if not get_option('eventfd').disabled()
  functions += ['eventfd']
endif

foreach f : functions
  if cc.has_function(f)
    define = 'HAVE_' + f.underscorify().to_upper()
    config_h.set10(define, true)
  endif
endforeach

if get_option('eventfd').enabled() and config_h.get('HAVE_EVENTFD') == 0
  error('eventfd function is required for -Deventfd=enabled')
endif

configure_file(output: 'config.h', configuration: config_h)

subdir('src')
if get_option('tests')
  subdir('testsuite')
endif
if get_option('examples')
  subdir('examples')
endif
if get_option('docs')
  subdir('docs')
endif
