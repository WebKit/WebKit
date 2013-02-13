{
  'targets': [
    {
      'target_name': 'icu',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(UNICODE_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(UNICODE_LIBS)' ],
        },
      },
    },
  ],
}
