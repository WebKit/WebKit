vars = {
  'android_git': 'https://android.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',
}

deps = {

  'build':
    Var('chromium_git') + '/chromium/src/build.git' + '@' + '8a1537f019e55807420ae76c62e4ce8f56fedca1',

  'buildtools':
    Var('chromium_git') + '/chromium/buildtools.git' + '@' + '461b345a815c1c745ac0534a6a4bd52d123abe68',

  'testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + '6dfa36ab2e5143fa2f7353e3af5d2935af2e61f7',

  # Cherry is a dEQP management GUI written in Go. We use it for viewing test results.
  'third_party/cherry':
    Var('android_git') + '/platform/external/cherry' + '@' + '4f8fb08d33ca5ff05a1c638f04c85bbb8d8b52cc',

  'third_party/deqp/src':
    Var('android_git') + '/platform/external/deqp' + '@' + '455d82c60b096e7bd83b6a2f5ed70c61e4bfa759',

  'third_party/glslang-angle/src':
    Var('android_git') + '/platform/external/shaderc/glslang' + '@' + '1e275c8486325aaab34734ad9a650c0121c5efdb',

  'third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + 'd175c8bf823e709d570772b038757fadf63bc632',

  'third_party/libpng/src':
    Var('android_git') + '/platform/external/libpng' + '@' + '094e181e79a3d6c23fd005679025058b7df1ad6c',

  'third_party/spirv-headers/src':
    Var('android_git') + '/platform/external/shaderc/spirv-headers' + '@' + 'c470b68225a04965bf87d35e143ae92f831e8110',

  'third_party/spirv-tools-angle/src':
    Var('android_git') + '/platform/external/shaderc/spirv-tools' + '@' + '68c5f0436f1d4f1f137e608780190865d0b193ca',

  'third_party/vulkan-validation-layers/src':
    Var('android_git') + '/platform/external/vulkan-validation-layers' + '@' + 'f47c534fee2f26f6b783209d56e0ade48e30eb8d',

  'third_party/zlib':
    Var('chromium_git') + '/chromium/src/third_party/zlib' + '@' + '24ab14872e8e068ba08cc31cc3d43bcc6d5cb832',

  'tools/clang':
    Var('chromium_git') + '/chromium/src/tools/clang.git' + '@' + 'e70074db10b27867e6c873adc3ac7e5f9ee0ff6e',

  'tools/gyp':
    Var('chromium_git') + '/external/gyp' + '@' + '5e2b3ddde7cda5eb6bc09a5546a76b00e49d888f',
}

hooks = [
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/linux64/clang-format.sha1',
    ],
  },
  # Pull GN binaries using checked-in hashes.
  {
    'name': 'gn_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'buildtools/linux64/gn.sha1',
    ],
  },
  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'tools/clang/scripts/update.py', '--if-needed'],
  },

  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },

  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    'pattern': '.',
    'action': ['python', 'gyp/gyp_angle'],
  },
]

recursedeps = [
  # buildtools provides clang_format.
  'buildtools',
]
