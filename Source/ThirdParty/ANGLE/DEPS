vars = {
  'android_git': 'https://android.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',

  # This variable is set on the Chrome infra for compatiblity with gclient.
  'angle_root': '.',

  # This variable is overrided in Chromium's DEPS file.
  'build_with_chromium': False,

  # Current revision of dEQP.
  'deqp_revision': '66a49e0a43f7af654ee1de8a3b1bcaf6c0d14aa4',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': '86c72c9486a97da534f97e56b8f7ae06cd1b580b',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '2434b89345a50c018c84f42a310b0fad4f3fd94f',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'c8b09744c6a15f3586c26351376bf5c6f656e89f',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': '982f0f84dccf6f281b48318c77261a9028000126',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '2f0abfcf9eb04018e6e92125a70bc28665aa17fe',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': 'f392e71b994036c92b896c2a62cc63d042b7f9b1',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': 'ff80a937c8a505abbdddb95d8ffaa446820c8391',
}

deps = {

  '{angle_root}/build': {
    'url': '{chromium_git}/chromium/src/build.git@54ea0e7fd122348de2f73ac21d1b6eafb9b78969',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/buildtools': {
    'url': '{chromium_git}/chromium/src/buildtools.git@d5c58b84d50d256968271db459cd29b22bff1ba2',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/testing': {
    'url': '{chromium_git}/chromium/src/testing@32e614b7ec7b2b741351c1b8470aaf30c2f532fa',
    'condition': 'not build_with_chromium',
  },

  # Cherry is a dEQP management GUI written in Go. We use it for viewing test results.
  '{angle_root}/third_party/cherry': {
    'url': '{android_git}/platform/external/cherry@4f8fb08d33ca5ff05a1c638f04c85bbb8d8b52cc',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/deqp/src': {
    'url': '{chromium_git}/external/deqp@{deqp_revision}',
  },

  '{angle_root}/third_party/fuchsia-sdk': {
    'url': '{chromium_git}/chromium/src/third_party/fuchsia-sdk.git@8e8db13b538ecb251e5ce9d5c781fc142f9752fd',
    'condition': 'checkout_fuchsia and not build_with_chromium',
  },

  # glmark2 is a GPL3-licensed OpenGL ES 2.0 benchmark. We use it for testing.
  '{angle_root}/third_party/glmark2/src': {
    'url': '{chromium_git}/external/github.com/glmark2/glmark2@c4b3ff5a481348e8bdc2b71ee275864db91e40b1',
  },

  '{angle_root}/third_party/glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@{glslang_revision}',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/googletest': {
    'url': '{chromium_git}/chromium/src/third_party/googletest@d5024103c8a8977156ee800eb1c84d92dffe9fdf',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/googletest/src': {
    'url': '{chromium_git}/external/github.com/google/googletest.git@9997a830ee5589c2da79198bc3b60d1c47e50118',
    'condition': 'not build_with_chromium',
  },

  # libjpeg_turbo is used by glmark2.
  '{angle_root}/third_party/libjpeg_turbo': {
    'url': '{chromium_git}/chromium/deps/libjpeg_turbo@6dcdade8828297e306cabfdae80f3510f3f3eea2',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/libpng/src': {
    'url': '{android_git}/platform/external/libpng@094e181e79a3d6c23fd005679025058b7df1ad6c',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/jsoncpp': {
    'url': '{chromium_git}/chromium/src/third_party/jsoncpp@fd0ac8ce63a47e99b71a58f1489136fbb19c9137',
    'condition': 'not build_with_chromium',
   },

  '{angle_root}/third_party/jsoncpp/source': {
    'url' : '{chromium_git}/external/github.com/open-source-parsers/jsoncpp@f572e8e42e22cfcf5ab0aea26574f408943edfa4',
    'condition': 'not build_with_chromium',
   },

  '{angle_root}/third_party/Python-Markdown': {
    'url': '{chromium_git}/chromium/src/third_party/Python-Markdown@b08af21eb795e522e1b972cb85bff59edb1ae209',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/qemu-linux-x64': {
      'packages': [
          {
              'package': 'fuchsia/qemu/linux-amd64',
              'version': '9cc486c5b18a0be515c39a280ca9a309c54cf994'
          },
      ],
      'condition': 'not build_with_chromium and (host_os == "linux" and checkout_fuchsia)',
      'dep_type': 'cipd',
  },

  '{angle_root}/third_party/qemu-mac-x64': {
      'packages': [
          {
              'package': 'fuchsia/qemu/mac-amd64',
              'version': '2d3358ae9a569b2d4a474f498b32b202a152134f'
          },
      ],
      'condition': 'not build_with_chromium and (host_os == "mac" and checkout_fuchsia)',
      'dep_type': 'cipd',
  },

  '{angle_root}/third_party/rapidjson/src': {
    'url': '{chromium_git}/external/github.com/Tencent/rapidjson@7484e06c589873e1ed80382d262087e4fa80fb63',
  },

  '{angle_root}/third_party/spirv-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@{spirv_headers_revision}',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/spirv-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@{spirv_tools_revision}',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/vulkan-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@{vulkan_headers_revision}',
  },

  '{angle_root}/third_party/vulkan-loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@{vulkan_loader_revision}',
  },

  '{angle_root}/third_party/vulkan-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@{vulkan_tools_revision}',
  },

  '{angle_root}/third_party/vulkan-validation-layers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@{vulkan_validation_revision}',
  },

  '{angle_root}/third_party/yasm': {
    'url': '{chromium_git}/chromium/src/third_party/yasm@86b6058141a42aed51bbd8bb9f9d54d199d9dbd0',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/yasm/source/patched-yasm': {
    'url': '{chromium_git}/chromium/deps/yasm/patched-yasm.git@720b70524a4424b15fc57e82263568c8ba0496ad',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/zlib': {
    'url': '{chromium_git}/chromium/src/third_party/zlib@f95aeb0fa7f136ef4a457a6d9ba6f3c2701a444b',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/tools/clang': {
    'url': '{chromium_git}/chromium/src/tools/clang.git@210f1dc3ebf8504ae246d925e9110ec427eef43f',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/tools/md_browser': {
    'url': '{chromium_git}/chromium/src/tools/md_browser@e9462696241f3ca832890473173e03e7bcfe6adc',
    'condition': 'not build_with_chromium',
  },
}

