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
    {
      'target_name': 'atspi',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(ATSPI2_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(ATSPI2_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'cairo',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(CAIRO_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(CAIRO_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'dlopen',
      'type': 'none',
      'direct_dependent_settings': {
        'link_settings': {
          'libraries' : [ '<@(DLOPEN_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'enchant',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(ENCHANT_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(ENCHANT_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'freetype',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(FREETYPE_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(FREETYPE_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'gail',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(GAIL_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(GAIL_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'gamepad',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(GAMEPAD_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(GAMEPAD_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'gstreamer',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(GSTREAMER_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(GSTREAMER_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'gtk2',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(GTK2_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(GTK2_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'gtk',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(GTK_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(GTK_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'gtk_unix_printing',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(GTK_UNIX_PRINTING_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(GTK_UNIX_PRINTING_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'jpeg',
      'type': 'none',
      'direct_dependent_settings': {
        'link_settings': { 'libraries' : [ '<@(JPEG_LIBS)' ], },
      },
    },
    {
      'target_name': 'libsecret',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(LIBSECRET_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(LIBSECRET_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'libsoup',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(LIBSOUP_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(LIBSOUP_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'libxml',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(LIBXML_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(LIBXML_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'libxslt',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(LIBXSLT_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(LIBXSLT_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'ole32',
      'type': 'none',
      'direct_dependent_settings': {
        'link_settings': {
          'libraries' : [ '<@(OLE32_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'pango',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(PANGO_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(PANGO_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'png',
      'type': 'none',
      'direct_dependent_settings': {
        'link_settings': {
          'libraries' : [ '<@(PNG_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'shwlapi',
      'type': 'none',
      'direct_dependent_settings': {
        'link_settings': {
          'libraries' : [ '<@(SHLWAPI_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'sqlite',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(SQLITE3_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(SQLITE3_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'webp',
      'type': 'none',
      'direct_dependent_settings': {
        'link_settings': {
          'libraries' : [ '<@(WEBP_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'winmm',
      'type': 'none',
      'direct_dependent_settings': {
        'link_settings': {
          'libraries' : [ '<@(WINMM_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'xcomposite',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(XCOMPOSITE_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(XCOMPOSITE_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'xdamage',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(XDAMAGE_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(XDAMAGE_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'xrender',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(XRENDER_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(XRENDER_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'xt',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(XT_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(XT_LIBS)' ],
        },
      },
    },
    {
      'target_name': 'zlib',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [ '<@(ZLIB_CFLAGS)' ],
        'link_settings': {
          'libraries' : [ '<@(ZLIB_LIBS)' ],
        },
      },
    },
  ],
}
