vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': 'ce95e5d83f3538e5797b85f986e52afda8a7a40a',
  'swarming_revision': '11e31afa5d330756ff87aa12064bb5d032896cb5',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': '63f24c8221a229f677d26ebe8f3d1528a9d787ac',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': 'e8775f0f64bcae6b0789df6d4a2a5aca9f5cf4ac',
}

deps = {
  'src/build':
    Var('chromium_git') + '/chromium/src/build' + '@' + 'bca1cbe2aaed610e412eed70a3a7d22799c65259',
  'src/buildtools':
    Var('chromium_git') + '/chromium/buildtools.git' + '@' + '88811f48a6b79786ef35be86825642fc33011151',
  'src/testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + '7b3e681f96c5bc54d1ba4a47285a9874ed18514f',
  'src/testing/gtest':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + '6f8a66431cb592dad629028a50b3dd418a408c87',
  'src/testing/gmock':
    Var('chromium_git') + '/external/googlemock.git' + '@' + '0421b6f358139f02e102c9c332ce19a33faf75be', # from svn revision 566
  'src/third_party':
    Var('chromium_git') + '/chromium/src/third_party' + '@' + 'b28b3325f9374dd79d3a874d6dd688222d61f4f1',
  'src/third_party/catapult':
   Var('chromium_git') + '/external/github.com/catapult-project/catapult.git' + '@' + Var('catapult_revision'),
  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + '7260e4d8b8e1e40b17f03fafdf1cd83296900f76',
  'src/third_party/yasm/source/patched-yasm':
    Var('chromium_git') + '/chromium/deps/yasm/patched-yasm.git' + '@' + '7da28c6c7c6a1387217352ce02b31754deb54d2a',
  'src/tools':
    Var('chromium_git') + '/chromium/src/tools' + '@' + '5c327d115e2022cbeb2e8f51f3a0b4dbcb1dd451',
  'src/tools/gyp':
    Var('chromium_git') + '/external/gyp.git' + '@' + 'eb296f67da078ec01f5e3a9ea9cdc6d26d680161',
   'src/tools/swarming_client':
     Var('chromium_git') + '/external/swarming.client.git' + '@' +  Var('swarming_revision'),

  # libyuv-only dependencies (not present in Chromium).
  'src/third_party/gflags':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gflags' + '@' + '892576179b45861b53e04a112996a738309cf364',
  'src/third_party/gflags/src':
    Var('chromium_git') + '/external/github.com/gflags/gflags' + '@' + '03bebcb065c83beff83d50ae025a55a4bf94dfca',
  'src/third_party/gtest-parallel':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gtest-parallel' + '@' + '1dad0e9f6d82ff994130b529d7d814b40eb32b0e',
}

deps_os = {
  'android': {
    'src/base':
      Var('chromium_git') + '/chromium/src/base' + '@' + 'f6489f4fd2be9f4245492fd4a708740bba073f13',
    'src/third_party/android_tools':
      Var('chromium_git') + '/android_tools.git' + '@' + 'b65c4776dac2cf1b80e969b3b2d4e081b9c84f29',
    'src/third_party/ced/src':
      Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + 'e21eb6aed10b9f6e2727f136c52420033214d458',
    'src/third_party/icu':
      Var('chromium_git') + '/chromium/deps/icu.git' + '@' + 'b34251f8b762f8e2112a89c587855ca4297fed96',
    'src/third_party/jsr-305/src':
      Var('chromium_git') + '/external/jsr-305.git' + '@' + '642c508235471f7220af6d5df2d3210e3bfc0919',
    'src/third_party/junit/src':
      Var('chromium_git') + '/external/junit.git' + '@' + '64155f8a9babcfcf4263cf4d08253a1556e75481',
    'src/third_party/lss':
      Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
    'src/third_party/mockito/src':
      Var('chromium_git') + '/external/mockito/mockito.git' + '@' + 'de83ad4598ad4cf5ea53c69a8a8053780b04b850',
    'src/third_party/requests/src':
      Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'f172b30356d821d180fa4ecfa3e71c7274a32de4',
    'src/third_party/robolectric/robolectric':
      Var('chromium_git') + '/external/robolectric.git' + '@' + '2a0b6ba221c14f3371813a676ce06143353e448d',
  },
  'ios': {
    'src/ios':
      Var('chromium_git') + '/chromium/src/ios' + '@' + '9595ed7ed6ab7bcb3b1f990a4455f56d6ed01ef3',
  },
  'unix': {
    'src/third_party/lss':
      Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
  },
  'win': {
    # Dependencies used by libjpeg-turbo
    'src/third_party/yasm/binaries':
      Var('chromium_git') + '/chromium/deps/yasm/binaries.git' + '@' + '52f9b3f4b0aa06da24ef8b123058bb61ee468881',
  },
}

