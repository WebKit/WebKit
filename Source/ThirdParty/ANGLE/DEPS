# This file is used to manage the dependencies of the ANGLE git repo. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

vars = {
  'android_git': 'https://android.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',

  # This variable is set on the Chrome infra for compatiblity with gclient.
  'angle_root': '.',

  # This variable is overrided in Chromium's DEPS file.
  'build_with_chromium': False,

  # By default, do not check out angle-internal. This can be overridden e.g. with custom_vars.
  # We overload Chromium's 'src-internal' for infra simplicity.
  'checkout_src_internal': False,

  # Current revision of dEQP.
  'deqp_revision': 'd3eef28e67ce6795ba3a2124aaa977819729d45f',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': '664ad418f8455159fb066e9e27d159f191f976a9',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '123dc278f204f8e833e1a88d31c46d0edf81d4b2',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': '7f7236f1eba0afae2669ea777896f0435be4c96d',

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

  '{angle_root}/angle-internal': {
    'url': 'https://chrome-internal.googlesource.com/angle/angle-internal.git@05522f9eb159afbbe35a246d5027893c56926a76',
    'condition': 'checkout_src_internal',
  },

  '{angle_root}/build': {
    'url': '{chromium_git}/chromium/src/build.git@af1bbc9483aed572f1ea1a32e43bb504c3edb357',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/buildtools': {
    'url': '{chromium_git}/chromium/src/buildtools.git@74cfb57006f83cfe050817526db359d5c8a11628',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/testing': {
    'url': '{chromium_git}/chromium/src/testing@78e8d947158ba72b0e457abc9ea3fb30c2252646',
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
    'url': '{chromium_git}/chromium/src/third_party/fuchsia-sdk.git@5fd29151cf35c0813c33cc368a7c78389e3f5caa',
    'condition': 'checkout_fuchsia and not build_with_chromium',
  },

  # glmark2 is a GPL3-licensed OpenGL ES 2.0 benchmark. We use it for testing.
  '{angle_root}/third_party/glmark2/src': {
    'url': '{chromium_git}/external/github.com/glmark2/glmark2@9e01aef1a786b28aca73135a5b00f85c357e8f5e',
  },

  '{angle_root}/third_party/glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@{glslang_revision}',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/googletest': {
    'url': '{chromium_git}/chromium/src/third_party/googletest@c721b68ddecc18bbc6b763b2fe8ab802c22f228a',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/googletest/src': {
    'url': '{chromium_git}/external/github.com/google/googletest.git@3f5b5b8f8493a03fa25f1e4a7eae7678514a431d',
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
    'url': '{chromium_git}/chromium/src/third_party/jsoncpp@48246a099549ab325c01f69f24a34fc72e5c42e4',
    'condition': 'not build_with_chromium',
   },

  '{angle_root}/third_party/jsoncpp/source': {
    'url' : '{chromium_git}/external/github.com/open-source-parsers/jsoncpp@645250b6690785be60ab6780ce4b58698d884d11',
    'condition': 'not build_with_chromium',
   },

  '{angle_root}/third_party/Python-Markdown': {
    'url': '{chromium_git}/chromium/src/third_party/Python-Markdown@36657c103ce5964733bbbb29377085e9cc1a9472',
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
    'url': '{chromium_git}/chromium/src/third_party/yasm@15439370afd9df5359a0ff12244c2ccb2111c787',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/yasm/source/patched-yasm': {
    'url': '{chromium_git}/chromium/deps/yasm/patched-yasm.git@720b70524a4424b15fc57e82263568c8ba0496ad',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/third_party/zlib': {
    'url': '{chromium_git}/chromium/src/third_party/zlib@0044d0424c7f7d15436541ed1ecc89479b8bfda4',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/tools/clang': {
    'url': '{chromium_git}/chromium/src/tools/clang.git@2fef805e5b05b26a8c87c47865590b5f43218611',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/tools/md_browser': {
    'url': '{chromium_git}/chromium/src/tools/md_browser@0bfd826f8566a99923e64a782908faca72bc457c',
    'condition': 'not build_with_chromium',
  },

  '{angle_root}/tools/memory': {
    'url': '{chromium_git}/chromium/src/tools/memory@89552acb6e60f528fe3c98eac7b445d4c34183ee',
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
  # angle-internal has its own DEPS file to pull additional internal repos
  '{angle_root}/angle-internal',

  # buildtools provides clang_format.
  '{angle_root}/buildtools',
]
