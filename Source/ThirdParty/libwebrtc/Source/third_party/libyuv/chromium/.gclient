solutions = [{
  'name': 'src',
  'url': 'https://chromium.googlesource.com/chromium/src.git',
  'deps_file': '.DEPS.git',
  'managed': False,
  'custom_deps': {
    # Skip syncing some large dependencies Libyuv will never need.
    'src/third_party/cld_2/src': None,
    'src/third_party/ffmpeg': None,
    'src/third_party/hunspell_dictionaries': None,
    'src/third_party/liblouis/src': None,
    'src/third_party/pdfium': None,
    'src/third_party/skia': None,
    'src/third_party/trace-viewer': None,
    'src/third_party/webrtc': None,
  },
  'safesync_url': ''
}]

cache_dir = None