# Define rules for which include paths are allowed in our source.
include_rules = [ '+gflags' ]

pre_deps_hooks = [
  {
    # Remove any symlinks from before 177567c518b121731e507e9b9c4049c4dc96e4c8.
    # TODO(kjellander): Remove this in March 2017.
    'name': 'cleanup_links',
    'pattern': '.',
    'action': ['python', 'src/cleanup_links.py'],
  },
]

hooks = [
  {
    # This clobbers when necessary (based on get_landmines.py). It should be
    # an early hook but it will need to be run after syncing Chromium and
    # setting up the links, so the script actually exists.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python',
        'src/build/landmines.py',
        '--landmine-scripts',
        'src/tools_libyuv/get_landmines.py',
        '--src-dir',
        'src',
    ],
  },
  # Android dependencies. Many are downloaded using Google Storage these days.
  # They're copied from https://cs.chromium.org/chromium/src/DEPS for all
  # such dependencies we share with Chromium.
  {
    # This downloads SDK extras and puts them in the
    # third_party/android_tools/sdk/extras directory.
    'name': 'sdkextras',
    'pattern': '.',
    # When adding a new sdk extras package to download, add the package
    # directory and zip file to .gitignore in third_party/android_tools.
    'action': ['python',
               'src/build/android/play_services/update.py',
               'download'
    ],
  },
  {
    'name': 'intellij',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-intellij',
               '-l', 'third_party/intellij'
    ],
  },
  {
    'name': 'javax_inject',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-javax-inject',
               '-l', 'third_party/javax_inject'
    ],
  },
  {
    'name': 'hamcrest',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-hamcrest',
               '-l', 'third_party/hamcrest'
    ],
  },
  {
    'name': 'guava',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-guava',
               '-l', 'third_party/guava'
    ],
  },
  {
    'name': 'android_support_test_runner',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-android-support-test-runner',
               '-l', 'third_party/android_support_test_runner'
    ],
  },
  {
    'name': 'byte_buddy',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-byte-buddy',
               '-l', 'third_party/byte_buddy'
    ],
  },
  {
    'name': 'espresso',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-espresso',
               '-l', 'third_party/espresso'
    ],
  },
  {
    'name': 'robolectric_libs',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-robolectric',
               '-l', 'third_party/robolectric'
    ],
  },
  {
    'name': 'apache_velocity',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-apache-velocity',
               '-l', 'third_party/apache_velocity'
    ],
  },
  {
    'name': 'ow2_asm',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-ow2-asm',
               '-l', 'third_party/ow2_asm'
    ],
  },
  {
    'name': 'icu4j',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-icu4j',
               '-l', 'third_party/icu4j'
    ],
  },
  {
    'name': 'accessibility_test_framework',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-accessibility-test-framework',
               '-l', 'third_party/accessibility_test_framework'
    ],
  },
  {
    'name': 'bouncycastle',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-bouncycastle',
               '-l', 'third_party/bouncycastle'
    ],
  },
  {
    'name': 'sqlite4java',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-sqlite4java',
               '-l', 'third_party/sqlite4java'
    ],
  },
  {
    'name': 'objenesis',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-objenesis',
               '-l', 'third_party/objenesis'
    ],
  },
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds or cross compiling.
    'name': 'sysroot',
    'pattern': '.',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--running-as-hook'],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'src/build/vs_toolchain.py', 'update'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'src/third_party/binutils',
    'action': [
        'python',
        'src/third_party/binutils/download.py',
    ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'src/tools/clang/scripts/update.py', '--if-needed'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE'],
  },
  # Pull GN binaries.
  {
    'name': 'gn_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'src/buildtools/linux64/gn.sha1',
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'src/buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull luci-go binaries (isolate, swarming) using checked-in hashes.
  {
    'name': 'luci-go_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/win64',
    ],
  },
  {
    'name': 'luci-go_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/mac64',
    ],
  },
  {
    'name': 'luci-go_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'src/tools/luci-go/linux64',
    ],
  },
  {
    'name': 'clang_format_merge_driver',
    'pattern': '.',
    'action': [ 'python',
                'src/tools/clang_format_merge_driver/install_git_hook.py',
    ],
  },
]

recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi.
  'src/buildtools',
  # android_tools manages the NDK.
  'src/third_party/android_tools',
]
