# Copyright (c) 2015, Google Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
}

deps = {
  'boringssl/util/bot/gyp':
    Var('chromium_git') + '/external/gyp.git' + '@' + 'eb296f67da078ec01f5e3a9ea9cdc6d26d680161',
}

deps_os = {
  'android': {
    'boringssl/util/bot/android_tools':
      Var('chromium_git') + '/android_tools.git' + '@' + 'cb6bc21107001e2f2eeee2707b482b2b755baf51',
  },
  'unix': {
    'boringssl/util/bot/libFuzzer':
      Var('chromium_git') + '/chromium/llvm-project/llvm/lib/Fuzzer.git' + '@' + '16f5f743c188c836d32cdaf349d5d3effb8a3518',
  },
}

recursedeps = [
  # android_tools pulls in the NDK from a separate repository.
  'boringssl/util/bot/android_tools',
]

hooks = [
  {
    'name': 'cmake_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-tools',
                '-s', 'boringssl/util/bot/cmake-linux64.tar.gz.sha1',
    ],
  },
  {
    'name': 'cmake_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-tools',
                '-s', 'boringssl/util/bot/cmake-mac.tar.gz.sha1',
    ],
  },
  {
    'name': 'cmake_win32',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-tools',
                '-s', 'boringssl/util/bot/cmake-win32.zip.sha1',
    ],
  },
  {
    'name': 'perl_win32',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-tools',
                '-s', 'boringssl/util/bot/perl-win32.zip.sha1',
    ],
  },
  {
    'name': 'yasm_win32',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-tools',
                '-s', 'boringssl/util/bot/yasm-win32.exe.sha1',
    ],
  },
  {
    'name': 'win_toolchain',
    'pattern': '.',
    'action': [ 'python',
                'boringssl/util/bot/vs_toolchain.py',
                'update',
    ],
  },
  {
    'name': 'clang',
    'pattern': '.',
    'action': [ 'python',
                'boringssl/util/bot/update_clang.py',
    ],
  },
  {
    'name': 'cmake_linux64_extract',
    'pattern': '.',
    'action': [ 'python',
                'boringssl/util/bot/extract.py',
                'boringssl/util/bot/cmake-linux64.tar.gz',
                'boringssl/util/bot/cmake-linux64/',
    ],
  },
  {
    'name': 'cmake_mac_extract',
    'pattern': '.',
    'action': [ 'python',
                'boringssl/util/bot/extract.py',
                'boringssl/util/bot/cmake-mac.tar.gz',
                'boringssl/util/bot/cmake-mac/',
    ],
  },
  {
    'name': 'cmake_win32_extract',
    'pattern': '.',
    'action': [ 'python',
                'boringssl/util/bot/extract.py',
                'boringssl/util/bot/cmake-win32.zip',
                'boringssl/util/bot/cmake-win32/',
    ],
  },
  {
    'name': 'perl_win32_extract',
    'pattern': '.',
    'action': [ 'python',
                'boringssl/util/bot/extract.py',
                '--no-prefix',
                'boringssl/util/bot/perl-win32.zip',
                'boringssl/util/bot/perl-win32/',
    ],
  },
]
