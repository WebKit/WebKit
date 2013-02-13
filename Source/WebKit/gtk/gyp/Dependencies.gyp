{
  'targets': [
    {
      'target_name': 'glib',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(GLIB_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(GLIB_LIBS)' ],
        },
      },
    },
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
