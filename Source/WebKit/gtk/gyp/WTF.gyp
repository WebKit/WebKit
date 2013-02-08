{
  'includes': [
    'Configuration.gypi',
    '../../../WTF/WTF.gypi',
  ],
  'targets': [
    {
      'target_name': 'glib',
      'type': 'none',
      'variables': {
        'glib_packages': 'gmodule-2.0 gobject-2.0 gthread-2.0 gio-2.0',
      },
      'direct_dependent_settings': {
         'cflags': [ '<!@(pkg-config --cflags <(glib_packages))', ],
         'link_settings': {
           'ldflags': [ '<!@(pkg-config --libs-only-L --libs-only-other <(glib_packages))', ],
           'libraries': [ '<!@(pkg-config --libs-only-l <(glib_packages))', ],
         },
       },
    },
    {
      'target_name': 'icu',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<!@(icu-config --cppflags)', ],
        'link_settings': {
          'ldflags': [ '<!@(icu-config --ldflags-libsonly)', ],
        },
       },
    },
    {
      'target_name': 'wtf',
      'type': 'static_library',
      'dependencies': [ 'glib', 'icu' ],
      'include_dirs': [
        '<(Source)/WTF',
        '<(Source)/WTF/wtf',
        '<(Source)/WTF/wtf/unicode',
      ],
      'sources': [
        '<@(wtf_privateheader_files)',
        '<@(wtf_files)',
      ],
      'sources/': [
        ['exclude', 'wtf/efl'],
        ['exclude', 'wtf/chromium'],
        ['exclude', 'wtf/mac'],
        ['exclude', 'wtf/qt'],
        ['exclude', 'wtf/url'],
        ['exclude', 'wtf/wince'],
        ['exclude', 'wtf/wx'],
        ['exclude', 'wtf/unicode/qt4'],
        ['exclude', 'wtf/unicode/wchar'],
        ['exclude', '(Default|Wchar|Mac|None|Qt|Win|Wx|Efl)\\.(cpp|mm)$'],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(Source)/WTF',
          '<(Source)/WTF/wtf',
        ],
        'ldflags': [ '-pthread' ],
      },
      'cflags': [ '-fPIC' ],
      'defines': [ '<@(default_defines)' ],
    },
  ]
}
