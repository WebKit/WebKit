{
  'includes': [
    '../../../WTF/WTF.gypi',
  ],
  'target_defaults' : {
      'cflags' : [ '<@(global_cflags)', ],
      'defines': [ '<@(global_defines)' ],
  },
  'targets': [
    {
      'target_name': 'wtf',
      'type': 'static_library',
      'dependencies': [
        'Dependencies.gyp:icu',
       ],
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
      'cflags' : [ '-fPIC', ],
      'all_dependent_settings': {
        'cflags' : [
          '<(global_cflags)',
         ],
        'include_dirs': [
          '<(Source)/WTF',
          '<(Source)/WTF/wtf',
        ],
      },
      'direct_dependent_settings': {
        'ldflags': [ '-pthread' ],
      },
    },
  ]
}
