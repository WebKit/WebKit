kind: manual

build-depends:
- freedesktop-sdk.bst:components/rust.bst
- freedesktop-sdk.bst:components/pkg-config.bst
- freedesktop-sdk.bst:components/openssl.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libffi.bst

environment:
  PATH: /usr/bin:/usr/lib/sdk/rust/bin

config:
  build-commands:
  - |
    cargo --offline build --features="all" --release
    strip ./target/release/sccache

  install-commands:
  - |
    install -D -m a+rx -t "%{install-root}%{bindir}" ./target/release/sccache

variables:
  strip-binaries: |
    true

sources:
- kind: git_repo
  url: github_com:mozilla/sccache
  track: v0.*
  ref: v0.7.1-0-g1d71d5d810c161ff85217c71608d1d2262c35afb
- kind: cargo
  ref:
  - name: addr2line
    version: 0.19.0
    sha: a76fd60b23679b7d19bd066031410fb7e458ccc5e958eb5c325888ce4baedc97
  - name: adler
    version: 1.0.2
    sha: f26201604c87b1e01bd3d98f8d5d9a8fcbb815e8cedb41ffccbeb4bf593a35fe
  - name: aho-corasick
    version: 1.0.2
    sha: 43f6cb1bf222025340178f382c426f13757b2960e89779dfcb319c32542a5a41
  - name: android-tzdata
    version: 0.1.1
    sha: e999941b234f3131b00bc13c22d06e8c5ff726d1b6318ac7eb276997bbb4fef0
  - name: android_system_properties
    version: 0.1.5
    sha: 819e7219dbd41043ac279b19830f2efc897156490d7fd6ea916720117ee66311
  - name: anstyle
    version: 1.0.0
    sha: 41ed9a86bf92ae6580e0a31281f65a1b1d867c0cc68d5346e2ae128dddfa6a7d
  - name: anyhow
    version: 1.0.71
    sha: 9c7d0618f0e0b7e8ff11427422b64564d5fb0be1940354bfe2e0529b18a9d9b8
  - name: ar
    version: 0.9.0
    sha: d67af77d68a931ecd5cbd8a3b5987d63a1d1d1278f7f6a60ae33db485cdebb69
  - name: arc-swap
    version: 1.5.1
    sha: 983cd8b9d4b02a6dc6ffa557262eb5858a27a0038ffffe21a0f133eaa819a164
  - name: arrayref
    version: 0.3.6
    sha: a4c527152e37cf757a3f78aae5a06fbeefdb07ccc535c980a3208ee3060dd544
  - name: arrayvec
    version: 0.5.2
    sha: 23b62fc65de8e4e7f52534fb52b0f3ed04746ae267519eef2a83941e8085068b
  - name: arrayvec
    version: 0.7.4
    sha: 96d30a06541fbafbc7f82ed10c06164cfbd2c401138f6addd8404629c4b16711
  - name: ascii
    version: 1.1.0
    sha: d92bec98840b8f03a5ff5413de5293bfcd8bf96467cf5452609f939ec6f5de16
  - name: assert_cmd
    version: 2.0.12
    sha: 88903cb14723e4d4003335bb7f8a14f27691649105346a0f0957466c096adfe6
  - name: async-compat
    version: 0.2.1
    sha: 9b48b4ff0c2026db683dea961cd8ea874737f56cffca86fa84415eaddc51c00d
  - name: async-trait
    version: 0.1.74
    sha: a66537f1bb974b254c98ed142ff995236e81b9d0fe4db0575f46612cb15eb0f9
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: backon
    version: 0.4.1
    sha: 0c1a6197b2120bb2185a267f6515038558b019e92b832bb0320e96d66268dcf9
  - name: backtrace
    version: 0.3.67
    sha: 233d376d6d185f2a3093e58f283f60f880315b6c60075b01f36b3b85154564ca
  - name: base64
    version: 0.13.1
    sha: 9e1b586273c5702936fe7b7d6896644d8be71e6314cfe09d3167c95f712589e8
  - name: base64
    version: 0.21.3
    sha: 414dcefbc63d77c526a76b3afcf6fbb9b5e2791c19c3aa2297733208750c6e53
  - name: base64ct
    version: 1.6.0
    sha: 8c3c1a368f70d6cf7302d78f8f7093da241fb8e8807c05cc9e51a125895a6d5b
  - name: bb8
    version: 0.8.0
    sha: 1627eccf3aa91405435ba240be23513eeca466b5dc33866422672264de061582
  - name: bincode
    version: 1.3.3
    sha: b1f45e9417d87227c7a56d22e471c6206462cba514c7590c09aff4cf6d1ddcad
  - name: bitflags
    version: 1.3.2
    sha: bef38d45163c2f1dde094a7dfd33ccf595c92905c8f8f4fdc18d06fb1037718a
  - name: bitflags
    version: 2.0.2
    sha: 487f1e0fcbe47deb8b0574e646def1c903389d95241dd1bbcc6ce4a715dfc0c1
  - name: blake3
    version: 1.5.0
    sha: 0231f06152bf547e9c2b5194f247cd97aacf6dcd8b15d8e5ec0663f64580da87
  - name: block-buffer
    version: 0.10.3
    sha: 69cce20737498f97b993470a6e536b8523f0af7892a4f928cceb1ac5e52ebe7e
  - name: bstr
    version: 1.0.1
    sha: fca0852af221f458706eb0725c03e4ed6c46af9ac98e6a689d5e634215d594dd
  - name: buf_redux
    version: 0.8.4
    sha: b953a6887648bb07a535631f2bc00fbdb2a2216f135552cb3f534ed136b9c07f
  - name: bumpalo
    version: 3.11.1
    sha: 572f695136211188308f16ad2ca5c851a712c464060ae6974944458eb83880ba
  - name: byteorder
    version: 1.5.0
    sha: 1fd0f2584146f6f2ef48085050886acf353beff7305ebd1ae69500e27c67f64b
  - name: bytes
    version: 1.4.0
    sha: 89b2fd2a0dcf38d7971e2194b6b6eebab45ae01067456a7fd93d5547a61b70be
  - name: cc
    version: 1.0.79
    sha: 50d30906286121d95be3d479533b458f87493b30a4b5f79a607db8f5d11aa91f
  - name: cfg-if
    version: 0.1.10
    sha: 4785bdd1c96b2a846b2bd7cc02e86b6b3dbf14e7e53446c4f54c92a361040822
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.31
    sha: 7f2c685bad3eb3d45a01354cedb7d5faa66194d1d58ba6e267a8de788f79db38
  - name: chunked_transfer
    version: 1.4.0
    sha: fff857943da45f546682664a79488be82e69e43c1a7a2307679ab9afb3a66d2e
  - name: clap
    version: 4.1.11
    sha: 42dfd32784433290c51d92c438bb72ea5063797fc3cc9a21a8c4346bebbb2098
  - name: clap_derive
    version: 4.1.9
    sha: fddf67631444a3a3e3e5ac51c36a5e01335302de677bd78759eaa90ab1f46644
  - name: clap_lex
    version: 0.3.0
    sha: 0d4198f73e42b4936b35b5bb248d81d2b595ecb170da0bac7655c54eedfa8da8
  - name: combine
    version: 4.6.6
    sha: 35ed6e9d84f0b51a7f52daf1c7d71dd136fd7a3f41a8462b8cdb8c78d920fad4
  - name: const-oid
    version: 0.9.2
    sha: 520fbf3c07483f94e3e3ca9d0cfd913d7718ef2483d2cfd91c0d9e91474ab913
  - name: const-random
    version: 0.1.15
    sha: 368a7a772ead6ce7e1de82bfb04c485f3db8ec744f72925af5735e29a22cc18e
  - name: const-random-macro
    version: 0.1.15
    sha: 9d7d6ab3c3a2282db210df5f02c4dab6e0a7057af0fb7ebd4070f30fe05c0ddb
  - name: constant_time_eq
    version: 0.3.0
    sha: f7144d30dcf0fafbce74250a3963025d8d52177934239851c917d29f1df280c2
  - name: core-foundation
    version: 0.9.3
    sha: 194a7a9e6de53fa55116934067c844d9d749312f75c6f6d0980e8c252f8c2146
  - name: core-foundation-sys
    version: 0.8.3
    sha: 5827cebf4670468b8772dd191856768aedcb1b0278a04f989f7766351917b9dc
  - name: core_affinity
    version: 0.8.0
    sha: 4436406e93f52cce33bfba4be067a9f7229da44a634c385e4b22cdfaca5f84cc
  - name: cpufeatures
    version: 0.2.5
    sha: 28d997bd5e24a5928dd43e46dc529867e207907fe0b239c3477d924f7f2ca320
  - name: crc16
    version: 0.4.0
    sha: 338089f42c427b86394a5ee60ff321da23a5c89c9d89514c829687b26359fcff
  - name: crc32fast
    version: 1.3.2
    sha: b540bd8bc810d3885c6ea91e2018302f68baba2129ab3e88f32389ee9370880d
  - name: crossbeam-utils
    version: 0.8.15
    sha: 3c063cd8cc95f5c377ed0d4b49a4b21f632396ff690e8470c29b3359b346984b
  - name: crunchy
    version: 0.2.2
    sha: 7a81dae078cea95a014a339291cec439d2f232ebe854a9d672b796c6afafa9b7
  - name: crypto-common
    version: 0.1.6
    sha: 1bfb12502f3fc46cca1bb51ac28df9d618d813cdc3d2f25b9fe775a34af26bb3
  - name: daemonize
    version: 0.5.0
    sha: ab8bfdaacb3c887a54d41bdf48d3af8873b3f5566469f8ba21b92057509f116e
  - name: dashmap
    version: 5.4.0
    sha: 907076dfda823b0b36d2a1bb5f90c96660a5bbcd7729e10727f07858f22c4edc
  - name: data-encoding
    version: 2.3.3
    sha: 23d8666cb01533c39dde32bcbab8e227b4ed6679b2c925eba05feabea39508fb
  - name: der
    version: 0.7.6
    sha: 56acb310e15652100da43d130af8d97b509e95af61aab1c5a7939ef24337ee17
  - name: difflib
    version: 0.4.0
    sha: 6184e33543162437515c2e2b48714794e37845ec9851711914eec9d308f6ebe8
  - name: digest
    version: 0.10.6
    sha: 8168378f4e5023e7218c89c891c0fd8ecdb5e5e4f18cb78f38cf245dd021e76f
  - name: directories
    version: 5.0.1
    sha: 9a49173b84e034382284f27f1af4dcbbd231ffa358c0fe316541a7337f376a35
  - name: dirs-sys
    version: 0.4.1
    sha: 520f05a5cbd335fae5a99ff7a6ab8627577660ee5cfd6a94a6a929b52ff0321c
  - name: displaydoc
    version: 0.1.7
    sha: adc2ab4d5a16117f9029e9a6b5e4e79f4c67f6519bc134210d4d4a04ba31f41b
  - name: dlv-list
    version: 0.5.0
    sha: d529fd73d344663edfd598ccb3f344e46034db51ebd103518eae34338248ad73
  - name: doc-comment
    version: 0.3.3
    sha: fea41bba32d969b513997752735605054bc0dfa92b4c56bf1189f2e174be7a10
  - name: either
    version: 1.8.0
    sha: 90e5c1c8368803113bf0c9584fc495a58b86dc8a29edbf8fe877d21d9507e797
  - name: encoding
    version: 0.2.33
    sha: 6b0d943856b990d12d3b55b359144ff341533e516d94098b1d3fc1ac666d36ec
  - name: encoding-index-japanese
    version: 1.20141219.5
    sha: 04e8b2ff42e9a05335dbf8b5c6f7567e5591d0d916ccef4e0b1710d32a0d0c91
  - name: encoding-index-korean
    version: 1.20141219.5
    sha: 4dc33fb8e6bcba213fe2f14275f0963fd16f0a02c878e3095ecfdf5bee529d81
  - name: encoding-index-simpchinese
    version: 1.20141219.5
    sha: d87a7194909b9118fc707194baa434a4e3b0fb6a5a757c73c3adb07aa25031f7
  - name: encoding-index-singlebyte
    version: 1.20141219.5
    sha: 3351d5acffb224af9ca265f435b859c7c01537c0849754d3db3fdf2bfe2ae84a
  - name: encoding-index-tradchinese
    version: 1.20141219.5
    sha: fd0e20d5688ce3cab59eb3ef3a2083a5c77bf496cb798dc6fcdb75f323890c18
  - name: encoding_index_tests
    version: 0.1.4
    sha: a246d82be1c9d791c5dfde9a2bd045fc3cbba3fa2b11ad558f27d01712f00569
  - name: encoding_rs
    version: 0.8.31
    sha: 9852635589dc9f9ea1b6fe9f05b50ef208c85c834a562f0c6abb1c475736ec2b
  - name: enum-as-inner
    version: 0.5.1
    sha: c9720bba047d567ffc8a3cba48bf19126600e249ab7f128e9233e6376976a116
  - name: env_logger
    version: 0.10.0
    sha: 85cdab6a89accf66733ad5a1693a4dcced6aeff64602b634530dd73c1f3ee9f0
  - name: equivalent
    version: 1.0.1
    sha: 5443807d6dff69373d433ab9ef5378ad8df50ca6298caf15de6e52e24aaf54d5
  - name: errno
    version: 0.3.1
    sha: 4bcfec3a70f97c962c307b2d2c56e358cf1d00b558d74262b5f929ee8cc7e73a
  - name: errno-dragonfly
    version: 0.1.2
    sha: aa68f1b12764fab894d2755d2518754e71b4fd80ecfb822714a1206c2aab39bf
  - name: error-chain
    version: 0.12.4
    sha: 2d2f06b9cac1506ece98fe3231e3cc9c4410ec3d5b1f24ae1c8946f0742cdefc
  - name: fastrand
    version: 1.9.0
    sha: e51093e27b0797c359783294ca4f0a911c270184cb10f85783b118614a1501be
  - name: filetime
    version: 0.2.22
    sha: d4029edd3e734da6fe05b6cd7bd2960760a616bd2ddd0d59a0124746d6272af0
  - name: flagset
    version: 0.4.3
    sha: cda653ca797810c02f7ca4b804b40b8b95ae046eb989d356bce17919a8c25499
  - name: flate2
    version: 1.0.28
    sha: 46303f565772937ffe1d394a4fac6f411c6013172fadde9dcdb1e147a086940e
  - name: float-cmp
    version: 0.9.0
    sha: 98de4bbd547a563b716d8dfa9aad1cb19bfab00f4fa09a6a4ed21dbcf44ce9c4
  - name: flume
    version: 0.10.14
    sha: 1657b4441c3403d9f7b3409e47575237dac27b1b5726df654a6ecbf92f0f7577
  - name: fnv
    version: 1.0.7
    sha: 3f9eec918d3f24069decb9af1554cad7c880e2da24a9afd88aca000531ab82c1
  - name: foreign-types
    version: 0.3.2
    sha: f6f339eb8adc052cd2ca78910fda869aefa38d22d5cb648e6485e4d3fc06f3b1
  - name: foreign-types-shared
    version: 0.1.1
    sha: 00b0228411908ca8685dba7fc2cdd70ec9990a6e753e89b6ac91a84c40fbaf4b
  - name: form_urlencoded
    version: 1.1.0
    sha: a9c384f161156f5260c24a097c56119f9be8c798586aecc13afbcbe7b7e26bf8
  - name: fs-err
    version: 2.9.0
    sha: 0845fa252299212f0389d64ba26f34fa32cfe41588355f21ed507c59a0f64541
  - name: futures
    version: 0.3.27
    sha: 531ac96c6ff5fd7c62263c5e3c67a603af4fcaee2e1a0ae5565ba3a11e69e549
  - name: futures-channel
    version: 0.3.27
    sha: 164713a5a0dcc3e7b4b1ed7d3b433cabc18025386f9339346e8daf15963cf7ac
  - name: futures-core
    version: 0.3.27
    sha: 86d7a0c1aa76363dac491de0ee99faf6941128376f1cf96f07db7603b7de69dd
  - name: futures-executor
    version: 0.3.27
    sha: 1997dd9df74cdac935c76252744c1ed5794fac083242ea4fe77ef3ed60ba0f83
  - name: futures-io
    version: 0.3.27
    sha: 89d422fa3cbe3b40dca574ab087abb5bc98258ea57eea3fd6f1fa7162c778b91
  - name: futures-macro
    version: 0.3.27
    sha: 3eb14ed937631bd8b8b8977f2c198443447a8355b6e3ca599f38c975e5a963b6
  - name: futures-sink
    version: 0.3.27
    sha: ec93083a4aecafb2a80a885c9de1f0ccae9dbd32c2bb54b0c3a65690e0b8d2f2
  - name: futures-task
    version: 0.3.27
    sha: fd65540d33b37b16542a0438c12e6aeead10d4ac5d05bd3f805b8f35ab592879
  - name: futures-util
    version: 0.3.27
    sha: 3ef6b17e481503ec85211fed8f39d1970f128935ca1f814cd32ac4a6842e84ab
  - name: generic-array
    version: 0.14.6
    sha: bff49e947297f3312447abdca79f45f4738097cc82b06e72054d2223f601f1b9
  - name: getrandom
    version: 0.2.8
    sha: c05aeb6a22b8f62540c194aac980f2115af067bfe15a0734d7277a768d396b31
  - name: gimli
    version: 0.27.2
    sha: ad0a93d233ebf96623465aad4046a8d3aa4da22d4f4beba5388838c8a434bbb4
  - name: gzp
    version: 0.11.3
    sha: e7c65d1899521a11810501b50b898464d133e1afc96703cff57726964cfa7baf
  - name: h2
    version: 0.3.17
    sha: 66b91535aa35fea1523ad1b86cb6b53c28e0ae566ba4a460f4457e936cad7c6f
  - name: hashbrown
    version: 0.12.3
    sha: 8a9ee70c43aaf417c914396645a0fa852624801b24ebb7ae78fe8272889ac888
  - name: hashbrown
    version: 0.13.2
    sha: 43a3c133739dddd0d2990f9a4bdf8eb4b21ef50e4851ca85ab661199821d510e
  - name: hashbrown
    version: 0.14.0
    sha: 2c6201b9ff9fd90a5a3bac2e56a830d0caa509576f0e503818ee82c181b3437a
  - name: heck
    version: 0.4.0
    sha: 2540771e65fc8cb83cd6e8a237f70c319bd5c29f78ed1084ba5d50eeac86f7f9
  - name: hermit-abi
    version: 0.3.1
    sha: fed44880c466736ef9a5c5b5facefb5ed0785676d0c02d612db14e54f0d84286
  - name: hex
    version: 0.4.3
    sha: 7f24254aa9a54b5c858eaee2f5bccdb46aaf0e486a595ed5fd8f86ba55232a70
  - name: hmac
    version: 0.12.1
    sha: 6c49c37c09c17a53d937dfbb742eb3a961d65a994e6bcdcf37e7399d0cc8ab5e
  - name: home
    version: 0.5.5
    sha: 5444c27eef6923071f7ebcc33e3444508466a76f7a2b93da00ed6e19f30c1ddb
  - name: hostname
    version: 0.3.1
    sha: 3c731c3e10504cc8ed35cfe2f1db4c9274c3d35fa486e3b31df46f068ef3e867
  - name: http
    version: 0.2.9
    sha: bd6effc99afb63425aff9b05836f029929e345a6148a14b7ecd5ab67af944482
  - name: http-body
    version: 0.4.5
    sha: d5f38f16d184e36f2408a55281cd658ecbd3ca05cce6d6510a176eca393e26d1
  - name: httparse
    version: 1.8.0
    sha: d897f394bad6a705d5f4104762e116a75639e470d80901eed05a860a95cb1904
  - name: httpdate
    version: 1.0.2
    sha: c4a1e36c821dbe04574f602848a19f742f4fb3c98d40449f11bcad18d6b17421
  - name: humantime
    version: 2.1.0
    sha: 9a3a5bfb195931eeb336b2a7b4d761daec841b97f947d34394601737a7bba5e4
  - name: hyper
    version: 0.14.27
    sha: ffb1cfd654a8219eaef89881fdb3bb3b1cdc5fa75ded05d6933b2b382e395468
  - name: hyper-rustls
    version: 0.24.0
    sha: 0646026eb1b3eea4cd9ba47912ea5ce9cc07713d105b1a14698f4e6433d348b7
  - name: hyper-tls
    version: 0.5.0
    sha: d6183ddfa99b85da61a140bea0efc93fdf56ceaa041b37d553518030827f9905
  - name: iana-time-zone
    version: 0.1.47
    sha: 4c495f162af0bf17656d0014a0eded5f3cd2f365fdd204548c2869db89359dc7
  - name: idna
    version: 0.2.3
    sha: 418a0a6fab821475f634efe3ccc45c013f742efe03d853e8d3355d5cb850ecf8
  - name: idna
    version: 0.3.0
    sha: e14ddfc70884202db2244c223200c204c2bda1bc6e0998d11b5e024d657209e6
  - name: indexmap
    version: 1.9.2
    sha: 1885e79c1fc4b10f0e172c475f458b7f7b93061064d98c3293e98c5ba0c8b399
  - name: indexmap
    version: 2.0.0
    sha: d5477fe2230a79769d8dc68e0eabf5437907c0457a5614a9e8dddb67f65eb65d
  - name: instant
    version: 0.1.12
    sha: 7a5bbe824c507c5da5956355e86a746d82e0e1464f65d862cc5e71da70e94b2c
  - name: io-lifetimes
    version: 1.0.3
    sha: 46112a93252b123d31a119a8d1a1ac19deac4fac6e0e8b0df58f0d4e5870e63c
  - name: ipconfig
    version: 0.3.1
    sha: bd302af1b90f2463a98fa5ad469fc212c8e3175a41c3068601bfa2727591c5be
  - name: ipnet
    version: 2.5.1
    sha: f88c5561171189e69df9d98bcf18fd5f9558300f7ea7b801eb8a0fd748bd8745
  - name: is-terminal
    version: 0.4.7
    sha: adcf93614601c8129ddf72e2d5633df827ba6551541c6d8c59520a371475be1f
  - name: itertools
    version: 0.10.5
    sha: b0fd2260e829bddf4cb6ea802289de2f86d6a7a690192fbe91b3f46e0f2c8473
  - name: itoa
    version: 1.0.4
    sha: 4217ad341ebadf8d8e724e264f13e593e0648f5b3e94b3896a5df283be015ecc
  - name: jobserver
    version: 0.1.26
    sha: 936cfd212a0155903bcbc060e316fb6cc7cbf2e1907329391ebadc1fe0ce77c2
  - name: js-sys
    version: 0.3.60
    sha: 49409df3e3bf0856b916e2ceaca09ee28e6871cf7d9ce97a692cacfdb2a25a47
  - name: jsonwebtoken
    version: 8.2.0
    sha: 09f4f04699947111ec1733e71778d763555737579e44b85844cae8e1940a1828
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: libc
    version: 0.2.149
    sha: a08173bc88b7955d1b3145aa561539096c421ac8debde8cbc3612ec635fee29b
  - name: libm
    version: 0.2.6
    sha: 348108ab3fba42ec82ff6e9564fc4ca0247bdccdc68dd8af9764bbc79c3c8ffb
  - name: libmount
    version: 0.1.15
    sha: 23c4c2ad2d5cbd2f5a05620c3daf45930add53ec207fa99ce5eec971089dc35f
  - name: linked-hash-map
    version: 0.5.6
    sha: 0717cef1bc8b636c6e1c1bbdefc09e6322da8a9321966e8928ef80d20f7f770f
  - name: linux-raw-sys
    version: 0.1.3
    sha: 8f9f08d8963a6c613f4b1a78f4f4a4dbfadf8e6545b2d72861731e4858b8b47f
  - name: linux-raw-sys
    version: 0.3.8
    sha: ef53942eb7bf7ff43a617b3e2c1c4a5ecf5944a7c1bc12d7ee39bbb15e5c1519
  - name: lock_api
    version: 0.4.9
    sha: 435011366fe56583b16cf956f9df0095b405b82d76425bc8981c0e22e60ec4df
  - name: log
    version: 0.4.19
    sha: b06a4cde4c0f271a446782e3eff8de789548ce57dbc8eca9292c27f4a42004b4
  - name: lru-cache
    version: 0.1.2
    sha: 31e24f1ad8321ca0e8a1e0ac13f23cb668e6f5466c2c57319f6a5cf1cc8e3b1c
  - name: match_cfg
    version: 0.1.0
    sha: ffbee8634e0d45d258acb448e7eaab3fce7a0a467395d4d9f228e3c1f01fb2e4
  - name: matches
    version: 0.1.10
    sha: 2532096657941c2fea9c289d370a250971c689d4f143798ff67113ec042024a5
  - name: md-5
    version: 0.10.5
    sha: 6365506850d44bff6e2fbcb5176cf63650e48bd45ef2fe2665ae1570e0f4b9ca
  - name: memchr
    version: 2.5.0
    sha: 2dffe52ecf27772e601905b7522cb4ef790d2cc203488bbd0e2fe85fcb74566d
  - name: memmap2
    version: 0.9.0
    sha: deaba38d7abf1d4cca21cc89e932e542ba2b9258664d2a9ef0e61512039c9375
  - name: memoffset
    version: 0.7.1
    sha: 5de893c32cde5f383baa4c04c5d6dbdd735cfd4a794b0debdb2bb1b421da5ff4
  - name: mime
    version: 0.3.17
    sha: 6877bb514081ee2a7ff5ef9de3281f14a4dd4bceac4c09388074a6b5df8a139a
  - name: mime_guess
    version: 2.0.4
    sha: 4192263c238a5f0d0c6bfd21f336a313a4ce1c450542449ca191bb657b4642ef
  - name: miniz_oxide
    version: 0.6.2
    sha: b275950c28b37e794e8c55d88aeb5e139d0ce23fdbbeda68f8d7174abdf9e8fa
  - name: miniz_oxide
    version: 0.7.1
    sha: e7810e0be55b428ada41041c41f32c9f1a42817901b4ccf45fa3d4b6561e74c7
  - name: mio
    version: 0.8.5
    sha: e5d732bc30207a6423068df043e3d02e0735b155ad7ce1a6f76fe2baa5b158de
  - name: multipart
    version: 0.18.0
    sha: 00dec633863867f29cb39df64a397cdf4a6354708ddd7759f70c7fb51c5f9182
  - name: nanorand
    version: 0.7.0
    sha: 6a51313c5820b0b02bd422f4b44776fbf47961755c74ce64afc73bfad10226c3
  - name: native-tls
    version: 0.2.11
    sha: 07226173c32f2926027b63cce4bcd8076c3552846cbe7925f3aaffeac0a3b92e
  - name: nix
    version: 0.14.1
    sha: 6c722bee1037d430d0f8e687bbdbf222f27cc6e4e68d5caf630857bb2b6dbdce
  - name: nix
    version: 0.26.2
    sha: bfdda3d196821d6af13126e40375cdf7da646a96114af134d5f417a9a1dc8e1a
  - name: normalize-line-endings
    version: 0.3.0
    sha: 61807f77802ff30975e01f4f071c8ba10c022052f98b3294119f3e615d13e5be
  - name: num-bigint
    version: 0.4.3
    sha: f93ab6289c7b344a8a9f60f88d80aa20032336fe78da341afc91c8a2341fc75f
  - name: num-bigint-dig
    version: 0.8.2
    sha: 2399c9463abc5f909349d8aa9ba080e0b88b3ce2885389b60b993f39b1a56905
  - name: num-integer
    version: 0.1.45
    sha: 225d3389fb3509a24c93f5c29eb6bde2586b98d9f016636dff58d7c6f7569cd9
  - name: num-iter
    version: 0.1.43
    sha: 7d03e6c028c5dc5cac6e2dec0efda81fc887605bb3d884578bb6d6bf7514e252
  - name: num-traits
    version: 0.2.15
    sha: 578ede34cf02f8924ab9447f50c28075b4d3e5b269972345e7e0372b38c6cdcd
  - name: num_cpus
    version: 1.16.0
    sha: 4161fcb6d602d4d2081af7c3a45852d875a03dd337a6bfdd6e06407b61342a43
  - name: num_threads
    version: 0.1.6
    sha: 2819ce041d2ee131036f4fc9d6ae7ae125a3a40e97ba64d04fe799ad9dabbb44
  - name: number_prefix
    version: 0.4.0
    sha: 830b246a0e5f20af87141b25c173cd1b609bd7779a4617d6ec582abaf90870f3
  - name: object
    version: 0.30.3
    sha: ea86265d3d3dcb6a27fc51bd29a4bf387fae9d2986b823079d4986af253eb439
  - name: once_cell
    version: 1.18.0
    sha: dd8b5dd2ae5ed71462c540258bedcb51965123ad7e7ccf4b9a8cafaa4a63576d
  - name: opendal
    version: 0.40.0
    sha: ddba7299bab261d3ae2f37617fb7f45b19ed872752bb4e22cf93a69d979366c5
  - name: openssl
    version: 0.10.55
    sha: 345df152bc43501c5eb9e4654ff05f794effb78d4efe3d53abc158baddc0703d
  - name: openssl-macros
    version: 0.1.0
    sha: b501e44f11665960c7e7fcf062c7d96a14ade4aa98116c004b2e37b5be7d736c
  - name: openssl-probe
    version: 0.1.5
    sha: ff011a302c396a5197692431fc1948019154afc178baf7d8e37367442a4601cf
  - name: openssl-src
    version: 111.25.1+1.1.1t
    sha: 1ef9a9cc6ea7d9d5e7c4a913dc4b48d0e359eddf01af1dfec96ba7064b4aba10
  - name: openssl-sys
    version: 0.9.90
    sha: 374533b0e45f3a7ced10fcaeccca020e66656bc03dac384f852e4e5a7a8104a6
  - name: option-ext
    version: 0.2.0
    sha: 04744f49eae99ab78e0d5c0b603ab218f515ea8cfe5a456d7629ad883a3b6e7d
  - name: ordered-multimap
    version: 0.6.0
    sha: 4ed8acf08e98e744e5384c8bc63ceb0364e68a6854187221c18df61c4797690e
  - name: os_str_bytes
    version: 6.4.1
    sha: 9b7820b9daea5457c9f21c69448905d723fbd21136ccf521748f23fd49e723ee
  - name: parking_lot
    version: 0.12.1
    sha: 3742b2c103b9f06bc9fff0a37ff4912935851bee6d36f3c02bcc755bcfec228f
  - name: parking_lot_core
    version: 0.9.4
    sha: 4dc9e0dc2adc1c69d09143aff38d3d30c5c3f0df0dad82e6d25547af174ebec0
  - name: pem
    version: 1.1.0
    sha: 03c64931a1a212348ec4f3b4362585eca7159d0d09cbdf4a7f74f02173596fd4
  - name: pem-rfc7468
    version: 0.7.0
    sha: 88b39c9bfcfc231068454382784bb460aae594343fb030d46e9f50a645418412
  - name: percent-encoding
    version: 2.2.0
    sha: 478c572c3d73181ff3c2539045f6eb99e5491218eae919370993b890cdbdd98e
  - name: pin-project
    version: 1.0.12
    sha: ad29a609b6bcd67fee905812e544992d216af9d755757c05ed2d0e15a74c6ecc
  - name: pin-project-internal
    version: 1.0.12
    sha: 069bdb1e05adc7a8990dce9cc75370895fbe4e3d58b9b73bf1aee56359344a55
  - name: pin-project-lite
    version: 0.2.13
    sha: 8afb450f006bf6385ca15ef45d71d2288452bc3683ce2e2cacc0d18e4be60b58
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkcs1
    version: 0.7.5
    sha: c8ffb9f10fa047879315e6625af03c164b16962a5368d724ed16323b68ace47f
  - name: pkcs8
    version: 0.10.2
    sha: f950b2377845cebe5cf8b5165cb3cc1a5e0fa5cfa3e1f7f55707d8fd82e0a7b7
  - name: pkg-config
    version: 0.3.26
    sha: 6ac9a59f73473f1b8d852421e59e64809f025994837ef743615c6d0c5b305160
  - name: ppv-lite86
    version: 0.2.17
    sha: 5b40af805b3121feab8a3c29f04d8ad262fa8e0561883e7653e024ae4479e6de
  - name: predicates
    version: 3.0.3
    sha: 09963355b9f467184c04017ced4a2ba2d75cbcb4e7462690d388233253d4b1a9
  - name: predicates-core
    version: 1.0.6
    sha: b794032607612e7abeb4db69adb4e33590fa6cf1149e95fd7cb00e634b92f174
  - name: predicates-tree
    version: 1.0.7
    sha: 54ff541861505aabf6ea722d2131ee980b8276e10a1297b94e896dd8b621850d
  - name: proc-macro-error
    version: 1.0.4
    sha: da25490ff9892aab3fcf7c36f08cfb902dd3e71ca0f9f9517bea02a73a5ce38c
  - name: proc-macro-error-attr
    version: 1.0.4
    sha: a1be40180e52ecc98ad80b184934baf3d0d29f979574e439af5a55274b35f869
  - name: proc-macro-hack
    version: 0.5.20+deprecated
    sha: dc375e1527247fe1a97d8b7156678dfe7c1af2fc075c9a4db3690ecd2a148068
  - name: proc-macro2
    version: 1.0.69
    sha: 134c189feb4956b20f6f547d2cf727d4c0fe06722b20a0eec87ed445a97f92da
  - name: quick-error
    version: 1.2.3
    sha: a1d01941d82fa2ab50be1e79e6714289dd7cde78eba4c074bc5a4374f650dfe0
  - name: quick-xml
    version: 0.29.0
    sha: 81b9228215d82c7b61490fec1de287136b5de6f5700f6e58ea9ad61a7964ca51
  - name: quote
    version: 1.0.33
    sha: 5267fca4496028628a95160fc423a33e8b2e6af8a5302579e322e4b520293cae
  - name: rand
    version: 0.8.5
    sha: 34af8d1a0e25924bc5b7c43c079c942339d8f0a8b57c39049bef581b46327404
  - name: rand_chacha
    version: 0.3.1
    sha: e6c10a63a0fa32252be49d21e7709d4d4baf8d231c2dbce1eaa8141b9b127d88
  - name: rand_core
    version: 0.6.4
    sha: ec0be4795e2f6a28069bec0b5ff3e2ac9bafc99e6a9a7dc3547996c5c816922c
  - name: redis
    version: 0.23.3
    sha: 4f49cdc0bb3f412bf8e7d1bd90fe1d9eb10bc5c399ba90973c14662a27b3f8ba
  - name: redox_syscall
    version: 0.2.16
    sha: fb5a58c1855b4b6819d59012155603f0b22ad30cad752600aadfcb695265519a
  - name: redox_syscall
    version: 0.3.5
    sha: 567664f262709473930a4bf9e51bf2ebf3348f2e748ccc50dea20646858f8f29
  - name: redox_users
    version: 0.4.3
    sha: b033d837a7cf162d7993aded9304e30a83213c648b6e389db233191f891e5c2b
  - name: regex
    version: 1.8.4
    sha: d0ab3ca65655bb1e41f2a8c8cd662eb4fb035e67c3f78da1d61dffe89d07300f
  - name: regex-automata
    version: 0.1.10
    sha: 6c230d73fb8d8c1b9c0b3135c5142a8acee3a0558fb8db5cf1cb65f8d7862132
  - name: regex-syntax
    version: 0.7.2
    sha: 436b050e76ed2903236f032a59761c1eb99e1b0aead2c257922771dab1fc8c78
  - name: reqsign
    version: 0.14.1
    sha: 3228e570df74d69d3d3236a71371f1edd748a3e4eb728ea1f29d403bc10fc727
  - name: reqwest
    version: 0.11.18
    sha: cde824a14b7c14f85caff81225f411faacc04a2013f41670f41443742b1c1c55
  - name: resolv-conf
    version: 0.7.0
    sha: 52e44394d2086d010551b14b53b1f24e31647570cd1deb0379e2c21b329aba00
  - name: retry
    version: 2.0.0
    sha: 9166d72162de3575f950507683fac47e30f6f2c3836b71b7fbc61aa517c9c5f4
  - name: ring
    version: 0.16.20
    sha: 3053cf52e236a3ed746dfc745aa9cacf1b791d846bdaf412f60a8d7d6e17c8fc
  - name: rouille
    version: 3.6.2
    sha: 3716fbf57fc1084d7a706adf4e445298d123e4a44294c4e8213caf1b85fcc921
  - name: rsa
    version: 0.9.2
    sha: 6ab43bb47d23c1a631b4b680199a45255dce26fa9ab2fa902581f624ff13e6a8
  - name: rust-ini
    version: 0.19.0
    sha: 7e2a3bcec1f113553ef1c88aae6c020a369d03d55b58de9869a0908930385091
  - name: rustc-demangle
    version: 0.1.23
    sha: d626bb9dae77e28219937af045c257c28bfd3f69333c512553507f5f9798cb76
  - name: rustix
    version: 0.36.16
    sha: 6da3636faa25820d8648e0e31c5d519bbb01f72fdf57131f0f5f7da5fed36eab
  - name: rustix
    version: 0.37.7
    sha: 2aae838e49b3d63e9274e1c01833cc8139d3fec468c3b84688c628f44b1ae11d
  - name: rustls
    version: 0.20.7
    sha: 539a2bfe908f471bfa933876bd1eb6a19cf2176d375f82ef7f99530a40e48c2c
  - name: rustls
    version: 0.21.7
    sha: cd8d6c9f025a446bc4d18ad9632e69aec8f287aa84499ee335599fabd20c3fd8
  - name: rustls-native-certs
    version: 0.6.2
    sha: 0167bac7a9f490495f3c33013e7722b53cb087ecbe082fb0c6387c96f634ea50
  - name: rustls-pemfile
    version: 1.0.1
    sha: 0864aeff53f8c05aa08d86e5ef839d3dfcf07aeba2db32f12db0ef716e87bd55
  - name: rustls-webpki
    version: 0.101.4
    sha: 7d93931baf2d282fff8d3a532bbfd7653f734643161b87e3e01e59a04439bf0d
  - name: ryu
    version: 1.0.11
    sha: 4501abdff3ae82a1c1b477a17252eb69cee9e66eb915c1abaa4f44d873df9f09
  - name: safemem
    version: 0.3.3
    sha: ef703b7cb59335eae2eb93ceb664c0eb7ea6bf567079d843e09420219668e072
  - name: same-file
    version: 1.0.6
    sha: 93fc1dc3aaa9bfed95e02e6eadabb4baf7e3078b0bd1b4d7b6b0b68378900502
  - name: schannel
    version: 0.1.20
    sha: 88d6731146462ea25d9244b2ed5fd1d716d25c52e4d54aa4fb0f3c4e9854dbe2
  - name: scopeguard
    version: 1.1.0
    sha: d29ab0c6d3fc0ee92fe66e2d99f700eab17a8d57d1c1d3b748380fb20baa78cd
  - name: sct
    version: 0.7.0
    sha: d53dcdb7c9f8158937a7981b48accfd39a43af418591a5d008c7b22b5e1b7ca4
  - name: security-framework
    version: 2.7.0
    sha: 2bc1bb97804af6631813c55739f771071e0f2ed33ee20b68c86ec505d906356c
  - name: security-framework-sys
    version: 2.6.1
    sha: 0160a13a177a45bfb43ce71c01580998474f556ad854dcbca936dd2841a5c556
  - name: semver
    version: 1.0.17
    sha: bebd363326d05ec3e2f532ab7660680f3b02130d780c299bca73469d521bc0ed
  - name: serde
    version: 1.0.190
    sha: 91d3c334ca1ee894a2c6f6ad698fe8c435b76d504b13d436f0685d648d6d96f7
  - name: serde_derive
    version: 1.0.190
    sha: 67c5609f394e5c2bd7fc51efda478004ea80ef42fee983d5c67a65e34f32c0e3
  - name: serde_json
    version: 1.0.94
    sha: 1c533a59c9d8a93a09c6ab31f0fd5e5f4dd1b8fc9434804029839884765d04ea
  - name: serde_repr
    version: 0.1.9
    sha: 1fe39d9fbb0ebf5eb2c7cb7e2a47e4f462fad1379f1166b8ae49ad9eae89a7ca
  - name: serde_spanned
    version: 0.6.4
    sha: 12022b835073e5b11e90a14f86838ceb1c8fb0325b72416845c487ac0fa95e80
  - name: serde_urlencoded
    version: 0.7.1
    sha: d3491c14715ca2294c4d6a88f15e84739788c1d030eed8c110436aafdaa2f3fd
  - name: serial_test
    version: 2.0.0
    sha: 0e56dd856803e253c8f298af3f4d7eb0ae5e23a737252cd90bb4f3b435033b2d
  - name: serial_test_derive
    version: 2.0.0
    sha: 91d129178576168c589c9ec973feedf7d3126c01ac2bf08795109aa35b69fb8f
  - name: sha1
    version: 0.10.5
    sha: f04293dc80c3993519f2d7f6f511707ee7094fe0c6d3406feb330cdb3540eba3
  - name: sha1_smol
    version: 1.0.0
    sha: ae1a47186c03a32177042e55dbc5fd5aee900b8e0069a8d70fba96a9375cd012
  - name: sha2
    version: 0.10.6
    sha: 82e6b795fe2e3b1e845bafcb27aa35405c4d47cdfc92af5fc8d3002f76cebdc0
  - name: signal-hook-registry
    version: 1.4.0
    sha: e51e73328dc4ac0c7ccbda3a494dfa03df1de2f46018127f60c693f2648455b0
  - name: signature
    version: 2.0.0
    sha: 8fe458c98333f9c8152221191a77e2a44e8325d0193484af2e9421a53019e57d
  - name: simple_asn1
    version: 0.6.2
    sha: adc4e5204eb1910f40f9cfa375f6f05b68c3abac4b6fd879c8ff5e7ae8a0a085
  - name: slab
    version: 0.4.7
    sha: 4614a76b2a8be0058caa9dbbaf66d988527d86d003c11a94fbd335d7661edcef
  - name: smallvec
    version: 1.10.0
    sha: a507befe795404456341dfab10cef66ead4c041f62b8b11bbb92bffe5d0953e0
  - name: socket2
    version: 0.4.9
    sha: 64a4a911eed85daf18834cfaa86a79b7d266ff93ff5ba14005426219480ed662
  - name: spin
    version: 0.5.2
    sha: 6e63cff320ae2c57904679ba7cb63280a3dc4613885beafb148ee7bf9aa9042d
  - name: spin
    version: 0.9.4
    sha: 7f6002a767bff9e83f8eeecf883ecb8011875a21ae8da43bffb817a57e78cc09
  - name: spki
    version: 0.7.2
    sha: 9d1e996ef02c474957d681f1b05213dfb0abab947b446a62d37770b23500184a
  - name: static_assertions
    version: 1.1.0
    sha: a2eb9349b6444b326872e140eb1cf5e7c522154d69e7a0ffb0fb81c06b37543f
  - name: stringmatch
    version: 0.3.3
    sha: a8c0faab770316c3838f895fc2dfc3a8707ef4da48676f1014e1061ebd583b40
  - name: strip-ansi-escapes
    version: 0.1.1
    sha: 011cbb39cf7c1f62871aea3cc46e5817b0937b49e9447370c93cacbe93a766d8
  - name: strsim
    version: 0.10.0
    sha: 73473c0e59e6d5812c5dfe2a064a6444949f089e20eec9a2e5506596494e4623
  - name: subtle
    version: 2.4.1
    sha: 6bdef32e8150c2a081110b42772ffe7d7c9032b606bc226c8260fd97e0976601
  - name: syn
    version: 1.0.104
    sha: 4ae548ec36cf198c0ef7710d3c230987c2d6d7bd98ad6edc0274462724c585ce
  - name: syn
    version: 2.0.38
    sha: e96b79aaa137db8f61e26363a0c9b47d8b4ec75da28b7d1d614c2303e232408b
  - name: syslog
    version: 6.1.0
    sha: 7434e95bcccce1215d30f4bf84fe8c00e8de1b9be4fb736d747ca53d36e7f96f
  - name: tar
    version: 0.4.38
    sha: 4b55807c0344e1e6c04d7c965f5289c39a8d94ae23ed5c0b57aabac549f871c6
  - name: temp-env
    version: 0.3.4
    sha: 9547444bfe52cbd79515c6c8087d8ae6ca8d64d2d31a27746320f5cb81d1a15c
  - name: tempfile
    version: 3.4.0
    sha: af18f7ae1acd354b992402e9ec5864359d693cd8a79dcbef59f76891701c1e95
  - name: termcolor
    version: 1.1.3
    sha: bab24d30b911b2376f3a13cc2cd443142f0c81dda04c118693e35b3835757755
  - name: terminal_size
    version: 0.2.6
    sha: 8e6bf6f19e9f8ed8d4048dc22981458ebcf406d67e94cd422e5ecd73d63b3237
  - name: termtree
    version: 0.4.0
    sha: 95059e91184749cb66be6dc994f67f182b6d897cb3df74a5bf66b5e709295fd8
  - name: thirtyfour
    version: 0.27.3
    sha: 6ac2540aff94b9f8d89a94bb5d8cc5e71560f78ee8f6c953cd31469083c61f6d
  - name: thirtyfour_sync
    version: 0.27.1
    sha: ab1e47e6c2fed609d851c6f6171a559ecffb1d121f2d6e02dd390e90ea2c3d38
  - name: thiserror
    version: 1.0.38
    sha: 6a9cd18aa97d5c45c6603caea1da6628790b37f7a34b6ca89522331c5180fed0
  - name: thiserror-impl
    version: 1.0.38
    sha: 1fb327af4685e4d03fa8cbcf1716380da910eeb2bb8be417e7f9fd3fb164f36f
  - name: threadpool
    version: 1.8.1
    sha: d050e60b33d41c19108b32cea32164033a9013fe3b46cbd4457559bfbf77afaa
  - name: time
    version: 0.3.17
    sha: a561bf4617eebd33bca6434b988f39ed798e527f51a1e797d0ee4f61c0a38376
  - name: time-core
    version: 0.1.0
    sha: 2e153e1f1acaef8acc537e68b44906d2db6436e2b35ac2c6b42640fff91f00fd
  - name: time-macros
    version: 0.2.6
    sha: d967f99f534ca7e495c575c62638eebc2898a8c84c119b89e250477bc4ba16b2
  - name: tiny-keccak
    version: 2.0.2
    sha: 2c9d3793400a45f954c52e73d068316d76b6f4e36977e3fcebb13a2721e80237
  - name: tiny_http
    version: 0.12.0
    sha: 389915df6413a2e74fb181895f933386023c71110878cd0825588928e64cdc82
  - name: tinyvec
    version: 1.6.0
    sha: 87cc5ceb3875bb20c2890005a4e226a4651264a5c75edb2421b52861a0a0cb50
  - name: tinyvec_macros
    version: 0.1.0
    sha: cda74da7e1a664f795bb1f8a87ec406fb89a02522cf6e50620d016add6dbbf5c
  - name: tokio
    version: 1.28.2
    sha: 94d7b1cfd2aa4011f2de74c2c4c63665e27a71006b0a192dcd2710272e73dfa2
  - name: tokio-macros
    version: 2.1.0
    sha: 630bdcf245f78637c13ec01ffae6187cca34625e8c63150d424b59e55af2675e
  - name: tokio-native-tls
    version: 0.3.0
    sha: f7d995660bd2b7f8c1568414c1126076c13fbb725c40112dc0120b78eb9b717b
  - name: tokio-retry
    version: 0.3.0
    sha: 7f57eb36ecbe0fc510036adff84824dd3c24bb781e21bfa67b69d556aa85214f
  - name: tokio-rustls
    version: 0.23.4
    sha: c43ee83903113e03984cb9e5cebe6c04a5116269e900e3ddba8f068a62adda59
  - name: tokio-rustls
    version: 0.24.0
    sha: e0d409377ff5b1e3ca6437aa86c1eb7d40c134bfec254e44c830defa92669db5
  - name: tokio-serde
    version: 0.8.0
    sha: 911a61637386b789af998ee23f50aa30d5fd7edcec8d6d3dedae5e5815205466
  - name: tokio-util
    version: 0.7.9
    sha: 1d68074620f57a0b21594d9735eb2e98ab38b17f80d3fcb189fca266771ca60d
  - name: toml
    version: 0.8.6
    sha: 8ff9e3abce27ee2c9a37f9ad37238c1bdd4e789c84ba37df76aa4d528f5072cc
  - name: toml_datetime
    version: 0.6.5
    sha: 3550f4e9685620ac18a50ed434eb3aec30db8ba93b0287467bca5826ea25baf1
  - name: toml_edit
    version: 0.20.7
    sha: 70f427fce4d84c72b5b732388bf4a9f4531b53f74e2887e3ecb2481f68f66d81
  - name: tower
    version: 0.4.13
    sha: b8fa9be0de6cf49e536ce1851f987bd21a43b771b09473c3549a6c853db37c1c
  - name: tower-layer
    version: 0.3.2
    sha: c20c8dbed6283a09604c3e69b4b7eeb54e298b8a600d4d5ecb5ad39de609f1d0
  - name: tower-service
    version: 0.3.2
    sha: b6bc1c9ce2b5135ac7f93c72918fc37feb872bdc6a5533a8b85eb4b86bfdae52
  - name: tracing
    version: 0.1.37
    sha: 8ce8c33a8d48bd45d624a6e523445fd21ec13d3653cd51f681abf67418f54eb8
  - name: tracing-attributes
    version: 0.1.23
    sha: 4017f8f45139870ca7e672686113917c71c7a6e02d4924eda67186083c03081a
  - name: tracing-core
    version: 0.1.30
    sha: 24eb03ba0eab1fd845050058ce5e616558e8f8d8fca633e6b163fe25c797213a
  - name: trust-dns-proto
    version: 0.22.0
    sha: 4f7f83d1e4a0e4358ac54c5c3681e5d7da5efc5a7a632c90bb6d6669ddd9bc26
  - name: trust-dns-resolver
    version: 0.22.0
    sha: aff21aa4dcefb0a1afbfac26deb0adc93888c7d295fb63ab273ef276ba2b7cfe
  - name: try-lock
    version: 0.2.3
    sha: 59547bce71d9c38b83d9c0e92b6066c4253371f15005def0c30d9657f50c7642
  - name: twoway
    version: 0.1.8
    sha: 59b11b2b5241ba34be09c3cc85a36e56e48f9888862e19cedf23336d35316ed1
  - name: typenum
    version: 1.15.0
    sha: dcf81ac59edc17cc8697ff311e8f5ef2d99fcbd9817b34cec66f90b6c3dfd987
  - name: unicase
    version: 2.6.0
    sha: 50f37be617794602aabbeee0be4f259dc1778fabe05e2d67ee8f79326d5cb4f6
  - name: unicode-bidi
    version: 0.3.8
    sha: 099b7128301d285f79ddd55b9a83d5e6b9e97c92e0ea0daebee7263e932de992
  - name: unicode-ident
    version: 1.0.5
    sha: 6ceab39d59e4c9499d4e5a8ee0e2735b891bb7308ac83dfb4e80cad195c9f6f3
  - name: unicode-normalization
    version: 0.1.22
    sha: 5c5713f0fc4b5db668a2ac63cdb7bb4469d8c9fed047b1d0292cc7b0ce2ba921
  - name: untrusted
    version: 0.7.1
    sha: a156c684c91ea7d62626509bce3cb4e1d9ed5c4d978f7b4352658f96a4c26b4a
  - name: url
    version: 2.3.1
    sha: 0d68c799ae75762b8c3fe375feb6600ef5602c883c5d21eb51c09f22b83c4643
  - name: urlparse
    version: 0.7.3
    sha: 110352d4e9076c67839003c7788d8604e24dcded13e0b375af3efaa8cf468517
  - name: utf8parse
    version: 0.2.0
    sha: 936e4b492acfd135421d8dca4b1aa80a7bfc26e702ef3af710e0752684df5372
  - name: uuid
    version: 1.3.3
    sha: 345444e32442451b267fc254ae85a209c64be56d2890e601a0c37ff0c3c5ecd2
  - name: vcpkg
    version: 0.2.15
    sha: accd4ea62f7bb7a82fe23066fb0957d48ef677f6eeb8215f372f52e48bb32426
  - name: version-compare
    version: 0.1.1
    sha: 579a42fc0b8e0c63b76519a339be31bed574929511fa53c1a3acae26eb258f29
  - name: version_check
    version: 0.9.4
    sha: 49874b5167b65d7193b8aba1567f5c7d93d001cafc34600cee003eda787e483f
  - name: void
    version: 1.0.2
    sha: 6a02e4885ed3bc0f2de90ea6dd45ebcbb66dacffe03547fadbb0eeae2770887d
  - name: vte
    version: 0.10.1
    sha: 6cbce692ab4ca2f1f3047fcf732430249c0e971bfdd2b234cf2c47ad93af5983
  - name: vte_generate_state_changes
    version: 0.1.1
    sha: d257817081c7dffcdbab24b9e62d2def62e2ff7d00b1c20062551e6cccc145ff
  - name: wait-timeout
    version: 0.2.0
    sha: 9f200f5b12eb75f8c1ed65abd4b2db8a6e1b138a20de009dacee265a2498f3f6
  - name: walkdir
    version: 2.4.0
    sha: d71d857dc86794ca4c280d616f7da00d2dbfd8cd788846559a6813e6aa4b54ee
  - name: want
    version: 0.3.0
    sha: 1ce8a968cb1cd110d136ff8b819a556d6fb6d919363c61534f6860c7eb172ba0
  - name: wasi
    version: 0.11.0+wasi-snapshot-preview1
    sha: 9c8d87e72b64a3b4db28d11ce29237c246188f4f51057d65a7eab63b7987e423
  - name: wasm-bindgen
    version: 0.2.83
    sha: eaf9f5aceeec8be17c128b2e93e031fb8a4d469bb9c4ae2d7dc1888b26887268
  - name: wasm-bindgen-backend
    version: 0.2.83
    sha: 4c8ffb332579b0557b52d268b91feab8df3615f265d5270fec2a8c95b17c1142
  - name: wasm-bindgen-futures
    version: 0.4.29
    sha: 2eb6ec270a31b1d3c7e266b999739109abce8b6c87e4b31fcfcd788b65267395
  - name: wasm-bindgen-macro
    version: 0.2.83
    sha: 052be0f94026e6cbc75cdefc9bae13fd6052cdcaf532fa6c45e7ae33a1e6c810
  - name: wasm-bindgen-macro-support
    version: 0.2.83
    sha: 07bc0c051dc5f23e307b13285f9d75df86bfdf816c5721e573dec1f9b8aa193c
  - name: wasm-bindgen-shared
    version: 0.2.83
    sha: 1c38c045535d93ec4f0b4defec448e4291638ee608530863b1e2ba115d4fff7f
  - name: wasm-streams
    version: 0.2.3
    sha: 6bbae3363c08332cadccd13b67db371814cd214c2524020932f0804b8cf7c078
  - name: web-sys
    version: 0.3.59
    sha: ed055ab27f941423197eb86b2035720b1a3ce40504df082cac2ecc6ed73335a1
  - name: webpki
    version: 0.22.2
    sha: 07ecc0cd7cac091bf682ec5efa18b1cff79d617b84181f38b3951dbe135f607f
  - name: webpki-roots
    version: 0.22.5
    sha: 368bfe657969fb01238bb756d351dcade285e0f6fcbd36dcb23359a5169975be
  - name: which
    version: 4.4.0
    sha: 2441c784c52b289a054b7201fc93253e288f094e2f4be9058343127c4226a269
  - name: widestring
    version: 0.5.1
    sha: 17882f045410753661207383517a6f62ec3dbeb6a4ed2acce01f0728238d1983
  - name: winapi
    version: 0.3.9
    sha: 5c839a674fcd7a98952e593242ea400abe93992746761e38641405d28b00f419
  - name: winapi-i686-pc-windows-gnu
    version: 0.4.0
    sha: ac3b87c63620426dd9b991e5ce0329eff545bccbbb34f3be09ff6fb6ab51b7b6
  - name: winapi-util
    version: 0.1.5
    sha: 70ec6ce85bb158151cae5e5c87f95a8e97d2c0c4b001223f33a334e3ce5de178
  - name: winapi-x86_64-pc-windows-gnu
    version: 0.4.0
    sha: 712e227841d057c1ee1cd2fb22fa7e5a5461ae8e48fa2ca79ec42cfc1931183f
  - name: windows-sys
    version: 0.36.1
    sha: ea04155a16a59f9eab786fe12a4a450e75cdb175f9e0d80da1e17db09f55b8d2
  - name: windows-sys
    version: 0.42.0
    sha: 5a3e1820f08b8513f676f7ab6c1f99ff312fb97b553d30ff4dd86f9f15728aa7
  - name: windows-sys
    version: 0.45.0
    sha: 75283be5efb2831d37ea142365f009c02ec203cd29a3ebecbc093d52315b66d0
  - name: windows-sys
    version: 0.48.0
    sha: 677d2418bec65e3338edb076e806bc1ec15693c5d0104683f2efe857f61056a9
  - name: windows-targets
    version: 0.42.1
    sha: 8e2522491fbfcd58cc84d47aeb2958948c4b8982e9a2d8a2a35bbaed431390e7
  - name: windows-targets
    version: 0.48.0
    sha: 7b1eb6f0cd7c80c79759c929114ef071b87354ce476d9d94271031c0497adfd5
  - name: windows_aarch64_gnullvm
    version: 0.42.1
    sha: 8c9864e83243fdec7fc9c5444389dcbbfd258f745e7853198f365e3c4968a608
  - name: windows_aarch64_gnullvm
    version: 0.48.0
    sha: 91ae572e1b79dba883e0d315474df7305d12f569b400fcf90581b06062f7e1bc
  - name: windows_aarch64_msvc
    version: 0.36.1
    sha: 9bb8c3fd39ade2d67e9874ac4f3db21f0d710bee00fe7cab16949ec184eeaa47
  - name: windows_aarch64_msvc
    version: 0.42.1
    sha: 4c8b1b673ffc16c47a9ff48570a9d85e25d265735c503681332589af6253c6c7
  - name: windows_aarch64_msvc
    version: 0.48.0
    sha: b2ef27e0d7bdfcfc7b868b317c1d32c641a6fe4629c171b8928c7b08d98d7cf3
  - name: windows_i686_gnu
    version: 0.36.1
    sha: 180e6ccf01daf4c426b846dfc66db1fc518f074baa793aa7d9b9aaeffad6a3b6
  - name: windows_i686_gnu
    version: 0.42.1
    sha: de3887528ad530ba7bdbb1faa8275ec7a1155a45ffa57c37993960277145d640
  - name: windows_i686_gnu
    version: 0.48.0
    sha: 622a1962a7db830d6fd0a69683c80a18fda201879f0f447f065a3b7467daa241
  - name: windows_i686_msvc
    version: 0.36.1
    sha: e2e7917148b2812d1eeafaeb22a97e4813dfa60a3f8f78ebe204bcc88f12f024
  - name: windows_i686_msvc
    version: 0.42.1
    sha: bf4d1122317eddd6ff351aa852118a2418ad4214e6613a50e0191f7004372605
  - name: windows_i686_msvc
    version: 0.48.0
    sha: 4542c6e364ce21bf45d69fdd2a8e455fa38d316158cfd43b3ac1c5b1b19f8e00
  - name: windows_x86_64_gnu
    version: 0.36.1
    sha: 4dcd171b8776c41b97521e5da127a2d86ad280114807d0b2ab1e462bc764d9e1
  - name: windows_x86_64_gnu
    version: 0.42.1
    sha: c1040f221285e17ebccbc2591ffdc2d44ee1f9186324dd3e84e99ac68d699c45
  - name: windows_x86_64_gnu
    version: 0.48.0
    sha: ca2b8a661f7628cbd23440e50b05d705db3686f894fc9580820623656af974b1
  - name: windows_x86_64_gnullvm
    version: 0.42.1
    sha: 628bfdf232daa22b0d64fdb62b09fcc36bb01f05a3939e20ab73aaf9470d0463
  - name: windows_x86_64_gnullvm
    version: 0.48.0
    sha: 7896dbc1f41e08872e9d5e8f8baa8fdd2677f29468c4e156210174edc7f7b953
  - name: windows_x86_64_msvc
    version: 0.36.1
    sha: c811ca4a8c853ef420abd8592ba53ddbbac90410fab6903b3e79972a631f7680
  - name: windows_x86_64_msvc
    version: 0.42.1
    sha: 447660ad36a13288b1db4d4248e857b510e8c3a225c822ba4fb748c0aafecffd
  - name: windows_x86_64_msvc
    version: 0.48.0
    sha: 1a515f5799fe4961cb532f983ce2b23082366b898e52ffbce459c86f67c8378a
  - name: winnow
    version: 0.5.4
    sha: acaaa1190073b2b101e15083c38ee8ec891b5e05cbee516521e94ec008f61e64
  - name: winreg
    version: 0.10.1
    sha: 80d0f4e272c85def139476380b12f9ac60926689dd2e01d4923222f40580869d
  - name: xattr
    version: 0.2.3
    sha: 6d1526bbe5aaeb5eb06885f4d987bcdfa5e23187055de9b83fe00156a821fabc
  - name: zeroize
    version: 1.5.7
    sha: c394b5bd0c6f669e7275d9c20aa90ae064cb22e75a1cad54e1b34088034b149f
  - name: zip
    version: 0.6.4
    sha: 0445d0fbc924bb93539b4316c11afb121ea39296f99a3c4c9edad09e3658cdef
  - name: zstd
    version: 0.12.4
    sha: 1a27595e173641171fc74a1232b7b1c7a7cb6e18222c11e9dfb9888fa424c53c
  - name: zstd-safe
    version: 6.0.4+zstd.1.5.4
    sha: 7afb4b54b8910cf5447638cb54bf4e8a65cbedd783af98b98c62ffe91f185543
  - name: zstd-sys
    version: 2.0.7+zstd.1.5.4
    sha: 94509c3ba2fe55294d752b79842c530ccfab760192521df74a081a78d2b3c7f5
