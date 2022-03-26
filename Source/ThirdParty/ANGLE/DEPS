# This file is used to manage the dependencies of the ANGLE git repo. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True

gclient_gn_args_file = 'build/config/gclient_args.gni'

gclient_gn_args = [
  'generate_location_tags',
]

vars = {
  'android_git': 'https://android.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',
  'chrome_internal_git': 'https://chrome-internal.googlesource.com',
  'swiftshader_git': 'https://swiftshader.googlesource.com',

  # This variable is overrided in Chromium's DEPS file.
  'build_with_chromium': False,

  # We don't use location metadata in our test isolates.
  'generate_location_tags': False,

  # Only check out public sources by default. This can be overridden with custom_vars.
  'checkout_angle_internal': False,

  # Controls if we check out the restricted traces.
  'checkout_angle_restricted_traces': 'checkout_angle_internal',

  # Pull in Android native toolchain dependencies for Chrome OS too, so we can
  # build ARC++ support libraries.
  'checkout_android_native_support': 'checkout_android or checkout_chromeos',

  # Version of Chromium our Chromium-based DEPS are mirrored from.
  'chromium_revision': '3e4963702e0228ccffdf19e38da7e840a88d59d5',
  # We never want to checkout chromium,
  # but need a dummy DEPS entry for the autoroller
  'dummy_checkout_chromium': False,

  # Current revision of VK-GL-CTS (a.k.a dEQP).
  'vk_gl_cts_revision': '823d49cf5f5e5622e1fade4424e92ec2f61b81a6',

  # Current revision of googletest.
  # Note: this dep cannot be auto-rolled b/c of nesting.
  'googletest_revision': '2d924d7a971e9667d76ad09727fb2402b4f8a1e3',

  # Current revision of Chrome's third_party googletest directory. This
  # repository is mirrored as a separate repository, with separate git hashes
  # that don't match the external googletest repository or Chrome. Mirrored
  # patches will have a different git hash associated with them.
  # To roll, first get the new hash for chromium_googletest_revision from the
  # mirror of third_party/googletest located here:
  # https://chromium.googlesource.com/chromium/src/third_party/googletest/
  # Then get the new hash for googletest_revision from the root Chrome DEPS
  # file: https://source.chromium.org/chromium/chromium/src/+/main:DEPS
  'chromium_googletest_revision': '17bbed2084d3127bd7bcd27283f18d7a5861bea8',

  # Current revision of jsoncpp.
  # Note: this dep cannot be auto-rolled b/c of nesting.
  'jsoncpp_revision': '645250b6690785be60ab6780ce4b58698d884d11',

  # Current revision of Chrome's third_party jsoncpp directory. This repository
  # is mirrored as a separate repository, with separate git hashes that
  # don't match the external JsonCpp repository or Chrome. Mirrored patches
  # will have a different git hash associated with them.
  # To roll, first get the new hash for chromium_jsoncpp_revision from the
  # mirror of third_party/jsoncpp located here:
  # https://chromium.googlesource.com/chromium/src/third_party/jsoncpp/
  # Then get the new hash for jsoncpp_revision from the root Chrome DEPS file:
  # https://source.chromium.org/chromium/chromium/src/+/main:DEPS
  'chromium_jsoncpp_revision': '30a6ac108e24dabac7c2e0df4d33d55032af4ee7',

  # Current revision of patched-yasm.
  # Note: this dep cannot be auto-rolled b/c of nesting.
  'patched_yasm_revision': '720b70524a4424b15fc57e82263568c8ba0496ad',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': '389f33bb40a3345b73a68613178c789476ceaecf',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling luci-go
  # and whatever else without interference from each other.
  'luci_go': 'git_revision:cb424e70e75136736a86359ef070aa96425fe7a3',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_build-tools_version
  # and whatever else without interference from each other.
  'android_sdk_build-tools_version': 'tRoD45SCi7UleQqSV7MrMQO1_e5P8ysphkCcj6z_cCQC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_emulator_version
  # and whatever else without interference from each other.
  'android_sdk_emulator_version': 'gMHhUuoQRKfxr-MBn3fNNXZtkAVXtOwMwT7kfx8jkIgC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_extras_version
  # and whatever else without interference from each other.
  'android_sdk_extras_version': 'ppQ4TnqDvBHQ3lXx5KPq97egzF5X2FFyOrVHkGmiTMQC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_patcher_version
  # and whatever else without interference from each other.
  'android_sdk_patcher_version': 'I6FNMhrXlpB-E1lOhMlvld7xt9lBVNOO83KIluXDyA0C',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platform-tools_version
  # and whatever else without interference from each other.
  'android_sdk_platform-tools_version': 'g7n_-r6yJd_SGRklujGB1wEt8iyr77FZTUJVS9w6O34C',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platforms_version
  # and whatever else without interference from each other.
  'android_sdk_platforms_version': 'lL3IGexKjYlwjO_1Ga-xwxgwbE_w-lmi2Zi1uOlWUIAC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_sources_version
  # and whatever else without interference from each other.
  'android_sdk_sources_version': '_a_BcnANjPYw5mSKlNHa7GFY8yc1kdqj2rmQgac7yUcC',
}

