gclient_gn_args_file = 'src/build/config/gclient_args.gni'
gclient_gn_args = [
  'generate_location_tags',
]

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': '829c6df33dce1085a61d8fd44209fc84bbf9a6a7',
  'gn_version': 'git_revision:6f13aaac55a977e1948910942675c69f2b4f7a94',

  # Keep the Chromium default of generating location tags.
  'generate_location_tags': True,
}

deps = {
  'src/build':
    Var('chromium_git') + '/chromium/src/build' + '@' + 'dcea3443035f48d58193788e0bc56daca4e5db33',
  'src/buildtools':
    Var('chromium_git') + '/chromium/src/buildtools' + '@' + '075dd7e22837a69189003e4fa84499acf63188cf',
  'src/testing':
    Var('chromium_git') + '/chromium/src/testing' + '@' + 'f4e42be13265ec304b0f3085eee2b15f30f44077',
  'src/third_party':
    Var('chromium_git') + '/chromium/src/third_party' + '@' + '42c249feeb71bc0cd184849f0509aefef599343d',

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
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' + '@' + '99876cacf78329e5f99c244dbe42ccd1654517a0',
  'src/buildtools/third_party/libc++/trunk':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + '79a2e924d96e2fc1e4b937c42efd08898fa472d7',
  'src/buildtools/third_party/libc++abi/trunk':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + '665b74f7d1b3bb295cd6ba7d8fcec1acd3d2ac84',
  'src/buildtools/third_party/libunwind/trunk':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/libunwind.git' + '@' + 'f51a154281bdfe746c46c07cd4fb05be97f9441d',

  'src/third_party/catapult':
    Var('chromium_git') + '/catapult.git' + '@' + '75423c310eb303d28978be892fcf7b9c2c824909',
  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/depot_tools':
    Var('chromium_git') + '/chromium/tools/depot_tools.git' + '@' + '2ffa1bde797a8127c0f72908d0bd74051fd65d0d',
  'src/third_party/freetype/src':
    Var('chromium_git') + '/chromium/src/third_party/freetype2.git' + '@' + 'cff026d41599945498044d2f4dcc0e610ffb6929',
  'src/third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + 'e2f3978937c0244508135f126e2617a7734a68be',
  'src/third_party/harfbuzz-ng/src':
    Var('chromium_git') + '/external/github.com/harfbuzz/harfbuzz.git' + '@' + '64b29dbd5994a511acee69cb9b45ad650ef88359',
  'src/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + '02959c3ee17abacfd1339ec22ea93301292ffd56',
  'src/third_party/nasm':
    Var('chromium_git') + '/chromium/deps/nasm.git' + '@' + '9215e8e1d0fe474ffd3e16c1a07a0f97089e6224',
  'src/tools':
    Var('chromium_git') + '/chromium/src/tools' + '@' + '198dc879529652b39ba6e223bcc0bcad5f1facd6',

  # libyuv-only dependencies (not present in Chromium).
  'src/third_party/gtest-parallel':
    Var('chromium_git') + '/external/webrtc/deps/third_party/gtest-parallel' + '@' + '1dad0e9f6d82ff994130b529d7d814b40eb32b0e',

  'src/third_party/lss': {
    'url': Var('chromium_git') + '/linux-syscall-support.git' + '@' + '92a65a8f5d705d1928874420c8d0d15bde8c89e5',
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
    'url': Var('chromium_git') + '/external/github.com/google/auto.git' + '@' + 'fe67d853d6356943dc79541c892ab6d3e6a7b61a',
    'condition': 'checkout_android',
  },
  'src/third_party/boringssl/src':
    'https://boringssl.googlesource.com/boringssl.git' + '@' + '3a667d10e94186fd503966f5638e134fe9fb4080',
  'src/base': {
    'url': Var('chromium_git') + '/chromium/src/base' + '@' + 'e9e639622449a893a1b5e32781d072cec08ead72',
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
    'url': Var('chromium_git') + '/android_ndk.git' + '@' + '401019bf85744311b26c88ced255cd53401af8b7',
    'condition': 'checkout_android',
  },
  'src/third_party/androidx': {
    'packages': [
      {
          'package': 'chromium/third_party/androidx',
          'version': '6d8ij5pzYh29WWjPbdbAWFBJSA1nUgkWf2p6wCVZKIsC',
      },
    ],
    'condition': 'checkout_android',
    'dep_type': 'cipd',
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
              'package': 'chromium/third_party/android_sdk/public/build-tools/31.0.0',
              'version': 'tRoD45SCi7UleQqSV7MrMQO1_e5P8ysphkCcj6z_cCQC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': 'gMHhUuoQRKfxr-MBn3fNNXZtkAVXtOwMwT7kfx8jkIgC',
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
              'version': 'g7n_-r6yJd_SGRklujGB1wEt8iyr77FZTUJVS9w6O34C',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-31',
              'version': 'lL3IGexKjYlwjO_1Ga-xwxgwbE_w-lmi2Zi1uOlWUIAC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/sources/android-31',
              'version': '_a_BcnANjPYw5mSKlNHa7GFY8yc1kdqj2rmQgac7yUcC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': 'Ez2NWws2SJYCF6qw2O-mSCqK6424l3ZdSTpppLyVR_cC',
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
    'url': Var('chromium_git') + '/chromium/deps/icu.git' + '@' + 'bf66d373ae781a3498f2babe7b61d933dd774b82',
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
      'url': Var('chromium_git') + '/chromium/src/third_party/libunwindstack.git' + '@' + '6868358481bb1e5e20d155c1084dc436c88b5e6b',
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
              'version': 'Nu_mvQJe34CotIXadFlA3w732CJ9EvQGuVs4udcZedAC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/proguard': {
      'packages': [
          {
              'package': 'chromium/third_party/proguard',
              'version': 'Fd91BJFVlmiO6c46YMTsdy7n2f5Sk2hVVGlzPLvqZPsC',
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
              'version': 'iC6RDM5EH3GEAzR-1shW_Mg0FeeNE5shq1okkFfuuNQC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
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
              'version': 'Om6yIEXgJxuqghErK29h9RcMH6VaymMbxwScwXmcN6EC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/ub-uiautomator/lib': {
    'url': Var('chromium_git') + '/chromium/third_party/ub-uiautomator.git' + '@' + '00270549ce3161ae72ceb24712618ea28b4f9434',
    'condition': 'checkout_android',
  },

  # iOS deps:
  'src/ios': {
    'url': Var('chromium_git') + '/chromium/src/ios' + '@' + '81826d980c159f949c2c7901f4dbec9a09788964',
    'condition': 'checkout_ios'
  },

  # Everything coming after this is automatically updated by the auto-roller.
  # === ANDROID_DEPS Generated Code Start ===

  'src/third_party/android_deps/libs/android_arch_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_common',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/android_arch_core_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_core_runtime',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/android_arch_lifecycle_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_common',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/android_arch_lifecycle_common_java8': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_common_java8',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/android_arch_lifecycle_livedata': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_livedata',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/android_arch_lifecycle_livedata_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_livedata_core',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/android_arch_lifecycle_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_runtime',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/android_arch_lifecycle_viewmodel': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/android_arch_lifecycle_viewmodel',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/backport_util_concurrent_backport_util_concurrent': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/backport_util_concurrent_backport_util_concurrent',
              'version': 'version:2@3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/classworlds_classworlds': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/classworlds_classworlds',
              'version': 'version:2@1.1-alpha-2.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_animated_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_animated_vector_drawable',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_appcompat_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_appcompat_v7',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_asynclayoutinflater': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_asynclayoutinflater',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_cardview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_cardview_v7',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_collections': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_collections',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_coordinatorlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_coordinatorlayout',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_cursoradapter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_cursoradapter',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_customview': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_customview',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_design': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_design',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_documentfile': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_documentfile',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_drawerlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_drawerlayout',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_interpolator': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_interpolator',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_loader': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_loader',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_localbroadcastmanager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_localbroadcastmanager',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_multidex': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_multidex',
              'version': 'version:2@1.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_print': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_print',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_recyclerview_v7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_recyclerview_v7',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_slidingpanelayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_slidingpanelayout',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_support_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_annotations',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_support_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_compat',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_support_core_ui': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_ui',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_support_core_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_core_utils',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_support_fragment': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_fragment',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_support_media_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_media_compat',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_support_v4': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_v4',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_support_vector_drawable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_vector_drawable',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_swiperefreshlayout': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_swiperefreshlayout',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_transition': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_transition',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_versionedparcelable': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_versionedparcelable',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_support_viewpager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_viewpager',
              'version': 'version:2@28.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_tools_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_common',
              'version': 'version:2@30.0.0-alpha10.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs_configuration': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_desugar_jdk_libs_configuration',
              'version': 'version:2@1.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api',
              'version': 'version:2@30.0.0-alpha10.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_android_tools_sdk_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_sdk_common',
              'version': 'version:2@30.0.0-alpha10.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_github_ben_manes_caffeine_caffeine': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_github_ben_manes_caffeine_caffeine',
              'version': 'version:2@2.8.8.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_github_kevinstern_software_and_algorithms': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_github_kevinstern_software_and_algorithms',
              'version': 'version:2@1.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_datatransport_transport_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_datatransport_transport_api',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone',
              'version': 'version:2@17.5.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_base',
              'version': 'version:2@17.5.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_basement': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_basement',
              'version': 'version:2@17.5.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_cast': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_cloud_messaging': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cloud_messaging',
              'version': 'version:2@16.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_fido': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_fido',
              'version': 'version:2@19.0.0-beta.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_flags': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_flags',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_gcm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_gcm',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_iid',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_location': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_location',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_places_placereport': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_places_placereport',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_stats': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_stats',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_tasks': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_tasks',
              'version': 'version:2@17.2.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_vision': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision',
              'version': 'version:2@18.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common',
              'version': 'version:2@18.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_material_material': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_material_material',
              'version': 'version:2@1.6.0-alpha01.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_android_play_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_play_core',
              'version': 'version:2@1.10.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_auto_auto_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_auto_common',
              'version': 'version:2@1.1.2.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_auto_service_auto_service': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service',
              'version': 'version:2@1.0-rc6.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_auto_service_auto_service_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service_annotations',
              'version': 'version:2@1.0-rc6.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_auto_value_auto_value_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_value_auto_value_annotations',
              'version': 'version:2@1.7.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_code_findbugs_jformatstring': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_findbugs_jformatstring',
              'version': 'version:2@3.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_code_findbugs_jsr305': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_findbugs_jsr305',
              'version': 'version:2@3.0.2.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_code_gson_gson': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_gson_gson',
              'version': 'version:2@2.8.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_dagger_dagger': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger',
              'version': 'version:2@2.30.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_dagger_dagger_compiler': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_compiler',
              'version': 'version:2@2.30.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_dagger_dagger_producers': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_producers',
              'version': 'version:2@2.30.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_dagger_dagger_spi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger_spi',
              'version': 'version:2@2.30.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_annotation': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotation',
              'version': 'version:2@2.10.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations',
              'version': 'version:2@2.10.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_check_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_check_api',
              'version': 'version:2@2.10.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_core',
              'version': 'version:2@2.10.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_errorprone_error_prone_type_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_type_annotations',
              'version': 'version:2@2.10.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_errorprone_javac': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac',
              'version': 'version:2@9+181-r4173-1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_errorprone_javac_shaded': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_javac_shaded',
              'version': 'version:2@9-dev-r4023-3.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_annotations',
              'version': 'version:2@16.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_common',
              'version': 'version:2@19.5.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_components': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_components',
              'version': 'version:2@16.1.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_encoders': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_encoders',
              'version': 'version:2@16.1.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_encoders_json': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_encoders_json',
              'version': 'version:2@17.1.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_iid',
              'version': 'version:2@21.0.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_iid_interop': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_iid_interop',
              'version': 'version:2@17.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_installations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_installations',
              'version': 'version:2@16.3.5.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_installations_interop': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_installations_interop',
              'version': 'version:2@16.0.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_measurement_connector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_measurement_connector',
              'version': 'version:2@18.0.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_firebase_firebase_messaging': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_messaging',
              'version': 'version:2@21.0.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_flatbuffers_flatbuffers_java': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_flatbuffers_flatbuffers_java',
              'version': 'version:2@2.0.3.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_googlejavaformat_google_java_format': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_googlejavaformat_google_java_format',
              'version': 'version:2@1.5.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_guava_failureaccess': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_failureaccess',
              'version': 'version:2@1.0.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_guava_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava',
              'version': 'version:2@31.0-jre.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_guava_guava_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava_android',
              'version': 'version:2@31.0-android.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_guava_listenablefuture': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_listenablefuture',
              'version': 'version:2@1.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations',
              'version': 'version:2@1.3.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_protobuf_protobuf_java': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_java',
              'version': 'version:2@3.4.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_google_protobuf_protobuf_javalite': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_javalite',
              'version': 'version:2@3.13.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils',
              'version': 'version:2@1.3.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:2@1.13.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/com_squareup_javawriter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javawriter',
              'version': 'version:2@2.1.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/io_github_java_diff_utils_java_diff_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_github_java_diff_utils_java_diff_utils',
              'version': 'version:2@4.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/javax_annotation_javax_annotation_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_javax_annotation_api',
              'version': 'version:2@1.3.2.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/javax_annotation_jsr250_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_jsr250_api',
              'version': 'version:2@1.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/javax_inject_javax_inject': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_inject_javax_inject',
              'version': 'version:2@1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/nekohtml_nekohtml': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/nekohtml_nekohtml',
              'version': 'version:2@1.9.6.2.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/nekohtml_xercesminimal': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/nekohtml_xercesminimal',
              'version': 'version:2@1.9.6.2.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/net_ltgt_gradle_incap_incap': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_ltgt_gradle_incap_incap',
              'version': 'version:2@0.2.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/net_sf_kxml_kxml2': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_sf_kxml_kxml2',
              'version': 'version:2@2.3.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_ant_ant': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_ant_ant',
              'version': 'version:2@1.8.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_ant_ant_launcher': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_ant_ant_launcher',
              'version': 'version:2@1.8.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_ant_tasks': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_ant_tasks',
              'version': 'version:2@2.1.3.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_artifact': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_artifact',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_artifact_manager': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_artifact_manager',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_error_diagnostics': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_error_diagnostics',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_model': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_model',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_plugin_registry': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_plugin_registry',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_profile': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_profile',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_project': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_project',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_repository_metadata': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_repository_metadata',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_maven_settings': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_maven_settings',
              'version': 'version:2@2.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_wagon_wagon_file': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_wagon_wagon_file',
              'version': 'version:2@1.0-beta-6.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_wagon_wagon_http_lightweight': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_wagon_wagon_http_lightweight',
              'version': 'version:2@1.0-beta-6.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_wagon_wagon_http_shared': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_wagon_wagon_http_shared',
              'version': 'version:2@1.0-beta-6.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_apache_maven_wagon_wagon_provider_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_apache_maven_wagon_wagon_provider_api',
              'version': 'version:2@1.0-beta-6.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup',
              'version': 'version:2@1.2.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2@2.5.5.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_checkerframework_checker_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_qual',
              'version': 'version:2@3.12.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_checkerframework_dataflow_errorprone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_dataflow_errorprone',
              'version': 'version:2@3.15.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations',
              'version': 'version:2@1.17.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_codehaus_plexus_plexus_container_default': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_plexus_plexus_container_default',
              'version': 'version:2@1.0-alpha-9-stable-1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_codehaus_plexus_plexus_interpolation': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_plexus_plexus_interpolation',
              'version': 'version:2@1.11.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_codehaus_plexus_plexus_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_plexus_plexus_utils',
              'version': 'version:2@1.5.15.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_eclipse_jgit_org_eclipse_jgit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_eclipse_jgit_org_eclipse_jgit',
              'version': 'version:2@4.4.1.201607150455-r.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_jetbrains_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_annotations',
              'version': 'version:2@13.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib',
              'version': 'version:2@1.6.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_common',
              'version': 'version:2@1.6.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk7': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk7',
              'version': 'version:2@1.5.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk8': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_stdlib_jdk8',
              'version': 'version:2@1.5.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_android',
              'version': 'version:2@1.5.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm',
              'version': 'version:2@1.5.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm',
              'version': 'version:2@0.1.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_ow2_asm_asm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm',
              'version': 'version:2@7.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_ow2_asm_asm_analysis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_analysis',
              'version': 'version:2@7.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_ow2_asm_asm_commons': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_commons',
              'version': 'version:2@7.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_ow2_asm_asm_tree': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_tree',
              'version': 'version:2@7.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_ow2_asm_asm_util': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_util',
              'version': 'version:2@7.0.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_pcollections_pcollections': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_pcollections_pcollections',
              'version': 'version:2@2.1.2.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_annotations',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_junit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_junit',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_pluginapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_pluginapi',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_resources': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_resources',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_robolectric': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_robolectric',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_sandbox': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_sandbox',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_shadowapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadowapi',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_shadows_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_framework',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_shadows_playservices': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_playservices',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils',
              'version': 'version:2@4.3.1.cr0',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/android_deps/libs/org_robolectric_utils_reflector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils_reflector',
              'version': 'version:2@4.3.1.cr0',
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
    'action': ['python3', 'src/cleanup_links.py'],
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
        'python3',
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
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_mips',
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=mips'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python3', 'src/build/vs_toolchain.py', 'update'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'action': ['python3', 'src/build/mac_toolchain.py'],
    'condition': 'checkout_mac',
  },
  {
    'name': 'msan_chained_origins',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                "--no_resume",
                "--no_auth",
                "--bucket", "chromium-instrumented-libraries",
                "-s", "src/third_party/instrumented_libraries/binaries/msan-chained-origins.tgz.sha1",
              ],
  },
  {
    'name': 'msan_no_origins',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                "--no_resume",
                "--no_auth",
                "--bucket", "chromium-instrumented-libraries",
                "-s", "src/third_party/instrumented_libraries/binaries/msan-no-origins.tgz.sha1",
              ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python3', 'src/tools/clang/scripts/update.py'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python3', 'src/build/util/lastchange.py',
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
  {
    'name': 'Generate component metadata for tests',
    'pattern': '.',
    'action': [
      'vpython3',
      'src/testing/generate_location_tags.py',
      '--out',
      'src/testing/location_tags.json',
    ],
  },
]

recursedeps = []
