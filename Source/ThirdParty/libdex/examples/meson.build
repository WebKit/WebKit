libgio_unix_dep = dependency('gio-unix-2.0', required: false, disabler: true)
libsoup_dep = dependency('libsoup-3.0', required: false, disabler: true)

examples = {
            'cat': {'dependencies': libgio_unix_dep},
        'cat-aio': {},
             'cp': {},
     'echo-bench': {},
           'host': {},
          'httpd': {'dependencies': libsoup_dep},
  'infinite-loop': {},
       'tcp-echo': {},
           'wget': {'dependencies': libsoup_dep},
}

foreach example, params: examples
  example_exe = executable(example, '@0@.c'.format(example),
          c_args: deprecated_c_args,
    dependencies: [libdex_static_dep, params.get('dependencies', [])],
         install: false,
  )
endforeach