deps = {

  'build': {
    'url': '{chromium_git}/chromium/src/build.git@8f75e7de55882e309c982e8d203f7ad0f354e10e',
    'condition': 'not build_with_chromium',
  },

  'buildtools': {
    'url': '{chromium_git}/chromium/src/buildtools.git@d8c375426d8f7f4147f7d4109bb63c12655fb8d6',
    'condition': 'not build_with_chromium',
  },

  'buildtools/clang_format/script': {
    'url': '{chromium_git}/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@e435ad79c17b1888b34df88d6a30a094936e3836',
    'condition': 'not build_with_chromium',
  },

  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': 'git_revision:bd99dbf98cbdefe18a4128189665c5761263bcfb',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and host_os == "linux"',
  },

  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': 'git_revision:bd99dbf98cbdefe18a4128189665c5761263bcfb',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and host_os == "mac"',
  },

  'buildtools/third_party/libc++/trunk': {
    'url': '{chromium_git}/external/github.com/llvm/llvm-project/libcxx.git@79a2e924d96e2fc1e4b937c42efd08898fa472d7',
    'condition': 'not build_with_chromium',
  },

  'buildtools/third_party/libc++abi/trunk': {
    'url': '{chromium_git}/external/github.com/llvm/llvm-project/libcxxabi.git@93b8dcd57bd8ebe201ec24f7257339988ed2ef7c',
    'condition': 'not build_with_chromium',
  },

  'buildtools/third_party/libunwind/trunk': {
    'url': '{chromium_git}/external/github.com/llvm/llvm-project/libunwind.git@d1c7f92b8b0bff8d9f710ca40e44563a63db376e',
    'condition': 'not build_with_chromium',
  },

  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': 'git_revision:bd99dbf98cbdefe18a4128189665c5761263bcfb',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and host_os == "win"',
  },

  'testing': {
    'url': '{chromium_git}/chromium/src/testing@55f1dc2d27cd6d5f8cdc4182ec30f5096dedbf51',
    'condition': 'not build_with_chromium',
  },

  'third_party/abseil-cpp': {
    'url': '{chromium_git}/chromium/src/third_party/abseil-cpp@a4297493d86022b91c93f7c1de29e9ad0a0e2489',
    'condition': 'not build_with_chromium',
  },

  'third_party/android_build_tools': {
    'url': '{chromium_git}/chromium/src/third_party/android_build_tools@29766b5d64108dbbd40be00167c7b8728c425299',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_build_tools/aapt2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2',
              'version': 'wicn5Ce1ay6ivbZ1GNFF0gRSS3NYv_7hJTPtVga3O-QC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/art': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/art',
              'version': '87169fbc701d244c311e6aa8843591a7f1710bc0',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_build_tools/bundletool': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/bundletool',
               'version': 'LoldiQDpZ0uTdAm5EPgZ8hBJ3La2KlTWLuaRxE7eDigC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps': {
    'url': '{chromium_git}/chromium/src/third_party/android_deps@be0731f39ef5bb7128eff8d09da90cd9e708c3b4',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_ndk': {
    'url': '{chromium_git}/android_ndk.git@9644104c8cf85bf1bdce5b1c0691e9778572c3f8',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_platform': {
    'url': '{chromium_git}/chromium/src/third_party/android_platform@87b4b48de3c8204224d63612c287eb5a447a562d',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_sdk': {
    'url': '{chromium_git}/chromium/src/third_party/android_sdk@5f109f3c4191f99cafc03e0dbcd5946859430174',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_sdk/androidx_browser/src': {
      'url': '{chromium_git}/external/gob/android/platform/frameworks/support/browser.git@65086eb5e52c16778fa7b4f157156d17b176fcb3',
      'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/31.0.0',
              'version': Var('android_sdk_build-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': Var('android_sdk_emulator_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/extras',
              'version': Var('android_sdk_extras_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/patcher',
              'version': Var('android_sdk_patcher_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platform-tools',
              'version': Var('android_sdk_platform-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-31',
              'version': Var('android_sdk_platforms_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/sources/android-31',
              'version': Var('android_sdk_sources_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': 'PGPmqJtSIQ84If155ba7iTU846h5WJ-bL5d_OoUWEWYC',
          },
      ],
      'condition': 'checkout_android_native_support and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_system_sdk': {
      'packages': [
          {
              'package': 'chromium/third_party/android_system_sdk/public',
              'version': 'oSfDIvHlCa6W0gS79Q5OOfB9E4eBg3uAvi3BEDN21U0C',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/bazel': {
      'packages': [
          {
              'package': 'chromium/third_party/bazel',
              'version': 'VjMsf48QUWw8n7XtJP2AuSjIGmbQeYdWdwyxVvIRLmAC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/catapult': {
    'url': '{chromium_git}/catapult.git@{catapult_revision}',
    'condition': 'not build_with_chromium',
  },

  # Cherry is a dEQP/VK-GL-CTS management GUI written in Go. We use it for viewing test results.
  'third_party/cherry': {
    'url': '{android_git}/platform/external/cherry@4f8fb08d33ca5ff05a1c638f04c85bbb8d8b52cc',
    'condition': 'not build_with_chromium',
  },

  'third_party/colorama/src': {
    'url': '{chromium_git}/external/colorama.git@799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/depot_tools': {
    'url': '{chromium_git}/chromium/tools/depot_tools.git@76979dae56c08d24b092347a1bdd2abe474555a3',
    'condition': 'not build_with_chromium',
  },

  # We never want to checkout chromium,
  # but need a dummy DEPS entry for the autoroller
  'third_party/dummy_chromium': {
    'url': '{chromium_git}/chromium/src.git@{chromium_revision}',
    'condition': 'dummy_checkout_chromium',
  },

  'third_party/fuchsia-sdk': {
    'url': '{chromium_git}/chromium/src/third_party/fuchsia-sdk.git@1785f0ac8e1fe81cb25e260acbe7de8f62fa3e44',
    'condition': 'checkout_fuchsia and not build_with_chromium',
  },

  # Closed-source OpenGL ES 1.1 Conformance tests.
  'third_party/gles1_conform': {
    'url': '{chrome_internal_git}/angle/es-cts.git@dc9f502f709c9cd88d7f8d3974f1c77aa246958e',
    'condition': 'checkout_angle_internal',
  },

  # glmark2 is a GPL3-licensed OpenGL ES 2.0 benchmark. We use it for testing.
  'third_party/glmark2/src': {
    'url': '{chromium_git}/external/github.com/glmark2/glmark2@ca8de51fedb70bace5351c6b002eb952c747e889',
  },

  'third_party/googletest': {
    'url': '{chromium_git}/chromium/src/third_party/googletest@{chromium_googletest_revision}',
    'condition': 'not build_with_chromium',
  },

  'third_party/ijar': {
    'url': '{chromium_git}/chromium/src/third_party/ijar@299c22307305d8749170cfc0837146b9b7cbab3a',
    'condition': 'checkout_android and not build_with_chromium',
  },

  # libjpeg_turbo is used by glmark2.
  'third_party/libjpeg_turbo': {
    'url': '{chromium_git}/chromium/deps/libjpeg_turbo.git@22f1a22c99e9dde8cd3c72ead333f425c5a7aa77',
    'condition': 'not build_with_chromium',
  },

  'third_party/libpng/src': {
    'url': '{android_git}/platform/external/libpng@d2ece84bd73af1cd5fae5e7574f79b40e5de4fba',
    'condition': 'not build_with_chromium',
  },

  'third_party/jdk': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk',
              'version': 'JhpgSvTpgVUkoKe56yQmYaR1jXNcY8NqlltA0mKIO4EC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/jdk/extras': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk/extras',
              'version': '-7m_pvgICYN60yQI3qmTj_8iKjtnT4NXicT0G_jJPqsC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/jinja2': {
    'url': '{chromium_git}/chromium/src/third_party/jinja2@ee69aa00ee8536f61db6a451f3858745cf587de6',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/jsoncpp': {
    'url': '{chromium_git}/chromium/src/third_party/jsoncpp@{chromium_jsoncpp_revision}',
    'condition': 'not build_with_chromium',
   },

  'third_party/markupsafe': {
    'url': '{chromium_git}/chromium/src/third_party/markupsafe@1b882ef6372b58bfd55a3285f37ed801be9137cd',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/nasm': {
    'url': '{chromium_git}/chromium/deps/nasm.git@9215e8e1d0fe474ffd3e16c1a07a0f97089e6224',
    'condition': 'not build_with_chromium',
  },

  'third_party/OpenCL-ICD-Loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/OpenCL-ICD-Loader@9b5e3849b49a1448996c8b96ba086cd774d987db',
    'condition': 'not build_with_chromium',
  },

  'third_party/proguard': {
      'packages': [
          {
              'package': 'chromium/third_party/proguard',
              'version': 'Fd91BJFVlmiO6c46YMTsdy7n2f5Sk2hVVGlzPLvqZPsC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/protobuf': {
    'url': '{chromium_git}/chromium/src/third_party/protobuf@71d78db8def0833cf17178d54fe63b20b97e80ce',
    'condition': 'not build_with_chromium',
  },

  'third_party/Python-Markdown': {
    'url': '{chromium_git}/chromium/src/third_party/Python-Markdown@872ba9e68a6c698ede103b32265c265c026b8fee',
    'condition': 'not build_with_chromium',
  },

  'third_party/qemu-linux-x64': {
      'packages': [
          {
              'package': 'fuchsia/third_party/qemu/linux-amd64',
              'version': 'FFZaD9tecL-z0lq2XP_7UqiAaMgRGwXTyvcmkv7XCQcC'
          },
      ],
      'condition': 'not build_with_chromium and (host_os == "linux" and checkout_fuchsia)',
      'dep_type': 'cipd',
  },

  'third_party/qemu-mac-x64': {
      'packages': [
          {
              'package': 'fuchsia/third_party/qemu/mac-amd64',
              'version': '79L6B9YhuL7uIg_CxwlQcZqLOixVtS2Cctn7dmVg0q4C'
          },
      ],
      'condition': 'not build_with_chromium and (host_os == "mac" and checkout_fuchsia)',
      'dep_type': 'cipd',
  },

  'third_party/r8': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'ovozeRSDDfERnEFpDo_WS6OYOcEF7oT1JzGxCSf-g0kC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/rapidjson/src': {
    'url': '{chromium_git}/external/github.com/Tencent/rapidjson@7484e06c589873e1ed80382d262087e4fa80fb63',
  },

  'third_party/requests/src': {
    'url': '{chromium_git}/external/github.com/kennethreitz/requests.git@refs/tags/v2.23.0',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/six': {
    'url': '{chromium_git}/chromium/src/third_party/six@c96255caa80a7e0e45de07ce9af088a2ce984b68',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'third_party/SwiftShader': {
    'url': '{swiftshader_git}/SwiftShader@57eca9df5dc56d290f662ace7481ed758618d559',
    'condition': 'not build_with_chromium',
  },

  'third_party/turbine': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': 'Go9J3Mz5ankZAgxmn5GxeXKdEDV73zaZp2ojNfGC1RQC',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/VK-GL-CTS/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/VK-GL-CTS@{vk_gl_cts_revision}',
  },

  'third_party/vulkan-deps': {
    'url': '{chromium_git}/vulkan-deps@c4109411d3f366353898aed63156ada2fa562817',
    'condition': 'not build_with_chromium',
  },

  'third_party/vulkan_memory_allocator': {
    'url': '{chromium_git}/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator@5e49f57a6e71a026a54eb42e366de09a4142d24e',
    'condition': 'not build_with_chromium',
  },

  'third_party/wayland': {
    'url': '{chromium_git}/external/anongit.freedesktop.org/git/wayland/wayland@upstream/1.20.0',
    'condition': 'not build_with_chromium and host_os == "linux"'
  },

  'third_party/zlib': {
    'url': '{chromium_git}/chromium/src/third_party/zlib@923f5eb4bfa403d0f14d9cbca2cdf7fa7416ee67',
    'condition': 'not build_with_chromium',
  },

  'tools/android/errorprone_plugin': {
    'url': '{chromium_git}/chromium/src/tools/android/errorprone_plugin@bd76c90eacab3446f7cfc32531d0881117e94324',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'tools/clang': {
    'url': '{chromium_git}/chromium/src/tools/clang.git@a15c2dfa35177dc21577a1760e8f4c0c1e854da6',
    'condition': 'not build_with_chromium',
  },

  'tools/clang/dsymutil': {
    'packages': [
      {
        'package': 'chromium/llvm-build-tools/dsymutil',
        'version': 'M56jPzDv1620Rnm__jTMYS62Zi8rxHVq7yw0qeBFEgkC',
      }
    ],
    'condition': 'checkout_mac and not build_with_chromium',
    'dep_type': 'cipd',
  },

  'tools/luci-go': {
    'packages': [
      {
        'package': 'infra/tools/luci/isolate/${{platform}}',
        'version': Var('luci_go'),
      },
      {
        'package': 'infra/tools/luci/swarming/${{platform}}',
        'version': Var('luci_go'),
      },
    ],
    'condition': 'not build_with_chromium',
    'dep_type': 'cipd',
  },

  'tools/mb': {
    'url': '{chromium_git}/chromium/src/tools/mb@e6f159df60d1788f9a5edf8ca32e456f0c72949b',
    'condition': 'not build_with_chromium',
  },

  'tools/md_browser': {
    'url': '{chromium_git}/chromium/src/tools/md_browser@ac44704f23348283ef7ae6ddbfe95420b37c34dc',
    'condition': 'not build_with_chromium',
  },

  'tools/memory': {
    'url': '{chromium_git}/chromium/src/tools/memory@8a340b64dce4b578494881a1112cfa11b72330e2',
    'condition': 'not build_with_chromium',
  },

  'tools/perf': {
    'url': '{chromium_git}/chromium/src/tools/perf@46fa9b4765c61bdff3e147cd68accabc4d1fe0db',
    'condition': 'not build_with_chromium',
  },

  'tools/protoc_wrapper': {
    'url': '{chromium_git}/chromium/src/tools/protoc_wrapper@c16b0dc8db35e95a04eaef88079237634c7f20c2',
    'condition': 'not build_with_chromium',
  },

  'tools/python': {
    'url': '{chromium_git}/chromium/src/tools/python@8cb897d6983330c179157160894ea57b73a5e623',
    'condition': 'checkout_android and not build_with_chromium',
  },

  'tools/skia_goldctl/linux': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/linux-amd64',
          'version': 'iqtz2prn9CUv6A8KCcxJzadmPEDLY1FPP-b2YqIFQ1cC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_linux and not build_with_chromium',
  },

  'tools/skia_goldctl/win': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/windows-amd64',
          'version': 'EbVQXa1u0hbZ8pxb0Il6Rbc1ErHpIN_-kMVOzBXMQyoC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_win and not build_with_chromium',
  },

  'tools/skia_goldctl/mac_amd64': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/mac-amd64',
          'version': 'Y4l14LBqCsT9EhffPIOtso9VSpwqQE9WccjZdDBZmLoC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_mac and not build_with_chromium',
  },

  'tools/skia_goldctl/mac_arm64': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/mac-arm64',
          'version': 'INEqc8JI_mtww_X0ShOlDkF3S8OG4tjF4Nkei0K7ci8C',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_mac and not build_with_chromium',
  },

  'tools/valgrind': {
    'url': '{chromium_git}/chromium/src/tools/valgrind@5045db509823b4c01bd9313d0fa4c745f48dc36d',
    'condition': 'not build_with_chromium',
  },

  # === ANGLE Restricted Trace Generated Code Start ===
  'src/tests/restricted_traces/aliexpress': {
      'packages': [
        {
            'package': 'angle/traces/aliexpress',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/among_us': {
      'packages': [
        {
            'package': 'angle/traces/among_us',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/angry_birds_2_1500': {
      'packages': [
        {
            'package': 'angle/traces/angry_birds_2_1500',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/angry_birds_2_launch': {
      'packages': [
        {
            'package': 'angle/traces/angry_birds_2_launch',
            'version': 'version:7',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/arena_of_valor': {
      'packages': [
        {
            'package': 'angle/traces/arena_of_valor',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/asphalt_8': {
      'packages': [
        {
            'package': 'angle/traces/asphalt_8',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/asphalt_9': {
      'packages': [
        {
            'package': 'angle/traces/asphalt_9',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/avakin_life': {
      'packages': [
        {
            'package': 'angle/traces/avakin_life',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/aztec_ruins': {
      'packages': [
        {
            'package': 'angle/traces/aztec_ruins',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/badland': {
      'packages': [
        {
            'package': 'angle/traces/badland',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/beach_buggy_racing': {
      'packages': [
        {
            'package': 'angle/traces/beach_buggy_racing',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/blade_and_soul_revolution': {
      'packages': [
        {
            'package': 'angle/traces/blade_and_soul_revolution',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/blockman_go': {
      'packages': [
        {
            'package': 'angle/traces/blockman_go',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/brawl_stars': {
      'packages': [
        {
            'package': 'angle/traces/brawl_stars',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/bricks_breaker_quest': {
      'packages': [
        {
            'package': 'angle/traces/bricks_breaker_quest',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/bubble_shooter': {
      'packages': [
        {
            'package': 'angle/traces/bubble_shooter',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/bus_simulator_indonesia': {
      'packages': [
        {
            'package': 'angle/traces/bus_simulator_indonesia',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/call_break_offline_card_game': {
      'packages': [
        {
            'package': 'angle/traces/call_break_offline_card_game',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/candy_crush_500': {
      'packages': [
        {
            'package': 'angle/traces/candy_crush_500',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/candy_crush_soda_saga': {
      'packages': [
        {
            'package': 'angle/traces/candy_crush_soda_saga',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/car_parking_multiplayer': {
      'packages': [
        {
            'package': 'angle/traces/car_parking_multiplayer',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/castlevania_sotn': {
      'packages': [
        {
            'package': 'angle/traces/castlevania_sotn',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/clash_of_clans': {
      'packages': [
        {
            'package': 'angle/traces/clash_of_clans',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/clash_royale': {
      'packages': [
        {
            'package': 'angle/traces/clash_royale',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/cod_mobile': {
      'packages': [
        {
            'package': 'angle/traces/cod_mobile',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/coin_master': {
      'packages': [
        {
            'package': 'angle/traces/coin_master',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/command_and_conquer_rivals': {
      'packages': [
        {
            'package': 'angle/traces/command_and_conquer_rivals',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/cookie_run_kingdom': {
      'packages': [
        {
            'package': 'angle/traces/cookie_run_kingdom',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/dead_by_daylight': {
      'packages': [
        {
            'package': 'angle/traces/dead_by_daylight',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/dead_trigger_2': {
      'packages': [
        {
            'package': 'angle/traces/dead_trigger_2',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/disney_tsum_tsum': {
      'packages': [
        {
            'package': 'angle/traces/disney_tsum_tsum',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/dota_underlords': {
      'packages': [
        {
            'package': 'angle/traces/dota_underlords',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/dr_driving': {
      'packages': [
        {
            'package': 'angle/traces/dr_driving',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/dragon_ball_legends': {
      'packages': [
        {
            'package': 'angle/traces/dragon_ball_legends',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/dragon_mania_legends': {
      'packages': [
        {
            'package': 'angle/traces/dragon_mania_legends',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/dragon_raja': {
      'packages': [
        {
            'package': 'angle/traces/dragon_raja',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/efootball_pes_2021': {
      'packages': [
        {
            'package': 'angle/traces/efootball_pes_2021',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/egypt_1500': {
      'packages': [
        {
            'package': 'angle/traces/egypt_1500',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/eight_ball_pool': {
      'packages': [
        {
            'package': 'angle/traces/eight_ball_pool',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/extreme_car_driving_simulator': {
      'packages': [
        {
            'package': 'angle/traces/extreme_car_driving_simulator',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/fallout_shelter_online': {
      'packages': [
        {
            'package': 'angle/traces/fallout_shelter_online',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/farm_heroes_saga': {
      'packages': [
        {
            'package': 'angle/traces/farm_heroes_saga',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/fate_grand_order': {
      'packages': [
        {
            'package': 'angle/traces/fate_grand_order',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/fifa_mobile': {
      'packages': [
        {
            'package': 'angle/traces/fifa_mobile',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/final_fantasy': {
      'packages': [
        {
            'package': 'angle/traces/final_fantasy',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/final_fantasy_brave_exvius': {
      'packages': [
        {
            'package': 'angle/traces/final_fantasy_brave_exvius',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/fire_emblem_heroes': {
      'packages': [
        {
            'package': 'angle/traces/fire_emblem_heroes',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/five_nights_at_freddys': {
      'packages': [
        {
            'package': 'angle/traces/five_nights_at_freddys',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/free_fire': {
      'packages': [
        {
            'package': 'angle/traces/free_fire',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/gardenscapes': {
      'packages': [
        {
            'package': 'angle/traces/gardenscapes',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/genshin_impact': {
      'packages': [
        {
            'package': 'angle/traces/genshin_impact',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/google_maps': {
      'packages': [
        {
            'package': 'angle/traces/google_maps',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/happy_color': {
      'packages': [
        {
            'package': 'angle/traces/happy_color',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/hay_day': {
      'packages': [
        {
            'package': 'angle/traces/hay_day',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/hearthstone': {
      'packages': [
        {
            'package': 'angle/traces/hearthstone',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/higgs_domino_island': {
      'packages': [
        {
            'package': 'angle/traces/higgs_domino_island',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/hill_climb_racing': {
      'packages': [
        {
            'package': 'angle/traces/hill_climb_racing',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/homescapes': {
      'packages': [
        {
            'package': 'angle/traces/homescapes',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/idle_heroes': {
      'packages': [
        {
            'package': 'angle/traces/idle_heroes',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/junes_journey': {
      'packages': [
        {
            'package': 'angle/traces/junes_journey',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/kartrider_rush': {
      'packages': [
        {
            'package': 'angle/traces/kartrider_rush',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/klondike_adventures': {
      'packages': [
        {
            'package': 'angle/traces/klondike_adventures',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/last_shelter_survival': {
      'packages': [
        {
            'package': 'angle/traces/last_shelter_survival',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/league_of_legends_wild_rift': {
      'packages': [
        {
            'package': 'angle/traces/league_of_legends_wild_rift',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/lego_legacy': {
      'packages': [
        {
            'package': 'angle/traces/lego_legacy',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/lineage_m': {
      'packages': [
        {
            'package': 'angle/traces/lineage_m',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/lords_mobile': {
      'packages': [
        {
            'package': 'angle/traces/lords_mobile',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/ludo_king': {
      'packages': [
        {
            'package': 'angle/traces/ludo_king',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/magic_rush_heroes': {
      'packages': [
        {
            'package': 'angle/traces/magic_rush_heroes',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/magic_tiles_3': {
      'packages': [
        {
            'package': 'angle/traces/magic_tiles_3',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/manhattan_10': {
      'packages': [
        {
            'package': 'angle/traces/manhattan_10',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/manhattan_31': {
      'packages': [
        {
            'package': 'angle/traces/manhattan_31',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/mario_kart_tour': {
      'packages': [
        {
            'package': 'angle/traces/mario_kart_tour',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/marvel_contest_of_champions': {
      'packages': [
        {
            'package': 'angle/traces/marvel_contest_of_champions',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/marvel_strike_force': {
      'packages': [
        {
            'package': 'angle/traces/marvel_strike_force',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/messenger_lite': {
      'packages': [
        {
            'package': 'angle/traces/messenger_lite',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/minecraft': {
      'packages': [
        {
            'package': 'angle/traces/minecraft',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/mini_world': {
      'packages': [
        {
            'package': 'angle/traces/mini_world',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/mobile_legends': {
      'packages': [
        {
            'package': 'angle/traces/mobile_legends',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/my_talking_tom2': {
      'packages': [
        {
            'package': 'angle/traces/my_talking_tom2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/my_talking_tom_friends': {
      'packages': [
        {
            'package': 'angle/traces/my_talking_tom_friends',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/nba2k20_800': {
      'packages': [
        {
            'package': 'angle/traces/nba2k20_800',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/nier_reincarnation': {
      'packages': [
        {
            'package': 'angle/traces/nier_reincarnation',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/one_punch_man': {
      'packages': [
        {
            'package': 'angle/traces/one_punch_man',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/plants_vs_zombies_2': {
      'packages': [
        {
            'package': 'angle/traces/plants_vs_zombies_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/pokemon_go': {
      'packages': [
        {
            'package': 'angle/traces/pokemon_go',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/pokemon_unite': {
      'packages': [
        {
            'package': 'angle/traces/pokemon_unite',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/professional_baseball_spirits': {
      'packages': [
        {
            'package': 'angle/traces/professional_baseball_spirits',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/pubg_mobile_battle_royale': {
      'packages': [
        {
            'package': 'angle/traces/pubg_mobile_battle_royale',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/pubg_mobile_launch': {
      'packages': [
        {
            'package': 'angle/traces/pubg_mobile_launch',
            'version': 'version:6',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/pubg_mobile_lite': {
      'packages': [
        {
            'package': 'angle/traces/pubg_mobile_lite',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/pubg_mobile_skydive': {
      'packages': [
        {
            'package': 'angle/traces/pubg_mobile_skydive',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/ragnarok_m_eternal_love': {
      'packages': [
        {
            'package': 'angle/traces/ragnarok_m_eternal_love',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/raid_shadow_legends': {
      'packages': [
        {
            'package': 'angle/traces/raid_shadow_legends',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/real_commando_secret_mission': {
      'packages': [
        {
            'package': 'angle/traces/real_commando_secret_mission',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/real_cricket_20': {
      'packages': [
        {
            'package': 'angle/traces/real_cricket_20',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/real_gangster_crime': {
      'packages': [
        {
            'package': 'angle/traces/real_gangster_crime',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/real_racing3': {
      'packages': [
        {
            'package': 'angle/traces/real_racing3',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/respawnables': {
      'packages': [
        {
            'package': 'angle/traces/respawnables',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/rise_of_kingdoms': {
      'packages': [
        {
            'package': 'angle/traces/rise_of_kingdoms',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/romancing_saga': {
      'packages': [
        {
            'package': 'angle/traces/romancing_saga',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/rope_hero_vice_town': {
      'packages': [
        {
            'package': 'angle/traces/rope_hero_vice_town',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/saint_seiya_awakening': {
      'packages': [
        {
            'package': 'angle/traces/saint_seiya_awakening',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/sakura_school_simulator': {
      'packages': [
        {
            'package': 'angle/traces/sakura_school_simulator',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/scrabble_go': {
      'packages': [
        {
            'package': 'angle/traces/scrabble_go',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/shadow_fight_2': {
      'packages': [
        {
            'package': 'angle/traces/shadow_fight_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/slingshot_test1': {
      'packages': [
        {
            'package': 'angle/traces/slingshot_test1',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/slingshot_test2': {
      'packages': [
        {
            'package': 'angle/traces/slingshot_test2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/sniper_3d': {
      'packages': [
        {
            'package': 'angle/traces/sniper_3d',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/solar_smash': {
      'packages': [
        {
            'package': 'angle/traces/solar_smash',
            'version': 'version:2',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/sonic_the_hedgehog': {
      'packages': [
        {
            'package': 'angle/traces/sonic_the_hedgehog',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/standoff_2': {
      'packages': [
        {
            'package': 'angle/traces/standoff_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/state_of_survival': {
      'packages': [
        {
            'package': 'angle/traces/state_of_survival',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/subway_princess_runner': {
      'packages': [
        {
            'package': 'angle/traces/subway_princess_runner',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/subway_surfers': {
      'packages': [
        {
            'package': 'angle/traces/subway_surfers',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/summoners_war': {
      'packages': [
        {
            'package': 'angle/traces/summoners_war',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/talking_tom_hero_dash': {
      'packages': [
        {
            'package': 'angle/traces/talking_tom_hero_dash',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/temple_run_2': {
      'packages': [
        {
            'package': 'angle/traces/temple_run_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/temple_run_300': {
      'packages': [
        {
            'package': 'angle/traces/temple_run_300',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/toon_blast': {
      'packages': [
        {
            'package': 'angle/traces/toon_blast',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/township': {
      'packages': [
        {
            'package': 'angle/traces/township',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/trex_200': {
      'packages': [
        {
            'package': 'angle/traces/trex_200',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/war_planet_online': {
      'packages': [
        {
            'package': 'angle/traces/war_planet_online',
            'version': 'version:1',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/whatsapp': {
      'packages': [
        {
            'package': 'angle/traces/whatsapp',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/words_with_friends_2': {
      'packages': [
        {
            'package': 'angle/traces/words_with_friends_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/wordscapes': {
      'packages': [
        {
            'package': 'angle/traces/wordscapes',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/world_cricket_championship_2': {
      'packages': [
        {
            'package': 'angle/traces/world_cricket_championship_2',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/world_of_kings': {
      'packages': [
        {
            'package': 'angle/traces/world_of_kings',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/world_of_tanks_blitz': {
      'packages': [
        {
            'package': 'angle/traces/world_of_tanks_blitz',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/world_war_doh': {
      'packages': [
        {
            'package': 'angle/traces/world_war_doh',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/worms_zone_io': {
      'packages': [
        {
            'package': 'angle/traces/worms_zone_io',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  'src/tests/restricted_traces/zillow': {
      'packages': [
        {
            'package': 'angle/traces/zillow',
            'version': 'version:5',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_angle_restricted_traces',
  },
  # === ANGLE Restricted Trace Generated Code End ===

  # === ANDROID_DEPS Generated Code Start ===
  # Generated by //third_party/android_deps/fetch_all.py
  'third_party/android_deps/libs/android_arch_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_common',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_core_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_runtime',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_common',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_common_java8': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_common_java8',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_livedata': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_livedata',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_livedata_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_livedata_core',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_runtime',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/android_arch_lifecycle_viewmodel': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_viewmodel',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_animated_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_animated_vector_drawable',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_appcompat_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_appcompat_v7',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_asynclayoutinflater': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_asynclayoutinflater',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_cardview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_cardview_v7',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_collections': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_collections',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_coordinatorlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_coordinatorlayout',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_cursoradapter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_cursoradapter',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_customview': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_customview',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_design': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_design',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_documentfile': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_documentfile',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_drawerlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_drawerlayout',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_interpolator': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_interpolator',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_loader': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_loader',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_localbroadcastmanager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_localbroadcastmanager',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_multidex': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_multidex',
              'version': 'version:2@1.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_print': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_print',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_recyclerview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_recyclerview_v7',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_slidingpanelayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_slidingpanelayout',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_annotations',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_compat',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_core_ui': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_ui',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_core_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_utils',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_fragment': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_fragment',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_media_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_media_compat',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_v4': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_v4',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_support_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_vector_drawable',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_swiperefreshlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_swiperefreshlayout',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_transition': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_transition',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_versionedparcelable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_versionedparcelable',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_support_viewpager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_viewpager',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_tools_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_common',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_tools_desugar_jdk_libs': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs',
              'version': 'version:2@1.1.5.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_tools_desugar_jdk_libs_configuration': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs_configuration',
              'version': 'version:2@1.1.5.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_android_tools_sdk_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_sdk_common',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_github_ben_manes_caffeine_caffeine': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_github_ben_manes_caffeine_caffeine',
              'version': 'version:2@2.8.8.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_github_kevinstern_software_and_algorithms': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_github_kevinstern_software_and_algorithms',
              'version': 'version:2@1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_datatransport_transport_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_datatransport_transport_api',
              'version': 'version:2@2.2.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_auth': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone',
              'version': 'version:2@17.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_auth_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_base',
              'version': 'version:2@17.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_basement': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_basement',
              'version': 'version:2@17.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_cast': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_clearcut': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_cloud_messaging': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cloud_messaging',
              'version': 'version:2@16.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_fido': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_fido',
              'version': 'version:2@19.0.0-beta.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_flags': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_flags',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_gcm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_gcm',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_iid',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_instantapps': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_location': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_location',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_phenotype': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_places_placereport': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_places_placereport',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_stats': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_stats',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_tasks': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_tasks',
              'version': 'version:2@17.2.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_vision': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision',
              'version': 'version:2@18.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_gms_play_services_vision_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common',
              'version': 'version:2@18.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_material_material': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_material_material',
              'version': 'version:2@1.6.0-alpha01.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_android_play_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_play_core',
              'version': 'version:2@1.10.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_auto_auto_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_auto_common',
              'version': 'version:2@1.2.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_auto_service_auto_service': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service',
              'version': 'version:2@1.0-rc6.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_auto_service_auto_service_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service_annotations',
              'version': 'version:2@1.0-rc6.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_auto_value_auto_value_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_value_auto_value_annotations',
              'version': 'version:2@1.9.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_code_findbugs_jsr305': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_findbugs_jsr305',
              'version': 'version:2@3.0.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_code_gson_gson': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_gson_gson',
              'version': 'version:2@2.8.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_dagger_dagger': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger',
              'version': 'version:2@2.30.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_dagger_dagger_compiler': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_compiler',
              'version': 'version:2@2.30.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_dagger_dagger_producers': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_producers',
              'version': 'version:2@2.30.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_dagger_dagger_spi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_spi',
              'version': 'version:2@2.30.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_annotation': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotation',
              'version': 'version:2@2.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations',
              'version': 'version:2@2.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_check_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_check_api',
              'version': 'version:2@2.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_core',
              'version': 'version:2@2.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_error_prone_type_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_type_annotations',
              'version': 'version:2@2.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_javac': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac',
              'version': 'version:2@9+181-r4173-1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_errorprone_javac_shaded': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac_shaded',
              'version': 'version:2@9-dev-r4023-3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_annotations',
              'version': 'version:2@16.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_common',
              'version': 'version:2@19.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_components': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_components',
              'version': 'version:2@16.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_encoders': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_encoders',
              'version': 'version:2@16.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_encoders_json': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_encoders_json',
              'version': 'version:2@17.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_iid',
              'version': 'version:2@21.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_iid_interop': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_iid_interop',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_installations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_installations',
              'version': 'version:2@16.3.5.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_installations_interop': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_installations_interop',
              'version': 'version:2@16.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_measurement_connector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_measurement_connector',
              'version': 'version:2@18.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_firebase_firebase_messaging': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_messaging',
              'version': 'version:2@21.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_flatbuffers_flatbuffers_java': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_flatbuffers_flatbuffers_java',
              'version': 'version:2@2.0.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_googlejavaformat_google_java_format': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_googlejavaformat_google_java_format',
              'version': 'version:2@1.5.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_guava_failureaccess': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_failureaccess',
              'version': 'version:2@1.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_guava_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava',
              'version': 'version:2@31.0.1-jre.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_guava_guava_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava_android',
              'version': 'version:2@31.0-android.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_guava_listenablefuture': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_listenablefuture',
              'version': 'version:2@1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_j2objc_j2objc_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations',
              'version': 'version:2@1.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_protobuf_protobuf_java': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_java',
              'version': 'version:2@3.19.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_google_protobuf_protobuf_javalite': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_javalite',
              'version': 'version:2@3.19.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils',
              'version': 'version:2@1.3.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:2@1.13.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/com_squareup_javawriter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javawriter',
              'version': 'version:2@2.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/io_github_java_diff_utils_java_diff_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_github_java_diff_utils_java_diff_utils',
              'version': 'version:2@4.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/javax_annotation_javax_annotation_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_javax_annotation_api',
              'version': 'version:2@1.3.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/javax_annotation_jsr250_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_jsr250_api',
              'version': 'version:2@1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/javax_inject_javax_inject': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_inject_javax_inject',
              'version': 'version:2@1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/net_ltgt_gradle_incap_incap': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_ltgt_gradle_incap_incap',
              'version': 'version:2@0.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/net_sf_kxml_kxml2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_sf_kxml_kxml2',
              'version': 'version:2@2.3.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup',
              'version': 'version:2@1.2.1.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2@2.5.5.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_checkerframework_checker_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_qual',
              'version': 'version:2@3.12.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_checkerframework_dataflow_errorprone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_dataflow_errorprone',
              'version': 'version:2@3.15.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations',
              'version': 'version:2@1.17.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_eclipse_jgit_org_eclipse_jgit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_eclipse_jgit_org_eclipse_jgit',
              'version': 'version:2@4.4.1.201607150455-r.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_annotations',
              'version': 'version:2@13.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib',
              'version': 'version:2@1.6.10.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_common',
              'version': 'version:2@1.6.10.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk7',
              'version': 'version:2@1.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk8': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk8',
              'version': 'version:2@1.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_android',
              'version': 'version:2@1.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm',
              'version': 'version:2@1.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm',
              'version': 'version:2@0.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm',
              'version': 'version:2@9.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm_analysis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_analysis',
              'version': 'version:2@9.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm_commons': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_commons',
              'version': 'version:2@9.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm_tree': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_tree',
              'version': 'version:2@9.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_ow2_asm_asm_util': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_util',
              'version': 'version:2@9.2.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_pcollections_pcollections': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_pcollections_pcollections',
              'version': 'version:2@3.1.4.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_annotations',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_junit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_junit',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_nativeruntime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_nativeruntime',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_pluginapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_pluginapi',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_resources': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_resources',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_robolectric': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_robolectric',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_sandbox': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_sandbox',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_shadowapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadowapi',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_shadows_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_framework',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_shadows_multidex': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_multidex',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_shadows_playservices': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_playservices',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  'third_party/android_deps/libs/org_robolectric_utils_reflector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils_reflector',
              'version': 'version:2@4.7.3.cr1',
          },
      ],
      'condition': 'checkout_android and not build_with_chromium',
      'dep_type': 'cipd',
  },

  # === ANDROID_DEPS Generated Code End ===
}

hooks = [
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [
        'python3',
        'third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },

  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
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
    'condition': 'host_os == "mac" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
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
    'condition': 'host_os == "linux" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and ((checkout_x86 or checkout_x64) and not build_with_chromium)',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x64 and not build_with_chromium)',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': ['python3', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac and not build_with_chromium',
    'action': ['python3', 'build/mac_toolchain.py'],
  },

  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python3', 'tools/clang/scripts/update.py'],
    'condition': 'not build_with_chromium',
  },

  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': ['python3', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },

  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },

  {
    'name': 'rc_mac',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "mac" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/mac/rc.sha1',
    ],
  },
  {
    'name': 'rc_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux" and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/linux64/rc.sha1',
    ]
  },

  {
    'name': 'fuchsia_sdk',
    'pattern': '.',
    'condition': 'checkout_fuchsia and not build_with_chromium',
    'action': [
      'python3',
      'build/fuchsia/update_sdk.py',
    ],
  },

  # Download glslang validator binary for Linux.
  {
    'name': 'linux_glslang_validator',
    'pattern': '.',
    'condition': 'checkout_linux and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'angle-glslang-validator',
                '-s', 'tools/glslang/glslang_validator.sha1',
    ],
  },

  # Download glslang validator binary for Windows.
  {
    'name': 'win_glslang_validator',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=win32*',
                '--no_auth',
                '--bucket', 'angle-glslang-validator',
                '-s', 'tools/glslang/glslang_validator.exe.sha1',
    ],
  },

  # Download flex/bison binaries for Linux.
  {
    'name': 'linux_flex_bison',
    'pattern': '.',
    'condition': 'checkout_linux and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'angle-flex-bison',
                '-d', 'tools/flex-bison/linux/',
    ],
  },

  # Download flex/bison binaries for Windows.
  {
    'name': 'win_flex_bison',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--platform=win32*',
                '--no_auth',
                '--bucket', 'angle-flex-bison',
                '-d', 'tools/flex-bison/windows/',
    ],
  },
]

recursedeps = [
  'third_party/googletest',
  'third_party/jsoncpp',
  'third_party/vulkan-deps',
]
