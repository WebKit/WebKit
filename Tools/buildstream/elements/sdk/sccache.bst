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
- kind: git_tag
  url: github_com:mozilla/sccache
  track: v0.3.0
  ref: v0.3.0-0-gf6e36e68c6cb1e17127273956df9ebf2127dff1a
- kind: cargo
  ref:
  - name: adler
    version: 1.0.2
    sha: f26201604c87b1e01bd3d98f8d5d9a8fcbb815e8cedb41ffccbeb4bf593a35fe
  - name: aho-corasick
    version: 0.7.18
    sha: 1e37cfd5e7657ada45f742d6e99ca5788580b5c529dc78faf11ece6dc702656f
  - name: ansi_term
    version: 0.12.1
    sha: d52a9bb7ec0cf484c551830a7ce27bd20d67eac647e1befb56b0be4ee39a55d2
  - name: anyhow
    version: 1.0.53
    sha: 94a45b455c14666b85fc40a019e8ab9eb75e3a124e05494f5397122bc9eb06e0
  - name: ar
    version: 0.9.0
    sha: d67af77d68a931ecd5cbd8a3b5987d63a1d1d1278f7f6a60ae33db485cdebb69
  - name: arrayref
    version: 0.3.6
    sha: a4c527152e37cf757a3f78aae5a06fbeefdb07ccc535c980a3208ee3060dd544
  - name: arrayvec
    version: 0.5.2
    sha: 23b62fc65de8e4e7f52534fb52b0f3ed04746ae267519eef2a83941e8085068b
  - name: ascii
    version: 1.0.0
    sha: bbf56136a5198c7b01a49e3afcbef6cf84597273d298f54432926024107b0109
  - name: assert_cmd
    version: 2.0.4
    sha: 93ae1ddd39efd67689deb1979d80bad3bf7f2b09c6e6117c8d1f2443b5e2f83e
  - name: async-channel
    version: 1.6.1
    sha: 2114d64672151c0c5eaa5e131ec84a74f06e1e559830dabba01ca30605d66319
  - name: async-trait
    version: 0.1.52
    sha: 061a7acccaa286c011ddc30970520b98fa40e00c9d644633fb26b5fc63a265e3
  - name: atty
    version: 0.2.14
    sha: d9b39be18770d11421cdb1b9947a45dd3f37e93092cbf377614828a319d5fee8
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: base64
    version: 0.12.3
    sha: 3441f0f7b02788e948e47f457ca01f1d7e6d92c693bc132c22b087d3141c03ff
  - name: base64
    version: 0.13.0
    sha: 904dfeac50f3cdaba28fc6f57fdcddb75f49ed61346676a78c4ffe55877802fd
  - name: bincode
    version: 1.3.3
    sha: b1f45e9417d87227c7a56d22e471c6206462cba514c7590c09aff4cf6d1ddcad
  - name: bitflags
    version: 1.3.2
    sha: bef38d45163c2f1dde094a7dfd33ccf595c92905c8f8f4fdc18d06fb1037718a
  - name: blake3
    version: 0.3.8
    sha: b64485778c4f16a6a5a9d335e80d449ac6c70cdd6a06d2af18a6f6f775a125b3
  - name: block-buffer
    version: 0.10.2
    sha: 0bf7fe51849ea569fd452f37822f606a5cabb684dc918707a0193fd4664ff324
  - name: boxfnonce
    version: 0.1.1
    sha: 5988cb1d626264ac94100be357308f29ff7cbdd3b36bda27f450a4ee3f713426
  - name: bstr
    version: 0.2.17
    sha: ba3569f383e8f1598449f1a423e72e99569137b47740b1da11ef19af3d5c3223
  - name: buf_redux
    version: 0.8.4
    sha: b953a6887648bb07a535631f2bc00fbdb2a2216f135552cb3f534ed136b9c07f
  - name: bufstream
    version: 0.1.4
    sha: 40e38929add23cdf8a366df9b0e088953150724bcbe5fc330b0d8eb3b328eec8
  - name: bumpalo
    version: 3.9.1
    sha: a4a45a46ab1f2412e53d3a0ade76ffad2025804294569aae387231a0cd6e0899
  - name: byteorder
    version: 1.4.3
    sha: 14c189c53d098945499cdfa7ecc63567cf3886b3332b312a5b4585d8d3a6a610
  - name: bytes
    version: 1.1.0
    sha: c4872d67bab6358e59559027aa3b9157c53d9358c51423c17554809a8858e0f8
  - name: cache-padded
    version: 1.2.0
    sha: c1db59621ec70f09c5e9b597b220c7a2b43611f4710dc03ceb8748637775692c
  - name: cc
    version: 1.0.72
    sha: 22a9137b95ea06864e018375b72adfb7db6e6f68cfc8df5a04d00288050485ee
  - name: cfg-if
    version: 0.1.10
    sha: 4785bdd1c96b2a846b2bd7cc02e86b6b3dbf14e7e53446c4f54c92a361040822
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.19
    sha: 670ad68c9088c2a963aaa298cb369688cf3f9465ce5e2d4ca10e6e0098a1ce73
  - name: chunked_transfer
    version: 1.4.0
    sha: fff857943da45f546682664a79488be82e69e43c1a7a2307679ab9afb3a66d2e
  - name: clap
    version: 2.34.0
    sha: a0610544180c38b88101fecf2dd634b174a62eef6946f84dfc6a7127512b381c
  - name: combine
    version: 4.6.3
    sha: 50b727aacc797f9fc28e355d21f34709ac4fc9adecfe470ad07b8f4464f53062
  - name: concurrent-queue
    version: 1.2.2
    sha: 30ed07550be01594c6026cff2a1d7fe9c8f683caa798e12b68694ac9e88286a3
  - name: config
    version: 0.10.1
    sha: 19b076e143e1d9538dde65da30f8481c2a6c44040edb8e02b9bf1351edb92ce3
  - name: conhash
    version: 0.4.0
    sha: 99d6364d028778d0d98b6014fa5882da377cd10d3492b7734d266a428e9b1fca
  - name: constant_time_eq
    version: 0.1.5
    sha: 245097e9a4535ee1e3e3931fcfcd55a796a44c643e8596ff6566d68f09b87bbc
  - name: core-foundation
    version: 0.9.3
    sha: 194a7a9e6de53fa55116934067c844d9d749312f75c6f6d0980e8c252f8c2146
  - name: core-foundation-sys
    version: 0.8.3
    sha: 5827cebf4670468b8772dd191856768aedcb1b0278a04f989f7766351917b9dc
  - name: counted-array
    version: 0.1.2
    sha: 384f8c53175c890920b6e0127b730709d2a173ca6c4dfdc81618ac9b46f648fe
  - name: cpufeatures
    version: 0.2.1
    sha: 95059428f66df56b63431fdb4e1947ed2190586af5c5a8a8b71122bdf5a7f469
  - name: crc32fast
    version: 1.3.2
    sha: b540bd8bc810d3885c6ea91e2018302f68baba2129ab3e88f32389ee9370880d
  - name: crossbeam-queue
    version: 0.3.4
    sha: 4dd435b205a4842da59efd07628f921c096bc1cc0a156835b4fa0bcb9a19bcce
  - name: crossbeam-utils
    version: 0.8.7
    sha: b5e5bed1f1c269533fa816a0a5492b3545209a205ca1a54842be180eb63a16a6
  - name: crypto-common
    version: 0.1.2
    sha: a4600d695eb3f6ce1cd44e6e291adceb2cc3ab12f20a33777ecd0bf6eba34e06
  - name: crypto-mac
    version: 0.8.0
    sha: b584a330336237c1eecd3e94266efb216c56ed91225d634cb2991c5f3fd1aeab
  - name: daemonize
    version: 0.4.1
    sha: 70c24513e34f53b640819f0ac9f705b673fcf4006d7aab8778bee72ebfc89815
  - name: deadpool
    version: 0.7.0
    sha: 3d126179d86aee4556e54f5f3c6bf6d9884e7cc52cef82f77ee6f90a7747616d
  - name: difflib
    version: 0.4.0
    sha: 6184e33543162437515c2e2b48714794e37845ec9851711914eec9d308f6ebe8
  - name: digest
    version: 0.10.2
    sha: 8cb780dce4f9a8f5c087362b3a4595936b2019e7c8b30f2c3e9a7e94e6ae9837
  - name: digest
    version: 0.9.0
    sha: d3dd60d1080a57a05ab032377049e0591415d2b31afd7028356dbf3cc6dcb066
  - name: directories
    version: 4.0.1
    sha: f51c5d4ddabd36886dd3e1438cb358cdcb0d7c499cb99cb4ac2e38e18b5cb210
  - name: dirs-sys
    version: 0.3.6
    sha: 03d86534ed367a67548dc68113a0f5db55432fdfbb6e6f9d77704397d95d5780
  - name: displaydoc
    version: 0.1.7
    sha: adc2ab4d5a16117f9029e9a6b5e4e79f4c67f6519bc134210d4d4a04ba31f41b
  - name: doc-comment
    version: 0.3.3
    sha: fea41bba32d969b513997752735605054bc0dfa92b4c56bf1189f2e174be7a10
  - name: dtoa
    version: 0.4.8
    sha: 56899898ce76aaf4a0f24d914c97ea6ed976d42fec6ad33fcbb0a1103e07b2b0
  - name: either
    version: 1.6.1
    sha: e78d4f1cc4ae33bbfc157ed5d5a5ef3bc29227303d595861deb238fcec4e9457
  - name: encoding_rs
    version: 0.8.30
    sha: 7896dc8abb250ffdda33912550faa54c88ec8b998dec0b2c55ab224921ce11df
  - name: env_logger
    version: 0.9.0
    sha: 0b2cf0344971ee6c64c31be0d530793fba457d322dfec2810c453d0ef228f9c3
  - name: error-chain
    version: 0.12.4
    sha: 2d2f06b9cac1506ece98fe3231e3cc9c4410ec3d5b1f24ae1c8946f0742cdefc
  - name: event-listener
    version: 2.5.2
    sha: 77f3309417938f28bf8228fcff79a4a37103981e3e186d2ccd19c74b38f4eb71
  - name: fastrand
    version: 1.7.0
    sha: c3fcf0cee53519c866c09b5de1f6c56ff9d647101f81c1964fa632e148896cdf
  - name: filetime
    version: 0.2.15
    sha: 975ccf83d8d9d0d84682850a38c8169027be83368805971cc4f238c2b245bc98
  - name: flate2
    version: 1.0.22
    sha: 1e6988e897c1c9c485f43b47a529cef42fde0547f9d8d41a7062518f1d8fc53f
  - name: float-cmp
    version: 0.9.0
    sha: 98de4bbd547a563b716d8dfa9aad1cb19bfab00f4fa09a6a4ed21dbcf44ce9c4
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
    version: 1.0.1
    sha: 5fc25a87fa4fd2094bffb06925852034d90a17f0d1e05197d4956d3555752191
  - name: fuchsia-cprng
    version: 0.1.1
    sha: a06f77d526c1a601b7c4cdd98f54b5eaabffc14d5f2f0296febdc7f357c6d3ba
  - name: futures
    version: 0.3.21
    sha: f73fe65f54d1e12b726f517d3e2135ca3125a437b6d998caf1962961f7172d9e
  - name: futures-channel
    version: 0.3.21
    sha: c3083ce4b914124575708913bca19bfe887522d6e2e6d0952943f5eac4a74010
  - name: futures-core
    version: 0.3.21
    sha: 0c09fd04b7e4073ac7156a9539b57a484a8ea920f79c7c675d05d289ab6110d3
  - name: futures-executor
    version: 0.3.21
    sha: 9420b90cfa29e327d0429f19be13e7ddb68fa1cccb09d65e5706b8c7a749b8a6
  - name: futures-io
    version: 0.3.21
    sha: fc4045962a5a5e935ee2fdedaa4e08284547402885ab326734432bed5d12966b
  - name: futures-lite
    version: 1.12.0
    sha: 7694489acd39452c77daa48516b894c153f192c3578d5a839b62c58099fcbf48
  - name: futures-locks
    version: 0.7.0
    sha: 3eb42d4fb72227be5778429f9ef5240a38a358925a49f05b5cf702ce7c7e558a
  - name: futures-macro
    version: 0.3.21
    sha: 33c1e13800337f4d4d7a316bf45a567dbcb6ffe087f16424852d97e97a91f512
  - name: futures-sink
    version: 0.3.21
    sha: 21163e139fa306126e6eedaf49ecdb4588f939600f0b1e770f4205ee4b7fa868
  - name: futures-task
    version: 0.3.21
    sha: 57c66a976bf5909d801bbef33416c41372779507e7a6b3a5e25e4749c58f776a
  - name: futures-timer
    version: 3.0.2
    sha: e64b03909df88034c26dc1547e8970b91f98bdb65165d6a4e9110d94263dbb2c
  - name: futures-util
    version: 0.3.21
    sha: d8b7abd5d659d9b90c8cba917f6ec750a74e2dc23902ef9cd4cc8c8b22e6036a
  - name: generic-array
    version: 0.14.5
    sha: fd48d33ec7f05fbfa152300fdad764757cbded343c1aa1cff2fbaf4134851803
  - name: getopts
    version: 0.2.21
    sha: 14dbbfd5c71d70241ecf9e6f13737f7b5ce823821063188d7e46c41d371eebd5
  - name: getrandom
    version: 0.1.16
    sha: 8fc3cb4d91f53b50155bdcfd23f6a4c39ae1969c2ae85982b135750cccaf5fce
  - name: getrandom
    version: 0.2.4
    sha: 418d37c8b1d42553c93648be529cb70f920d3baf8ef469b74b9638df426e0b4c
  - name: h2
    version: 0.3.11
    sha: d9f1f717ddc7b2ba36df7e871fd88db79326551d3d6f1fc406fbfd28b582ff8e
  - name: hashbrown
    version: 0.11.2
    sha: ab5ef0d4909ef3724cc8cce6ccc8572c5c817592e9285f5464f8e86f8bd3726e
  - name: hermit-abi
    version: 0.1.19
    sha: 62b467343b94ba476dcb2500d242dadbb39557df889310ac77c5d99100aaac33
  - name: hmac
    version: 0.12.0
    sha: ddca131f3e7f2ce2df364b57949a9d47915cfbd35e46cfee355ccebbf794d6a2
  - name: http
    version: 0.2.6
    sha: 31f4c6746584866f0feabcc69893c5b51beef3831656a968ed7ae254cdc4fd03
  - name: http-body
    version: 0.4.4
    sha: 1ff4f84919677303da5f147645dbea6b1881f368d03ac84e1dc09031ebd7b2c6
  - name: http-types
    version: 2.12.0
    sha: 6e9b187a72d63adbfba487f48095306ac823049cb504ee195541e91c7775f5ad
  - name: httparse
    version: 1.6.0
    sha: 9100414882e15fb7feccb4897e5f0ff0ff1ca7d1a86a23208ada4d7a18e6c6c4
  - name: httpdate
    version: 1.0.2
    sha: c4a1e36c821dbe04574f602848a19f742f4fb3c98d40449f11bcad18d6b17421
  - name: humantime
    version: 2.1.0
    sha: 9a3a5bfb195931eeb336b2a7b4d761daec841b97f947d34394601737a7bba5e4
  - name: hyper
    version: 0.14.17
    sha: 043f0e083e9901b6cc658a77d1eb86f4fc650bbb977a4337dd63192826aa85dd
  - name: hyper-tls
    version: 0.5.0
    sha: d6183ddfa99b85da61a140bea0efc93fdf56ceaa041b37d553518030827f9905
  - name: hyperx
    version: 1.4.0
    sha: 5617e92fc2f2501c3e2bc6ce547cad841adba2bae5b921c7e52510beca6d084c
  - name: idna
    version: 0.2.3
    sha: 418a0a6fab821475f634efe3ccc45c013f742efe03d853e8d3355d5cb850ecf8
  - name: indexmap
    version: 1.8.0
    sha: 282a6247722caba404c065016bbfa522806e51714c34f5dfc3e4a3a46fcb4223
  - name: infer
    version: 0.2.3
    sha: 64e9829a50b42bb782c1df523f78d332fe371b10c661e78b7a3c34b0198e9fac
  - name: instant
    version: 0.1.12
    sha: 7a5bbe824c507c5da5956355e86a746d82e0e1464f65d862cc5e71da70e94b2c
  - name: ipnet
    version: 2.3.1
    sha: 68f2d64f2edebec4ce84ad108148e67e1064789bee435edc5b60ad398714a3a9
  - name: itertools
    version: 0.10.3
    sha: a9a9d19fa1e79b6215ff29b9d6880b706147f16e9b1dbb1e4e5947b5b02bc5e3
  - name: itoa
    version: 0.4.8
    sha: b71991ff56294aa922b450139ee08b3bfc70982c6b2c7562771375cf73542dd4
  - name: itoa
    version: 1.0.1
    sha: 1aab8fc367588b89dcee83ab0fd66b72b50b72fa1904d7095045ace2b0c81c35
  - name: jobserver
    version: 0.1.24
    sha: af25a77299a7f711a01975c35a6a424eb6862092cc2d6c72c4ed6cbc56dfc1fa
  - name: js-sys
    version: 0.3.56
    sha: a38fc24e30fd564ce974c02bf1d337caddff65be6cc4735a1f7eab22a7440f04
  - name: jsonwebtoken
    version: 7.2.0
    sha: afabcc15e437a6484fc4f12d0fd63068fe457bf93f1c148d3d9649c60b103f32
  - name: kernel32-sys
    version: 0.2.2
    sha: 7507624b29483431c0ba2d82aece8ca6cdba9382bff4ddd0f7490560c056098d
  - name: language-tags
    version: 0.3.2
    sha: d4345964bb142484797b161f473a503a434de77149dd8c7427788c6e13379388
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: lexical-core
    version: 0.7.6
    sha: 6607c62aa161d23d17a9072cc5da0be67cdfc89d3afb1e8d9c842bebc2525ffe
  - name: libc
    version: 0.2.117
    sha: e74d72e0f9b65b5b4ca49a346af3976df0f9c61d550727f349ecd559f251a26c
  - name: libmount
    version: 0.1.15
    sha: 23c4c2ad2d5cbd2f5a05620c3daf45930add53ec207fa99ce5eec971089dc35f
  - name: linked-hash-map
    version: 0.5.4
    sha: 7fb9b38af92608140b86b693604b9ffcc5824240a484d1ecd4795bacb2fe88f3
  - name: local-encoding
    version: 0.2.0
    sha: e1ceb20f39ff7ae42f3ff9795f3986b1daad821caaa1e1732a0944103a5a1a66
  - name: lock_api
    version: 0.4.6
    sha: 88943dd7ef4a2e5a4bfa2753aaab3013e34ce2533d1996fb18ef591e315e2b3b
  - name: log
    version: 0.4.14
    sha: 51b9bbe6c47d51fc3e1a9b945965946b4c44142ab8792c50835a980d362c2710
  - name: matches
    version: 0.1.9
    sha: a3e378b66a060d48947b590737b30a1be76706c8dd7b8ba0f2fe3989c68a853f
  - name: md-5
    version: 0.10.0
    sha: e6a38fc55c8bbc10058782919516f88826e70320db6d206aebc49611d24216ae
  - name: md5
    version: 0.3.8
    sha: 79c56d6a0b07f9e19282511c83fc5b086364cbae4ba8c7d5f190c3d9b0425a48
  - name: memcached-rs
    version: 0.4.2
    sha: e3804643a8b556cd39d6b3ec5a181fdf36b642952d57f4c52bebd457d0ee0b1d
  - name: memchr
    version: 2.4.1
    sha: 308cc39be01b73d0d18f82a0e7b2a3df85245f84af96fdddc5d202d27e47b86a
  - name: memoffset
    version: 0.6.5
    sha: 5aa361d4faea93603064a027415f07bd8e1d5c88c9fbf68bf56a285428fd79ce
  - name: mime
    version: 0.3.16
    sha: 2a60c7ce501c71e03a9c9c0d35b861413ae925bd979cc7a4e30d060069aaac8d
  - name: mime_guess
    version: 2.0.3
    sha: 2684d4c2e97d99848d30b324b00c8fcc7e5c897b7cbb5819b09e7c90e8baf212
  - name: miniz_oxide
    version: 0.4.4
    sha: a92518e98c078586bc6c934028adcca4c92a53d6a958196de835170a01d84e4b
  - name: mio
    version: 0.7.14
    sha: 8067b404fe97c70829f082dec8bcf4f71225d7eaea1d8645349cb76fa06205cc
  - name: miow
    version: 0.3.7
    sha: b9f1c5b025cda876f66ef43a113f91ebc9f4ccef34843000e0adf6ebbab84e21
  - name: multipart
    version: 0.18.0
    sha: 00dec633863867f29cb39df64a397cdf4a6354708ddd7759f70c7fb51c5f9182
  - name: native-tls
    version: 0.2.8
    sha: 48ba9f7719b5a0f42f338907614285fb5fd70e53858141f69898a1fb7203b24d
  - name: nix
    version: 0.14.1
    sha: 6c722bee1037d430d0f8e687bbdbf222f27cc6e4e68d5caf630857bb2b6dbdce
  - name: nix
    version: 0.23.1
    sha: 9f866317acbd3a240710c63f065ffb1e4fd466259045ccb504130b7f668f35c6
  - name: nom
    version: 5.1.2
    sha: ffb4262d26ed83a1c0a33a38fe2bb15797329c85770da05e6b828ddb782627af
  - name: normalize-line-endings
    version: 0.3.0
    sha: 61807f77802ff30975e01f4f071c8ba10c022052f98b3294119f3e615d13e5be
  - name: ntapi
    version: 0.3.7
    sha: c28774a7fd2fbb4f0babd8237ce554b73af68021b5f695a3cebd6c59bac0980f
  - name: num-bigint
    version: 0.2.6
    sha: 090c7f9998ee0ff65aa5b723e4009f7b217707f1fb5ea551329cc4d6231fb304
  - name: num-integer
    version: 0.1.44
    sha: d2cc698a63b549a70bc047073d2949cce27cd1c7b0a4a862d08a8031bc2801db
  - name: num-traits
    version: 0.2.14
    sha: 9a64b1ec5cda2586e284722486d802acf1f7dbdc623e2bfc57e65ca1cd099290
  - name: num_cpus
    version: 1.13.1
    sha: 19e64526ebdee182341572e50e9ad03965aa510cd94427a4549448f285e957a1
  - name: number_prefix
    version: 0.4.0
    sha: 830b246a0e5f20af87141b25c173cd1b609bd7779a4617d6ec582abaf90870f3
  - name: once_cell
    version: 1.9.0
    sha: da32515d9f6e6e489d7bc9d84c71b060db7247dc035bbe44eac88cf87486d8d5
  - name: openssl
    version: 0.10.38
    sha: 0c7ae222234c30df141154f159066c5093ff73b63204dcda7121eb082fc56a95
  - name: openssl-probe
    version: 0.1.5
    sha: ff011a302c396a5197692431fc1948019154afc178baf7d8e37367442a4601cf
  - name: openssl-sys
    version: 0.9.72
    sha: 7e46109c383602735fa0a2e48dd2b7c892b048e1bf69e5c3b1d804b7d9c203cb
  - name: parity-tokio-ipc
    version: 0.9.0
    sha: 9981e32fb75e004cc148f5fb70342f393830e0a4aa62e3cc93b50976218d42b6
  - name: parking
    version: 2.0.0
    sha: 427c3892f9e783d91cc128285287e70a59e206ca452770ece88a76f7a3eddd72
  - name: parking_lot
    version: 0.11.2
    sha: 7d17b78036a60663b797adeaee46f5c9dfebb86948d1255007a1d6be0271ff99
  - name: parking_lot_core
    version: 0.8.5
    sha: d76e8e1493bcac0d2766c42737f34458f1c8c50c0d23bcb24ea953affb273216
  - name: pem
    version: 0.8.3
    sha: fd56cbd21fea48d0c440b41cd69c589faacade08c992d9a54e471b79d0fd13eb
  - name: percent-encoding
    version: 2.1.0
    sha: d4fd5641d01c8f18a23da7b6fe29298ff4b55afcccdf78973b24cf3175fee32e
  - name: pin-project
    version: 1.0.10
    sha: 58ad3879ad3baf4e44784bc6a718a8698867bb991f8ce24d1bcbe2cfb4c3a75e
  - name: pin-project-internal
    version: 1.0.10
    sha: 744b6f092ba29c3650faf274db506afd39944f48420f6c86b17cfe0ee1cb36bb
  - name: pin-project-lite
    version: 0.2.8
    sha: e280fbe77cc62c91527259e9442153f4688736748d24660126286329742b4c6c
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.24
    sha: 58893f751c9b0412871a09abd62ecd2a00298c6c83befa223ef98c52aef40cbe
  - name: ppv-lite86
    version: 0.2.16
    sha: eb9f9e6e233e5c4a35559a617bf40a4ec447db2e84c20b55a6f83167b7e57872
  - name: predicates
    version: 2.1.1
    sha: a5aab5be6e4732b473071984b3164dbbfb7a3674d30ea5ff44410b6bcd960c3c
  - name: predicates-core
    version: 1.0.3
    sha: da1c2388b1513e1b605fcec39a95e0a9e8ef088f71443ef37099fa9ae6673fcb
  - name: predicates-tree
    version: 1.0.5
    sha: 4d86de6de25020a36c6d3643a86d9a6a9f552107c0559c60ea03551b5e16c032
  - name: proc-macro2
    version: 1.0.36
    sha: c7342d5883fbccae1cc37a2353b09c87c9b0f3afd73f5fb9bba687a1f733b029
  - name: pulldown-cmark
    version: 0.0.3
    sha: 8361e81576d2e02643b04950e487ec172b687180da65c731c03cf336784e6c07
  - name: quick-error
    version: 1.2.3
    sha: a1d01941d82fa2ab50be1e79e6714289dd7cde78eba4c074bc5a4374f650dfe0
  - name: quote
    version: 1.0.15
    sha: 864d3e96a899863136fc6e99f3d7cae289dafe43bf2c5ac19b70df7210c0a145
  - name: rand
    version: 0.4.6
    sha: 552840b97013b1a26992c11eac34bdd778e464601a4c2054b5f0bff7c6761293
  - name: rand
    version: 0.7.3
    sha: 6a6b1679d49b24bbfe0c803429aa1874472f50d9b363131f0e89fc356b544d03
  - name: rand
    version: 0.8.4
    sha: 2e7573632e6454cf6b99d7aac4ccca54be06da05aca2ef7423d22d27d4d4bcd8
  - name: rand_chacha
    version: 0.2.2
    sha: f4c8ed856279c9737206bf725bf36935d8666ead7aa69b52be55af369d193402
  - name: rand_chacha
    version: 0.3.1
    sha: e6c10a63a0fa32252be49d21e7709d4d4baf8d231c2dbce1eaa8141b9b127d88
  - name: rand_core
    version: 0.3.1
    sha: 7a6fdeb83b075e8266dcc8762c22776f6877a63111121f5f8c7411e5be7eed4b
  - name: rand_core
    version: 0.4.2
    sha: 9c33a3c44ca05fa6f1807d8e6743f3824e8509beca625669633be0acbdf509dc
  - name: rand_core
    version: 0.5.1
    sha: 90bde5296fc891b0cef12a6d03ddccc162ce7b2aff54160af9338f8d40df6d19
  - name: rand_core
    version: 0.6.3
    sha: d34f1408f55294453790c48b2f1ebbb1c5b4b7563eb1f418bcfcfdbb06ebb4e7
  - name: rand_hc
    version: 0.2.0
    sha: ca3129af7b92a17112d59ad498c6f81eaf463253766b90396d39ea7a39d6613c
  - name: rand_hc
    version: 0.3.1
    sha: d51e9f596de227fda2ea6c84607f5558e196eeaf43c986b724ba4fb8fdf497e7
  - name: rdrand
    version: 0.4.0
    sha: 678054eb77286b51581ba43620cc911abf02758c91f93f479767aed0f90458b2
  - name: redis
    version: 0.21.5
    sha: 1a80b5f38d7f5a020856a0e16e40a9cfabf88ae8f0e4c2dcd8a3114c1e470852
  - name: redox_syscall
    version: 0.2.10
    sha: 8383f39639269cde97d255a32bdb68c047337295414940c68bdd30c2e13203ff
  - name: redox_users
    version: 0.4.0
    sha: 528532f3d801c87aec9def2add9ca802fe569e44a544afe633765267840abe64
  - name: regex
    version: 1.5.4
    sha: d07a8629359eb56f1e2fb1652bb04212c072a87ba68546a04065d525673ac461
  - name: regex-automata
    version: 0.1.10
    sha: 6c230d73fb8d8c1b9c0b3135c5142a8acee3a0558fb8db5cf1cb65f8d7862132
  - name: regex-syntax
    version: 0.6.25
    sha: f497285884f3fcff424ffc933e56d7cbca511def0c9831a7f9b5f6153e3cc89b
  - name: remove_dir_all
    version: 0.5.3
    sha: 3acd125665422973a33ac9d3dd2df85edad0f4ae9b00dafb1a05e43a9f5ef8e7
  - name: reqwest
    version: 0.11.9
    sha: 87f242f1488a539a79bac6dbe7c8609ae43b7914b7736210f239a37cccb32525
  - name: retry
    version: 1.3.1
    sha: ac95c60a949a63fd2822f4964939662d8f2c16c4fa0624fd954bc6e703b9a3f6
  - name: ring
    version: 0.16.20
    sha: 3053cf52e236a3ed746dfc745aa9cacf1b791d846bdaf412f60a8d7d6e17c8fc
  - name: rouille
    version: 3.4.0
    sha: 660f081d053ac1dce7c2683a3f2818d9b60a293bc142e7d3de36fd5541ebb671
  - name: ryu
    version: 1.0.9
    sha: 73b4b750c782965c211b42f022f59af1fbceabdd026623714f104152f1ec149f
  - name: safemem
    version: 0.3.3
    sha: ef703b7cb59335eae2eb93ceb664c0eb7ea6bf567079d843e09420219668e072
  - name: same-file
    version: 1.0.6
    sha: 93fc1dc3aaa9bfed95e02e6eadabb4baf7e3078b0bd1b4d7b6b0b68378900502
  - name: schannel
    version: 0.1.19
    sha: 8f05ba609c234e60bee0d547fe94a4c7e9da733d1c962cf6e59efa4cd9c8bc75
  - name: scopeguard
    version: 1.1.0
    sha: d29ab0c6d3fc0ee92fe66e2d99f700eab17a8d57d1c1d3b748380fb20baa78cd
  - name: security-framework
    version: 2.6.1
    sha: 2dc14f172faf8a0194a3aded622712b0de276821addc574fa54fc0a1167e10dc
  - name: security-framework-sys
    version: 2.6.1
    sha: 0160a13a177a45bfb43ce71c01580998474f556ad854dcbca936dd2841a5c556
  - name: semver
    version: 0.9.0
    sha: 1d7eb9ef2c18661902cc47e535f9bc51b78acd254da71d375c2f6720d9a40403
  - name: semver-parser
    version: 0.7.0
    sha: 388a1df253eca08550bef6c72392cfe7c30914bf41df5269b68cbd6ff8f570a3
  - name: serde
    version: 1.0.136
    sha: ce31e24b01e1e524df96f1c2fdd054405f8d7376249a5110886fb4b658484789
  - name: serde_derive
    version: 1.0.136
    sha: 08597e7152fcd306f41838ed3e37be9eaeed2b61c42e2117266a554fab4662f9
  - name: serde_json
    version: 1.0.79
    sha: 8e8d9fa5c3b304765ce1fd9c4c8a3de2c8db365a5b91be52f186efc675681d95
  - name: serde_qs
    version: 0.8.5
    sha: c7715380eec75f029a4ef7de39a9200e0a63823176b759d055b613f5a87df6a6
  - name: serde_repr
    version: 0.1.7
    sha: 98d0516900518c29efa217c298fa1f4e6c6ffc85ae29fd7f4ee48f176e1a9ed5
  - name: serde_urlencoded
    version: 0.7.1
    sha: d3491c14715ca2294c4d6a88f15e84739788c1d030eed8c110436aafdaa2f3fd
  - name: serial_test
    version: 0.5.1
    sha: e0bccbcf40c8938196944a3da0e133e031a33f4d6b72db3bda3cc556e361905d
  - name: serial_test_derive
    version: 0.5.1
    sha: b2acd6defeddb41eb60bb468f8825d0cfd0c2a76bc03bfd235b6a1dc4f6a1ad5
  - name: sha-1
    version: 0.10.0
    sha: 028f48d513f9678cda28f6e4064755b3fbb2af6acd672f2c209b62323f7aea0f
  - name: sha1
    version: 0.6.1
    sha: c1da05c97445caa12d05e848c4a4fcbbea29e748ac28f7e80e9b010392063770
  - name: sha1_smol
    version: 1.0.0
    sha: ae1a47186c03a32177042e55dbc5fd5aee900b8e0069a8d70fba96a9375cd012
  - name: sha2
    version: 0.10.1
    sha: 99c3bd8169c58782adad9290a9af5939994036b76187f7b4f0e6de91dbbfc0ec
  - name: signal-hook-registry
    version: 1.4.0
    sha: e51e73328dc4ac0c7ccbda3a494dfa03df1de2f46018127f60c693f2648455b0
  - name: simple_asn1
    version: 0.4.1
    sha: 692ca13de57ce0613a363c8c2f1de925adebc81b04c923ac60c5488bb44abe4b
  - name: skeptic
    version: 0.4.0
    sha: 24ebf8a06f5f8bae61ae5bbc7af7aac4ef6907ae975130faba1199e5fe82256a
  - name: slab
    version: 0.4.5
    sha: 9def91fd1e018fe007022791f865d0ccc9b3a0d5001e01aabb8b40e46000afb5
  - name: smallvec
    version: 1.8.0
    sha: f2dd574626839106c320a323308629dcb1acfc96e32a8cba364ddc61ac23ee83
  - name: socket2
    version: 0.4.4
    sha: 66d72b759436ae32898a2af0a14218dbf55efde3feeb170eb623637db85ee1e0
  - name: spin
    version: 0.5.2
    sha: 6e63cff320ae2c57904679ba7cb63280a3dc4613885beafb148ee7bf9aa9042d
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
    version: 0.8.0
    sha: 8ea5119cdb4c55b55d432abb513a0429384878c15dde60cc77b1c99de1a95a6a
  - name: subtle
    version: 2.4.1
    sha: 6bdef32e8150c2a081110b42772ffe7d7c9032b606bc226c8260fd97e0976601
  - name: syn
    version: 1.0.86
    sha: 8a65b3f4ffa0092e9887669db0eae07941f023991ab58ea44da8fe8e2d511c6b
  - name: syslog
    version: 5.0.0
    sha: 9a5d8ef1b679c07976f3ee336a436453760c470f54b5e7237556728b8589515d
  - name: tar
    version: 0.4.38
    sha: 4b55807c0344e1e6c04d7c965f5289c39a8d94ae23ed5c0b57aabac549f871c6
  - name: tempdir
    version: 0.3.7
    sha: 15f2b5fb00ccdf689e0149d1b1b3c03fead81c2b37735d812fa8bddbbf41b6d8
  - name: tempfile
    version: 3.3.0
    sha: 5cdb1ef4eaeeaddc8fbd371e5017057064af0911902ef36b39801f67cc6d79e4
  - name: termcolor
    version: 1.1.2
    sha: 2dfed899f0eb03f32ee8c6a0aabdb8a7949659e3466561fc0adf54e26d88c5f4
  - name: termtree
    version: 0.2.4
    sha: 507e9898683b6c43a9aa55b64259b721b52ba226e0f3779137e50ad114a4c90b
  - name: textwrap
    version: 0.11.0
    sha: d326610f408c7a4eb6f51c37c330e496b08506c9457c9d34287ecc38809fb060
  - name: thirtyfour
    version: 0.27.3
    sha: 6ac2540aff94b9f8d89a94bb5d8cc5e71560f78ee8f6c953cd31469083c61f6d
  - name: thirtyfour_sync
    version: 0.27.1
    sha: ab1e47e6c2fed609d851c6f6171a559ecffb1d121f2d6e02dd390e90ea2c3d38
  - name: thiserror
    version: 1.0.30
    sha: 854babe52e4df1653706b98fcfc05843010039b406875930a70e4d9644e5c417
  - name: thiserror-impl
    version: 1.0.30
    sha: aa32fd3f627f367fe16f893e2597ae3c05020f8bba2666a4e6ea73d377e5714b
  - name: threadpool
    version: 1.8.1
    sha: d050e60b33d41c19108b32cea32164033a9013fe3b46cbd4457559bfbf77afaa
  - name: time
    version: 0.1.43
    sha: ca8a50ef2360fbd1eeb0ecd46795a87a19024eb4b53c5dc916ca1fd95fe62438
  - name: time
    version: 0.3.2
    sha: 3e0a10c9a9fb3a5dce8c2239ed670f1a2569fcf42da035f5face1b19860d52b0
  - name: tiny_http
    version: 0.8.2
    sha: 9ce51b50006056f590c9b7c3808c3bd70f0d1101666629713866c227d6e58d39
  - name: tinyvec
    version: 1.5.1
    sha: 2c1c1d5a42b6245520c249549ec267180beaffcc0615401ac8e31853d4b6d8d2
  - name: tinyvec_macros
    version: 0.1.0
    sha: cda74da7e1a664f795bb1f8a87ec406fb89a02522cf6e50620d016add6dbbf5c
  - name: tokio
    version: 1.16.1
    sha: 0c27a64b625de6d309e8c57716ba93021dccf1b3b5c97edd6d3dd2d2135afc0a
  - name: tokio-macros
    version: 1.7.0
    sha: b557f72f448c511a979e2564e55d74e6c4432fc96ff4f6241bc6bded342643b7
  - name: tokio-native-tls
    version: 0.3.0
    sha: f7d995660bd2b7f8c1568414c1126076c13fbb725c40112dc0120b78eb9b717b
  - name: tokio-serde
    version: 0.8.0
    sha: 911a61637386b789af998ee23f50aa30d5fd7edcec8d6d3dedae5e5815205466
  - name: tokio-util
    version: 0.6.9
    sha: 9e99e1983e5d376cd8eb4b66604d2e99e79f5bd988c3055891dcd8c9e2604cc0
  - name: toml
    version: 0.5.8
    sha: a31142970826733df8241ef35dc040ef98c679ab14d7c3e54d827099b3acecaa
  - name: tower
    version: 0.4.11
    sha: 5651b5f6860a99bd1adb59dbfe1db8beb433e73709d9032b413a77e2fb7c066a
  - name: tower-layer
    version: 0.3.1
    sha: 343bc9466d3fe6b0f960ef45960509f84480bf4fd96f92901afe7ff3df9d3a62
  - name: tower-service
    version: 0.3.1
    sha: 360dfd1d6d30e05fda32ace2c8c70e9c0a9da713275777f5a4dbb8a1893930c6
  - name: tracing
    version: 0.1.30
    sha: 2d8d93354fe2a8e50d5953f5ae2e47a3fc2ef03292e7ea46e3cc38f549525fb9
  - name: tracing-attributes
    version: 0.1.19
    sha: 8276d9a4a3a558d7b7ad5303ad50b53d58264641b82914b7ada36bd762e7a716
  - name: tracing-core
    version: 0.1.22
    sha: 03cfcb51380632a72d3111cb8d3447a8d908e577d31beeac006f836383d29a23
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
    version: 0.3.7
    sha: 1a01404663e3db436ed2746d9fefef640d868edae3cceb81c3b8d5732fda678f
  - name: unicode-normalization
    version: 0.1.19
    sha: d54590932941a9e9266f0832deed84ebe1bf2e4c9e4a3554d393d18f5e854bf9
  - name: unicode-width
    version: 0.1.9
    sha: 3ed742d4ea2bd1176e236172c8429aaf54486e7ac098db29ffe6529e0ce50973
  - name: unicode-xid
    version: 0.2.2
    sha: 8ccb82d61f80a663efe1f787a51b16b5a51e3314d6ac365b08639f52387b33f3
  - name: unix_socket
    version: 0.5.0
    sha: 6aa2700417c405c38f5e6902d699345241c28c0b7ade4abaad71e35a87eb1564
  - name: untrusted
    version: 0.7.1
    sha: a156c684c91ea7d62626509bce3cb4e1d9ed5c4d978f7b4352658f96a4c26b4a
  - name: url
    version: 2.2.2
    sha: a507c383b2d33b5fc35d1861e77e6b383d158b2da5e14fe51b83dfedf6fd578c
  - name: urlparse
    version: 0.7.3
    sha: 110352d4e9076c67839003c7788d8604e24dcded13e0b375af3efaa8cf468517
  - name: utf8parse
    version: 0.2.0
    sha: 936e4b492acfd135421d8dca4b1aa80a7bfc26e702ef3af710e0752684df5372
  - name: uuid
    version: 0.8.2
    sha: bc5cf98d8186244414c848017f0e2676b3fcb46807f6668a97dfe67359a3c4b7
  - name: vcpkg
    version: 0.2.15
    sha: accd4ea62f7bb7a82fe23066fb0957d48ef677f6eeb8215f372f52e48bb32426
  - name: vec_map
    version: 0.8.2
    sha: f1bddf1187be692e79c5ffeab891132dfb0f236ed36a43c7ed39f1165ee20191
  - name: version-compare
    version: 0.1.0
    sha: fe88247b92c1df6b6de80ddc290f3976dbdf2f5f5d3fd049a9fb598c6dd5ca73
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
  - name: waker-fn
    version: 1.1.0
    sha: 9d5b2c62b4012a3e1eca5a7e077d13b3bf498c4073e33ccd58626607748ceeca
  - name: walkdir
    version: 2.3.2
    sha: 808cf2735cd4b6866113f648b791c6adc5714537bc222d9347bb203386ffda56
  - name: want
    version: 0.3.0
    sha: 1ce8a968cb1cd110d136ff8b819a556d6fb6d919363c61534f6860c7eb172ba0
  - name: wasi
    version: 0.10.2+wasi-snapshot-preview1
    sha: fd6fbd9a79829dd1ad0cc20627bf1ed606756a7f77edff7b66b7064f9cb327c6
  - name: wasi
    version: 0.9.0+wasi-snapshot-preview1
    sha: cccddf32554fecc6acb585f82a32a72e28b48f8c4c1883ddfeeeaa96f7d8e519
  - name: wasm-bindgen
    version: 0.2.79
    sha: 25f1af7423d8588a3d840681122e72e6a24ddbcb3f0ec385cac0d12d24256c06
  - name: wasm-bindgen-backend
    version: 0.2.79
    sha: 8b21c0df030f5a177f3cba22e9bc4322695ec43e7257d865302900290bcdedca
  - name: wasm-bindgen-futures
    version: 0.4.29
    sha: 2eb6ec270a31b1d3c7e266b999739109abce8b6c87e4b31fcfcd788b65267395
  - name: wasm-bindgen-macro
    version: 0.2.79
    sha: 2f4203d69e40a52ee523b2529a773d5ffc1dc0071801c87b3d270b471b80ed01
  - name: wasm-bindgen-macro-support
    version: 0.2.79
    sha: bfa8a30d46208db204854cadbb5d4baf5fcf8071ba5bf48190c3e59937962ebc
  - name: wasm-bindgen-shared
    version: 0.2.79
    sha: 3d958d035c4438e28c70e4321a2911302f10135ce78a9c7834c0cab4123d06a2
  - name: web-sys
    version: 0.3.56
    sha: c060b319f29dd25724f09a2ba1418f142f539b2be99fbf4d2d5a8f7330afb8eb
  - name: which
    version: 4.2.4
    sha: 2a5a7e487e921cf220206864a94a89b6c6905bfc19f1057fa26a4cb360e5c1d2
  - name: winapi
    version: 0.2.8
    sha: 167dc9d6949a9b857f3451275e911c3f44255842c1f7a76f33c55103a909087a
  - name: winapi
    version: 0.3.9
    sha: 5c839a674fcd7a98952e593242ea400abe93992746761e38641405d28b00f419
  - name: winapi-build
    version: 0.1.1
    sha: 2d315eee3b34aca4797b2da6b13ed88266e6d612562a0c46390af8299fc699bc
  - name: winapi-i686-pc-windows-gnu
    version: 0.4.0
    sha: ac3b87c63620426dd9b991e5ce0329eff545bccbbb34f3be09ff6fb6ab51b7b6
  - name: winapi-util
    version: 0.1.5
    sha: 70ec6ce85bb158151cae5e5c87f95a8e97d2c0c4b001223f33a334e3ce5de178
  - name: winapi-x86_64-pc-windows-gnu
    version: 0.4.0
    sha: 712e227841d057c1ee1cd2fb22fa7e5a5461ae8e48fa2ca79ec42cfc1931183f
  - name: winreg
    version: 0.7.0
    sha: 0120db82e8a1e0b9fb3345a539c478767c0048d842860994d96113d5b667bd69
  - name: wiremock
    version: 0.4.9
    sha: 860c1b991b29b6bc5096b46bdaaea904417ed12fda814d38ccf1b869c13629c8
  - name: xattr
    version: 0.2.2
    sha: 244c3741f4240ef46274860397c7c74e50eb23624996930e484c16679633a54c
  - name: zip
    version: 0.5.13
    sha: 93ab48844d61251bb3835145c521d88aa4031d7139e8485990f60ca911fa0815
  - name: zstd
    version: 0.6.1+zstd.1.4.9
    sha: 5de55e77f798f205d8561b8fe2ef57abfb6e0ff2abe7fd3c089e119cdb5631a3
  - name: zstd-safe
    version: 3.0.1+zstd.1.4.9
    sha: 1387cabcd938127b30ce78c4bf00b30387dddf704e3f0881dbc4ff62b5566f8c
  - name: zstd-sys
    version: 1.4.20+zstd.1.4.9
    sha: ebd5b733d7cf2d9447e2c3e76a5589b4f5e5ae065c22a2bc0b023cbc331b6c8e
