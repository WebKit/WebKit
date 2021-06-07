gclient_gn_args_file = 'src/build/config/gclient_args.gni'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': '64c8c30faaf969c15c028131dfcd0819208039c1',
  'gn_version': 'git_revision:6f13aaac55a977e1948910942675c69f2b4f7a94',
}

deps = {
  'src/build':
    Var('chromium_git') + '/chromium/src/build' + '@' + '2d2f9f2b85592bb9af5753ef300c055e6feb709f',
  'src/buildtools':
    Var('chromium_git') + '/chromium/src/buildtools' + '@' + '6302c1175607a436e18947a5abe9df2209e845fc',
  'src/testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + '40b44171056045ed1f85ca0b57485e46c03d7867',
  'src/third_party':
    Var('chromium_git') + '/chromium/src/third_party' + '@' + '24ccdf9b7553446791983bf357261c5e0a4314a0',

  'src/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_linux',
  },
  'src/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac',
  },
  'src/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_win',
  },

  'src/buildtools/clang_format/script':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' + '@' + '99803d74e35962f63a775f29477882afd4d57d94',
  'src/buildtools/third_party/libc++/trunk':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + 'd9040c75cfea5928c804ab7c235fed06a63f743a',
  'src/buildtools/third_party/libc++abi/trunk':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + '196ba1aaa8ac285d94f4ea8d9836390a45360533',
  'src/buildtools/third_party/libunwind/trunk':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/libunwind.git' + '@' + 'd999d54f4bca789543a2eb6c995af2d9b5a1f3ed',

  'src/third_party/catapult':
    Var('chromium_git') + '/catapult.git' + '@' + 'ccc9dd2835f5a7c5c82ae3c1a2fbc2fe2fd9dfd1',
  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/depot_tools':
    Var('chromium_git') + '/chromium/tools/depot_tools.git' + '@' + '91bb7506bd20ed22b8787e7a8b9975cc07e97175',
  'src/third_party/freetype/src':
    Var('chromium_git') + '/chromium/src/third_party/freetype2.git' + '@' + '26e2a89598d69c7aba76c83f6a1fcf1db17574ab',
  'src/third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + '4fe018038f87675c083d0cfb6a6b57c274fb1753',
  'src/third_party/harfbuzz-ng/src':
    Var('chromium_git') + '/external/github.com/harfbuzz/harfbuzz.git' + '@' + 'c39ab82c90479341dcf28eaa8174af6f08c0d7ae',
  'src/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + 'd5148db386ceb4a608058320071cbed890bd6ad2',
  'src/third_party/nasm':
    Var('chromium_git') + '/chromium/deps/nasm.git' + '@' + '19f3fad68da99277b2882939d3b2fa4c4b8d51d9',
  'src/tools':
    Var('chromium_git') + '/chromium/src/tools' + '@' + '1bb7c085e67a0fc8c63511af83299d1632f5a3f3',
  'src/tools/swarming_client':
    Var('chromium_git') + '/infra/luci/client-py.git' + '@' + 'd46ea7635f2911208268170512cb611412488fd8',

  # libyuv-only dependencies (not present in Chromium).
  'src/third_party/gtest-parallel':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gtest-parallel' + '@' + '1dad0e9f6d82ff994130b529d7d814b40eb32b0e',

  'src/third_party/lss': {
    'url': Var('chromium_git') + '/linux-syscall-support.git' + '@' + '29f7c7e018f4ce706a709f0b0afbf8bacf869480',
    'condition': 'checkout_android or checkout_linux',
  },

  # Android deps:
  'src/third_party/accessibility_test_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/accessibility-test-framework',
              'version': 'b5ec1e56e58e56bc1a0c77d43111c37f9b512c8a',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/auto/src': {
    'url': Var('chromium_git') + '/external/github.com/google/auto.git' + '@' + 'f40317ae215863102cf87fe0679ad66f4b19454e',
    'condition': 'checkout_android',
  },
  'src/third_party/boringssl/src':
    'https://boringssl.googlesource.com/boringssl.git' + '@' + '1607f54fed72c6589d560254626909a64124f091',
  'src/base': {
    'url': Var('chromium_git') + '/chromium/src/base' + '@' + 'e096814b0448fba1095c6e7be7c7a0b5d7264251',
    'condition': 'checkout_android',
  },
  'src/third_party/bazel': {
      'packages': [
          {
              'package': 'chromium/third_party/bazel',
              'version': 'VjMsf48QUWw8n7XtJP2AuSjIGmbQeYdWdwyxVvIRLmAC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/bouncycastle': {
      'packages': [
          {
              'package': 'chromium/third_party/bouncycastle',
              'version': 'c078e87552ba26e776566fdaf0f22cd8712743d0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_ndk': {
    'url': Var('chromium_git') + '/android_ndk.git' + '@' + '27c0a8d090c666a50e40fceb4ee5b40b1a2d3f87',
    'condition': 'checkout_android',
  },
  'src/third_party/android_support_test_runner': {
      'packages': [
          {
              'package': 'chromium/third_party/android_support_test_runner',
              'version': '96d4bf848cd210fdcbca6bcc8c1b4b39cbd93141',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/30.0.1',
              'version': '8LZujEmLjSh0g3JciDA3cslSptxKs9HOa_iUPXkOeYQC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': 'ijpIFSitwBfaEdO9VXBGPqDHUVzPimXy_whw3aHTN9oC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': 'A4EvXZUIuQho0QRDJopMUpgyp6NA3aiDQjGKPUKbowMC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/extras',
              'version': 'ppQ4TnqDvBHQ3lXx5KPq97egzF5X2FFyOrVHkGmiTMQC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/patcher',
              'version': 'I6FNMhrXlpB-E1lOhMlvld7xt9lBVNOO83KIluXDyA0C',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platform-tools',
              'version': '8tF0AOj7Dwlv4j7_nfkhxWB0jzrvWWYjEIpirt8FIWYC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-30',
              'version': 'YMUu9EHNZ__2Xcxl-KsaSf-dI5TMt_P62IseUVsxktMC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/sources/android-29',
              'version': '4gxhM8E62bvZpQs7Q3d0DinQaW0RLCIefhXrQBFkNy8C',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/tools/clang/dsymutil': {
    'packages': [
      {
        'package': 'chromium/llvm-build-tools/dsymutil',
        'version': 'OWlhXkmj18li3yhJk59Kmjbc5KdgLh56TwCd1qBdzlIC',
      }
    ],
    'condition': 'checkout_mac',
    'dep_type': 'cipd',
  },
  'src/third_party/android_build_tools/aapt2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2',
              'version': 'version:3.6.0-alpha03-5516695-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/byte_buddy': {
      'packages': [
          {
              'package': 'chromium/third_party/byte_buddy',
              'version': 'c9b53316603fc2d997c899c7ca1707f809b918cd',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/ced/src': {
    'url': Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + 'ba412eaaacd3186085babcd901679a48863c7dd5',
    'condition': 'checkout_android',
  },
  'src/third_party/errorprone/lib': {
      'url': Var('chromium_git') + '/chromium/third_party/errorprone.git' + '@' + '980d49e839aa4984015efed34b0134d4b2c9b6d7',
      'condition': 'checkout_android',
  },
  'src/third_party/findbugs': {
    'url': Var('chromium_git') + '/chromium/deps/findbugs.git' + '@' + '4275d9ac8610db6b1bc9a5e887f97e41b33fac67',
    'condition': 'checkout_android',
  },
  'src/third_party/gson': {
      'packages': [
          {
              'package': 'chromium/third_party/gson',
              'version': '681931c9778045903a0ed59856ce2dd8dd7bf7ca',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/guava': {
      'packages': [
          {
              'package': 'chromium/third_party/guava',
              'version': 'a6fba501f3a0de88b9be1daa2052632de5b96a46',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/hamcrest': {
      'packages': [
          {
              'package': 'chromium/third_party/hamcrest',
              'version': '37eccfc658fe79695d6abb6dd497463c4372032f',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/icu': {
    'url': Var('chromium_git') + '/chromium/deps/icu.git' + '@' + 'c2a4cae149aae7fd30c4cbe3cf1b30df03b386f1',
  },
  'src/third_party/icu4j': {
      'packages': [
          {
              'package': 'chromium/third_party/icu4j',
              'version': 'e87e5bed2b4935913ee26a3ebd0b723ee2344354',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/intellij': {
      'packages': [
          {
              'package': 'chromium/third_party/intellij',
              'version': '77c2721b024b36ee073402c08e6d8428c0295336',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/jdk': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk',
              'version': 'PfRSnxe8Od6WU4zBXomq-zsgcJgWmm3z4gMQNB-r2QcC',
          },
          {
              'package': 'chromium/third_party/jdk/extras',
              'version': 'fkhuOQ3r-zKtWEdKplpo6k0vKkjl-LY_rJTmtzFCQN4C',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/jsr-305/src': {
    'url': Var('chromium_git') + '/external/jsr-305.git' + '@' + '642c508235471f7220af6d5df2d3210e3bfc0919',
    'condition': 'checkout_android',
  },
  'src/third_party/junit/src': {
    'url': Var('chromium_git') + '/external/junit.git' + '@' + '64155f8a9babcfcf4263cf4d08253a1556e75481',
    'condition': 'checkout_android',
  },
  'src/third_party/libunwindstack': {
      'url': Var('chromium_git') + '/chromium/src/third_party/libunwindstack.git' + '@' + '11659d420a71e7323b379ea8781f07c6f384bc7e',
      'condition': 'checkout_android',
  },
  'src/third_party/mockito/src': {
    'url': Var('chromium_git') + '/external/mockito/mockito.git' + '@' + '04a2a289a4222f80ad20717c25144981210d2eac',
    'condition': 'checkout_android',
  },
  'src/third_party/objenesis': {
      'packages': [
          {
              'package': 'chromium/third_party/objenesis',
              'version': '9e367f55e5a65781ee77bfcbaa88fb82b30e75c0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/ow2_asm': {
      'packages': [
          {
              'package': 'chromium/third_party/ow2_asm',
              'version': 'NNAhdJzMdnutUVqfSJm5v0tVazA9l3Dd6CRwH6N4Q5kC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/r8': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'N9LppKV-9lFkp7JQtmcLHhm7xHqFv0SPa6aDPtgNCdwC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/proguard': {
      'packages': [
          {
              'package': 'chromium/third_party/proguard',
              'version': '3bd778c422ea5496de2ef25c007a517dbb5ce5ca',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/requests/src': {
    'url': Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'refs/tags/v2.23.0',
    'condition': 'checkout_android',
  },
  'src/third_party/robolectric': {
      'packages': [
          {
              'package': 'chromium/third_party/robolectric',
              'version': '1KXoOiNP1a_uZNdM2ybWKwAQNow1dHTXTig-ZK4Xgq8C',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/robolectric/robolectric': {
    'url': Var('chromium_git') + '/external/robolectric.git' + '@' + '2f3e0a3ac450a17dbf2e7d4eaab3a1f14dda50e6',
    'condition': 'checkout_android',
  },
  'src/third_party/sqlite4java': {
      'packages': [
          {
              'package': 'chromium/third_party/sqlite4java',
              'version': '889660698187baa7c8b0d79f7bf58563125fbd66',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/turbine': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': 'O_jNDJ4VdwYKBSDbd2BJ3mknaTFoVkvE7Po8XIiKy8sC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/turbine/src': {
      'url': Var('chromium_git') + '/external/github.com/google/turbine.git' + '@' + '0f2a5024fe4a9bb745bcd5ac7c655cebe11649bc',
      'condition': 'checkout_android',
  },
  'src/third_party/ub-uiautomator/lib': {
    'url': Var('chromium_git') + '/chromium/third_party/ub-uiautomator.git' + '@' + '00270549ce3161ae72ceb24712618ea28b4f9434',
    'condition': 'checkout_android',
  },
  'src/third_party/xstream': {
      'packages': [
          {
              'package': 'chromium/third_party/xstream',
              'version': '4278b1b78b86ab7a1a29e64d5aec9a47a9aab0fe',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  # iOS deps:
  'src/ios': {
    'url': Var('chromium_git') + '/chromium/src/ios' + '@' + '60ef55beac67e3c0eda1c35ab7944c786b377313',
    'condition': 'checkout_ios'
  },

  # === ANDROID_DEPS Generated Code Start ===
  # Generated by //third_party/android_deps/fetch_all.py
  'src/third_party/android_deps/libs/android_arch_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_common',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_core_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_runtime',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_lifecycle_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_common',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_lifecycle_common_java8': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_common_java8',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_lifecycle_livedata': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_livedata',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_lifecycle_livedata_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_livedata_core',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_lifecycle_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_runtime',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/android_arch_lifecycle_viewmodel': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_viewmodel',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_activity_activity': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_activity_activity',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_annotation_annotation': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_annotation_annotation',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_annotation_annotation_experimental': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_annotation_annotation_experimental',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_appcompat_appcompat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_appcompat_appcompat',
              'version': 'version:1.2.0-beta01-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_appcompat_appcompat_resources': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_appcompat_appcompat_resources',
              'version': 'version:1.2.0-beta01-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_arch_core_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_arch_core_core_common',
              'version': 'version:2.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_arch_core_core_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_arch_core_core_runtime',
              'version': 'version:2.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_asynclayoutinflater_asynclayoutinflater': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_asynclayoutinflater_asynclayoutinflater',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_cardview_cardview': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_cardview_cardview',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_collection_collection': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_collection_collection',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_concurrent_concurrent_futures': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_concurrent_concurrent_futures',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_coordinatorlayout_coordinatorlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_coordinatorlayout_coordinatorlayout',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_core_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_core_core',
              'version': 'version:1.3.0-beta01-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_cursoradapter_cursoradapter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_cursoradapter_cursoradapter',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_customview_customview': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_customview_customview',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_documentfile_documentfile': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_documentfile_documentfile',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_drawerlayout_drawerlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_drawerlayout_drawerlayout',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_exifinterface_exifinterface': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_exifinterface_exifinterface',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_fragment_fragment': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_fragment_fragment',
              'version': 'version:1.2.5-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_gridlayout_gridlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_gridlayout_gridlayout',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_interpolator_interpolator': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_interpolator_interpolator',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_leanback_leanback': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_leanback_leanback',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_leanback_leanback_preference': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_leanback_leanback_preference',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_legacy_legacy_preference_v14': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_legacy_legacy_preference_v14',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_legacy_legacy_support_core_ui': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_legacy_legacy_support_core_ui',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_legacy_legacy_support_core_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_legacy_legacy_support_core_utils',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_legacy_legacy_support_v13': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_legacy_legacy_support_v13',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_legacy_legacy_support_v4': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_legacy_legacy_support_v4',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_lifecycle_lifecycle_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_lifecycle_lifecycle_common',
              'version': 'version:2.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_lifecycle_lifecycle_common_java8': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_lifecycle_lifecycle_common_java8',
              'version': 'version:2.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_lifecycle_lifecycle_livedata': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_lifecycle_lifecycle_livedata',
              'version': 'version:2.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_lifecycle_lifecycle_livedata_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_lifecycle_lifecycle_livedata_core',
              'version': 'version:2.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_lifecycle_lifecycle_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_lifecycle_lifecycle_runtime',
              'version': 'version:2.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_lifecycle_lifecycle_viewmodel': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_lifecycle_lifecycle_viewmodel',
              'version': 'version:2.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_lifecycle_lifecycle_viewmodel_savedstate': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_lifecycle_lifecycle_viewmodel_savedstate',
              'version': 'version:2.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_loader_loader': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_loader_loader',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_localbroadcastmanager_localbroadcastmanager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_localbroadcastmanager_localbroadcastmanager',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_media_media': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_media_media',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_mediarouter_mediarouter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_mediarouter_mediarouter',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_multidex_multidex': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_multidex_multidex',
              'version': 'version:2.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_palette_palette': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_palette_palette',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_preference_preference': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_preference_preference',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_print_print': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_print_print',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_recyclerview_recyclerview': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_recyclerview_recyclerview',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_savedstate_savedstate': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_savedstate_savedstate',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_slice_slice_builders': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_slice_slice_builders',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_slice_slice_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_slice_slice_core',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_slidingpanelayout_slidingpanelayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_slidingpanelayout_slidingpanelayout',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_swiperefreshlayout_swiperefreshlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_swiperefreshlayout_swiperefreshlayout',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_core',
              'version': 'version:1.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_espresso_espresso_contrib': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_espresso_espresso_contrib',
              'version': 'version:3.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_espresso_espresso_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_espresso_espresso_core',
              'version': 'version:3.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_espresso_espresso_idling_resource': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_espresso_espresso_idling_resource',
              'version': 'version:3.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_espresso_espresso_intents': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_espresso_espresso_intents',
              'version': 'version:3.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_espresso_espresso_web': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_espresso_espresso_web',
              'version': 'version:3.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_ext_junit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_ext_junit',
              'version': 'version:1.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_monitor': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_monitor',
              'version': 'version:1.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_rules': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_rules',
              'version': 'version:1.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_runner': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_runner',
              'version': 'version:1.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_test_uiautomator_uiautomator': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_test_uiautomator_uiautomator',
              'version': 'version:2.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_transition_transition': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_transition_transition',
              'version': 'version:1.2.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_tvprovider_tvprovider': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_tvprovider_tvprovider',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_vectordrawable_vectordrawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_vectordrawable_vectordrawable',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_vectordrawable_vectordrawable_animated': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_vectordrawable_vectordrawable_animated',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_versionedparcelable_versionedparcelable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_versionedparcelable_versionedparcelable',
              'version': 'version:1.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_viewpager2_viewpager2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_viewpager2_viewpager2',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_viewpager_viewpager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_viewpager_viewpager',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_webkit_webkit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_webkit_webkit',
              'version': 'version:1.3.0-rc01-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/androidx_window_window': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/androidx_window_window',
              'version': 'version:1.0.0-alpha01-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/backport_util_concurrent_backport_util_concurrent': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/backport_util_concurrent_backport_util_concurrent',
              'version': 'version:3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/classworlds_classworlds': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/classworlds_classworlds',
              'version': 'version:1.1-alpha-2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_animated_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_animated_vector_drawable',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_appcompat_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_appcompat_v7',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_asynclayoutinflater': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_asynclayoutinflater',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_cardview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_cardview_v7',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_collections': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_collections',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_coordinatorlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_coordinatorlayout',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_cursoradapter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_cursoradapter',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_customview': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_customview',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_design': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_design',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_documentfile': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_documentfile',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_drawerlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_drawerlayout',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_gridlayout_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_gridlayout_v7',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_interpolator': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_interpolator',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_leanback_v17': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_leanback_v17',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_loader': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_loader',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_localbroadcastmanager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_localbroadcastmanager',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_mediarouter_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_mediarouter_v7',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_multidex': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_multidex',
              'version': 'version:1.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_palette_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_palette_v7',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_preference_leanback_v17': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_preference_leanback_v17',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_preference_v14': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_preference_v14',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_preference_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_preference_v7',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_print': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_print',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_recyclerview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_recyclerview_v7',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_slidingpanelayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_slidingpanelayout',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_annotations',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_compat',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_core_ui': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_ui',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_core_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_utils',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_fragment': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_fragment',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_media_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_media_compat',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_v13': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_v13',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_v4': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_v4',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_support_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_vector_drawable',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_swiperefreshlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_swiperefreshlayout',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_transition': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_transition',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_versionedparcelable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_versionedparcelable',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_support_viewpager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_viewpager',
              'version': 'version:28.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_tools_build_jetifier_jetifier_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_build_jetifier_jetifier_core',
              'version': 'version:1.0.0-beta08-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_tools_build_jetifier_jetifier_processor': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_build_jetifier_jetifier_processor',
              'version': 'version:1.0.0-beta08-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs',
              'version': 'version:1.0.10-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs_configuration': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs_configuration',
              'version': 'version:1.0.10-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_github_ben_manes_caffeine_caffeine': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_github_ben_manes_caffeine_caffeine',
              'version': 'version:2.8.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_github_kevinstern_software_and_algorithms': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_github_kevinstern_software_and_algorithms',
              'version': 'version:1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone',
              'version': 'version:17.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_base',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_basement': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_basement',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_cast': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_fido': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_fido',
              'version': 'version:18.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_flags': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_flags',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_gcm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_gcm',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_iid',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_location': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_location',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_places_placereport': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_places_placereport',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_stats': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_stats',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_tasks': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_tasks',
              'version': 'version:17.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_vision': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision',
              'version': 'version:18.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common',
              'version': 'version:18.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_android_material_material': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_material_material',
              'version': 'version:1.2.0-alpha06-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_auto_auto_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_auto_common',
              'version': 'version:0.10-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_auto_service_auto_service': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service',
              'version': 'version:1.0-rc6-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_auto_service_auto_service_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service_annotations',
              'version': 'version:1.0-rc6-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_auto_value_auto_value_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_value_auto_value_annotations',
              'version': 'version:1.7-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_code_findbugs_jFormatString': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_findbugs_jformatstring',
              'version': 'version:3.0.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_code_findbugs_jsr305': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_findbugs_jsr305',
              'version': 'version:3.0.2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_code_gson_gson': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_gson_gson',
              'version': 'version:2.8.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_dagger_dagger': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger',
              'version': 'version:2.26-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_dagger_dagger_compiler': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_compiler',
              'version': 'version:2.26-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_dagger_dagger_producers': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_producers',
              'version': 'version:2.26-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_dagger_dagger_spi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_spi',
              'version': 'version:2.26-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_annotation': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotation',
              'version': 'version:2.4.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations',
              'version': 'version:2.4.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_check_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_check_api',
              'version': 'version:2.4.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_core',
              'version': 'version:2.4.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_type_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_type_annotations',
              'version': 'version:2.4.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_javac': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac',
              'version': 'version:9+181-r4173-1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_errorprone_javac_shaded': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac_shaded',
              'version': 'version:9-dev-r4023-3-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_googlejavaformat_google_java_format': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_googlejavaformat_google_java_format',
              'version': 'version:1.5-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_guava_failureaccess': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_failureaccess',
              'version': 'version:1.0.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_guava_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava',
              'version': 'version:27.1-jre-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_guava_listenablefuture': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_listenablefuture',
              'version': 'version:1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations',
              'version': 'version:1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_protobuf_protobuf_java': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_java',
              'version': 'version:3.4.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_google_protobuf_protobuf_javalite': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_javalite',
              'version': 'version:3.13.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils',
              'version': 'version:1.3.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:1.11.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/com_squareup_javawriter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javawriter',
              'version': 'version:2.1.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/commons_cli_commons_cli': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/commons_cli_commons_cli',
              'version': 'version:1.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/javax_annotation_javax_annotation_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_javax_annotation_api',
              'version': 'version:1.3.2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/javax_annotation_jsr250_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_jsr250_api',
              'version': 'version:1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/javax_inject_javax_inject': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_inject_javax_inject',
              'version': 'version:1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/nekohtml_nekohtml': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/nekohtml_nekohtml',
              'version': 'version:1.9.6.2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/nekohtml_xercesMinimal': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/nekohtml_xercesminimal',
              'version': 'version:1.9.6.2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/net_ltgt_gradle_incap_incap': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_ltgt_gradle_incap_incap',
              'version': 'version:0.2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/net_sf_kxml_kxml2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_sf_kxml_kxml2',
              'version': 'version:2.3.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_ant_ant': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_ant_ant',
              'version': 'version:1.8.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_ant_ant_launcher': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_ant_ant_launcher',
              'version': 'version:1.8.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_ant_tasks': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_ant_tasks',
              'version': 'version:2.1.3-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_artifact': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_artifact',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_artifact_manager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_artifact_manager',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_error_diagnostics': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_error_diagnostics',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_model': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_model',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_plugin_registry': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_plugin_registry',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_profile': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_profile',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_project': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_project',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_repository_metadata': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_repository_metadata',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_maven_settings': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_settings',
              'version': 'version:2.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_wagon_wagon_file': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_wagon_wagon_file',
              'version': 'version:1.0-beta-6-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_wagon_wagon_http_lightweight': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_wagon_wagon_http_lightweight',
              'version': 'version:1.0-beta-6-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_wagon_wagon_http_shared': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_wagon_wagon_http_shared',
              'version': 'version:1.0-beta-6-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_apache_maven_wagon_wagon_provider_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_wagon_wagon_provider_api',
              'version': 'version:1.0-beta-6-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup',
              'version': 'version:1.2.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2.5.3-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_checkerframework_checker_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_qual',
              'version': 'version:2.10.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_checkerframework_dataflow_shaded': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_dataflow_shaded',
              'version': 'version:3.1.2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations',
              'version': 'version:1.17-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_codehaus_plexus_plexus_container_default': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_plexus_plexus_container_default',
              'version': 'version:1.0-alpha-9-stable-1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_codehaus_plexus_plexus_interpolation': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_plexus_plexus_interpolation',
              'version': 'version:1.11-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_codehaus_plexus_plexus_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_plexus_plexus_utils',
              'version': 'version:1.5.15-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_jdom_jdom2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jdom_jdom2',
              'version': 'version:2.0.6-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_jetbrains_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_annotations',
              'version': 'version:13.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib',
              'version': 'version:1.3.50-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_common',
              'version': 'version:1.3.50-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm',
              'version': 'version:0.1.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_ow2_asm_asm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm',
              'version': 'version:7.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_ow2_asm_asm_analysis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_analysis',
              'version': 'version:7.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_ow2_asm_asm_commons': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_commons',
              'version': 'version:7.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_ow2_asm_asm_tree': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_tree',
              'version': 'version:7.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_ow2_asm_asm_util': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_util',
              'version': 'version:7.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_pcollections_pcollections': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_pcollections_pcollections',
              'version': 'version:2.1.2-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_annotations',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_junit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_junit',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_pluginapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_pluginapi',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_resources': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_resources',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_robolectric': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_robolectric',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_sandbox': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_sandbox',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_shadowapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadowapi',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_shadows_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_framework',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_shadows_multidex': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_multidex',
              'version': 'version:4.3.1-cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_shadows_playservices': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_playservices',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_robolectric_utils_reflector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils_reflector',
              'version': 'version:4.3.1-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/libs/org_threeten_threeten_extra': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_threeten_threeten_extra',
              'version': 'version:1.5.0-cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  # === ANDROID_DEPS Generated Code End ===
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
  # Downloads the current stable linux sysroot to build/linux/ if needed.
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_mips',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=mips'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': ['python', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'src/build/vs_toolchain.py', 'update'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'action': ['python', 'src/build/mac_toolchain.py'],
    'condition': 'checkout_mac',
  },
  # Pull the msan libraries on linux.
  {
    'name': 'msan_chained_origins',
    'pattern': '.',
    'condition': 'checkout_linux',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-instrumented-libraries',
                '-s', 'src/third_party/instrumented_libraries/binaries/msan-chained-origins-trusty.tgz.sha1',
              ],
  },
  {
    'name': 'msan_no_origins',
    'pattern': '.',
    'condition': 'checkout_linux',
    'action': [ 'python',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-instrumented-libraries',
                '-s', 'src/third_party/instrumented_libraries/binaries/msan-no-origins-trusty.tgz.sha1',
              ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'src/tools/clang/scripts/update.py'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE'],
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
    'condition': 'host_os == "linux"',
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
    # We used to use src as a CIPD root. We moved it to a different directory
    # in crrev.com/c/930178 but left the clobber here to ensure that that CL
    # could be reverted safely. This can be safely removed once crbug.com/794764
    # is resolved.
    'name': 'Android Clobber Deprecated CIPD Root',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': ['src/build/cipd/clobber_cipd_root.py',
               '--root', 'src',
    ],
  },
]

recursedeps = []
