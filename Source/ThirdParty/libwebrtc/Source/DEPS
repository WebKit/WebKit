# This file contains dependencies for WebRTC.

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': 'ba61076b34f386bdebc8a43f5254a4506396d161',
  'boringssl_git': 'https://boringssl.googlesource.com',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarming_client
  # and whatever else without interference from each other.
  'swarming_revision': 'af6b06ca68ba7a618024f28856418296a9acf375',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling openmax_dl
  # and whatever else without interference from each other.
  'openmax_dl_revision': '7acede9c039ea5d14cf326f44aad1245b9e674a7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  'boringssl_revision': '24e5886c0edfc409c8083d10f9f1120111efd6f5',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': '63f24c8221a229f677d26ebe8f3d1528a9d787ac',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': 'ee4b073833a40a8ebaf58ea3f6eff8195884bc64',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libFuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': '16f5f743c188c836d32cdaf349d5d3effb8a3518',
}
deps = {
  # TODO(kjellander): Move this to be Android-only once the libevent dependency
  # in base/third_party/libevent is solved.
  'src/base':
    Var('chromium_git') + '/chromium/src/base' + '@' + '2c8c1581129db665d283bcd6ad0baa16b0244338',
  'src/build':
    Var('chromium_git') + '/chromium/src/build' + '@' + '8bb5c119b92a304a00485b2209d436ddc1cb2818',
  'src/buildtools':
    Var('chromium_git') + '/chromium/buildtools.git' + '@' + 'ee9c3a70889f452fb0cf10792ba3cb26de2d071e',
  'src/testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + 'c2b21cd5d749944177448ac155c6ce4c12bacc8a',
  'src/third_party':
    Var('chromium_git') + '/chromium/src/third_party' + '@' + '09c529dd13714dc7874b31e938e6bc418509ea9e',
  'src/third_party/boringssl/src':
   Var('boringssl_git') + '/boringssl.git' + '@' +  Var('boringssl_revision'),
  'src/third_party/catapult':
   Var('chromium_git') + '/external/github.com/catapult-project/catapult.git' + '@' + Var('catapult_revision'),
  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/ffmpeg':
    Var('chromium_git') + '/chromium/third_party/ffmpeg.git' + '@' + '06ac9ea361fa8d48916b83783bb7f36872388cc2',
  'src/third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + '42bc671f47b122fad36db5eccbc06868afdf7862',
  'src/third_party/jsoncpp/source':
    Var('chromium_git') + '/external/github.com/open-source-parsers/jsoncpp.git' + '@' + 'f572e8e42e22cfcf5ab0aea26574f408943edfa4', # from svn 248
  # Used for building libFuzzers (only supports Linux).
  'src/third_party/libFuzzer/src':
    Var('chromium_git') + '/chromium/llvm-project/llvm/lib/Fuzzer.git' + '@' +  Var('libfuzzer_revision'),
  'src/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + 'a1750dbc79a8792dde3d3f7d7d8ac28ba01ac9dd',
  'src/third_party/libsrtp':
   Var('chromium_git') + '/chromium/deps/libsrtp.git' + '@' + 'ccf84786f8ef803cb9c75e919e5a3976b9f5a672',
  'src/third_party/libvpx/source/libvpx':
    Var('chromium_git') + '/webm/libvpx.git' + '@' +  '8d391a111a4830f946f6fe8b0cf51be8661224d9',
  'src/third_party/libyuv':
    Var('chromium_git') + '/libyuv/libyuv.git' + '@' + '7bffe5e1c54bc22daebd57003735e61693719ac6',
  'src/third_party/openh264/src':
    Var('chromium_git') + '/external/github.com/cisco/openh264' + '@' + '0fd88df93c5dcaf858c57eb7892bd27763f0f0ac',
  'src/third_party/openmax_dl':
    Var('chromium_git') + '/external/webrtc/deps/third_party/openmax.git' + '@' +  Var('openmax_dl_revision'),
  'src/third_party/usrsctp/usrsctplib':
    Var('chromium_git') + '/external/github.com/sctplab/usrsctp' + '@' + '2f6478eb8d40f1766a96b5b033ed26c0c2244589',
  'src/third_party/yasm/source/patched-yasm':
    Var('chromium_git') + '/chromium/deps/yasm/patched-yasm.git' + '@' + '7da28c6c7c6a1387217352ce02b31754deb54d2a',
  'src/tools':
    Var('chromium_git') + '/chromium/src/tools' + '@' + '7d5c45370bb6d140ba7e86f40ea2ff7ab8c6d408',
  'src/tools/gyp':
    Var('chromium_git') + '/external/gyp.git' + '@' + 'eb296f67da078ec01f5e3a9ea9cdc6d26d680161',
   'src/tools/swarming_client':
     Var('chromium_git') + '/external/swarming.client.git' + '@' +  Var('swarming_revision'),
  # WebRTC-only dependencies (not present in Chromium).
  'src/third_party/gflags':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gflags' + '@' + '892576179b45861b53e04a112996a738309cf364',
  'src/third_party/gflags/src':
    Var('chromium_git') + '/external/github.com/gflags/gflags' + '@' + '03bebcb065c83beff83d50ae025a55a4bf94dfca',
  'src/third_party/gtest-parallel':
    Var('chromium_git') + '/external/github.com/google/gtest-parallel' + '@' + '4bf9c03d93965f7a5b5ab0c773536fafacaac847',
}
deps_os = {
  'android': {
    'src/third_party/android_tools':
      Var('chromium_git') + '/android_tools.git' + '@' + '023e2f65409a2b7886b8d644d6a88542ead6cd0a',
    'src/third_party/ced/src':
      Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + '910cca22d881b02cbc8950fa02ccbcdcfb782456',
    'src/third_party/icu':
      Var('chromium_git') + '/chromium/deps/icu.git' + '@' + 'dfa798fe694702b43a3debc3290761f22b1acaf8',
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
    'src/third_party/ub-uiautomator/lib':
      Var('chromium_git') + '/chromium/third_party/ub-uiautomator.git' + '@' + '00270549ce3161ae72ceb24712618ea28b4f9434',
    # Gradle 3.5.0. Used for testing Android Studio project generation for WebRTC.
    'src/webrtc/examples/androidtests/third_party/gradle':
      Var('chromium_git') + '/external/github.com/gradle/gradle.git' + '@' +
      'b762622a185d59ce0cfc9cbc6ab5dd22469e18a6',
  },
  'ios': {
    'src/ios':
      Var('chromium_git') + '/chromium/src/ios' + '@' + '34147146059e812adc220add9895ffa5ae228fd3',
  },
  'unix': {
    'src/third_party/lss':
      Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
  },
  'win': {
    # Dependencies used by libjpeg-turbo
    'src/third_party/yasm/binaries':
      Var('chromium_git') + '/chromium/deps/yasm/binaries.git' + '@' + '52f9b3f4b0aa06da24ef8b123058bb61ee468881',
    # WebRTC-only dependency (not present in Chromium).
    'src/third_party/winsdk_samples':
      Var('chromium_git') + '/external/webrtc/deps/third_party/winsdk_samples_v71' + '@' + '2d31a1cbecc86359e6ec041fb9ff6c082babd073',
  },
}
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
    # Check for legacy named top-level dir (named 'trunk').
    'name': 'check_root_dir_name',
    'pattern': '.',
    'action': ['python','-c',
               ('import os,sys;'
                'script = os.path.join("trunk","check_root_dir.py");'
                '_ = os.system("%s %s" % (sys.executable,script)) '
                'if os.path.exists(script) else 0')],
  },
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
        'src/tools_webrtc/get_landmines.py',
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
    'name': 'xstream',
    'pattern': '.',
    'action': ['python',
               'src/build/android/update_deps/update_third_party_deps.py',
               'download',
               '-b', 'chromium-robolectric',
               '-l', 'third_party/xstream'
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
  # Pull the Syzygy binaries, used for optimization and instrumentation.
  {
    'name': 'syzygy-binaries',
    'pattern': '.',
    'action': ['python',
               'src/build/get_syzygy_binaries.py',
               '--output-dir=src/third_party/syzygy/binaries',
               '--revision=a8456d9248a126881dcfb8707ca7dcdae56e1ac7',
               '--overwrite',
    ],
  },
  {
    # Pull sanitizer-instrumented third-party libraries if requested via
    # GYP_DEFINES.
    # See src/third_party/instrumented_libraries/scripts/download_binaries.py.
    # TODO(kjellander): Update comment when GYP is completely cleaned up.
    'name': 'instrumented_libraries',
    'pattern': '\\.sha1',
    'action': ['python', 'src/third_party/instrumented_libraries/scripts/download_binaries.py'],
  },
  {
    # Download test resources, i.e. video and audio files from Google Storage.
    'pattern': '.',
    'action': ['download_from_google_storage',
               '--directory',
               '--recursive',
               '--num_threads=10',
               '--no_auth',
               '--quiet',
               '--bucket', 'chromium-webrtc-resources',
               'src/resources'],
  },
]
recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi.
  'src/buildtools',
  # android_tools manages the NDK.
  'src/third_party/android_tools',
]