hooks = [
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win" and not build_with_chromium',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', '{angle_root}/buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'condition': 'host_os == "mac" and not build_with_chromium',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', '{angle_root}/buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux" and not build_with_chromium',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', '{angle_root}/buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and ((checkout_x86 or checkout_x64) and not build_with_chromium)',
    'action': ['python', '{angle_root}/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x64 and not build_with_chromium)',
    'action': ['python', '{angle_root}/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': ['python', '{angle_root}/build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac and not build_with_chromium',
    'action': ['python', '{angle_root}/build/mac_toolchain.py'],
  },

  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', '{angle_root}/tools/clang/scripts/update.py'],
    'condition': 'not build_with_chromium',
  },

  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': ['python', '{angle_root}/build/util/lastchange.py',
               '-o', '{angle_root}/build/util/LASTCHANGE'],
  },

  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and (host_os == "win" and not build_with_chromium)',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', '{angle_root}/build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },

  {
    'name': 'fuchsia_sdk',
    'pattern': '.',
    'condition': 'checkout_fuchsia and not build_with_chromium',
    'action': [
      'python',
      '{angle_root}/build/fuchsia/update_sdk.py',
    ],
  },

  # Download glslang validator binary for Linux.
  {
    'name': 'linux_glslang_validator',
    'pattern': '.',
    'condition': 'checkout_linux and not build_with_chromium',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'angle-glslang-validator',
                '-s', '{angle_root}/tools/glslang/glslang_validator.sha1',
    ],
  },

  # Download glslang validator binary for Windows.
  {
    'name': 'win_glslang_validator',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32*',
                '--no_auth',
                '--bucket', 'angle-glslang-validator',
                '-s', '{angle_root}/tools/glslang/glslang_validator.exe.sha1',
    ],
  },
]

recursedeps = [
  # buildtools provides clang_format.
  '{angle_root}/buildtools',
]
