kind: manual

build-depends:
- freedesktop-sdk.bst:components/rust.bst
- freedesktop-sdk.bst:components/libffi.bst
- freedesktop-sdk.bst:components/pkg-config.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

environment:
  PATH: /usr/bin:/usr/lib/sdk/rust/bin

config:
  build-commands:
  - |
    cargo --offline build --release

  install-commands:
  - |
    install -D -m a+rx -t "%{install-root}%{bindir}" ./target/release/cargo-cbuild

public:
  bst:
    integration-commands:
    - |
      cargo-cbuild --help

sources:
- kind: tar
  url: https://static.crates.io/crates/cargo-c/cargo-c-0.9.12+cargo-0.64.crate
  ref: b75aa4b381ea5da82b1f1b64e5fc410a66126f166aed9555be02a72e58c685bb
- kind: cargo
  ref:
  - name: adler
    version: 1.0.2
    sha: f26201604c87b1e01bd3d98f8d5d9a8fcbb815e8cedb41ffccbeb4bf593a35fe
  - name: aho-corasick
    version: 0.7.18
    sha: 1e37cfd5e7657ada45f742d6e99ca5788580b5c529dc78faf11ece6dc702656f
  - name: anyhow
    version: 1.0.61
    sha: 508b352bb5c066aac251f6daf6b36eccd03e8a88e8081cd44959ea277a3af9a8
  - name: arrayvec
    version: 0.5.2
    sha: 23b62fc65de8e4e7f52534fb52b0f3ed04746ae267519eef2a83941e8085068b
  - name: atty
    version: 0.2.14
    sha: d9b39be18770d11421cdb1b9947a45dd3f37e93092cbf377614828a319d5fee8
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: bitflags
    version: 1.3.2
    sha: bef38d45163c2f1dde094a7dfd33ccf595c92905c8f8f4fdc18d06fb1037718a
  - name: bitmaps
    version: 2.1.0
    sha: 031043d04099746d8db04daf1fa424b2bc8bd69d92b25962dcde24da39ab64a2
  - name: bstr
    version: 0.2.17
    sha: ba3569f383e8f1598449f1a423e72e99569137b47740b1da11ef19af3d5c3223
  - name: bytes
    version: 1.2.1
    sha: ec8a7b6a70fde80372154c65702f00a0f56f3e1c36abbc6c440484be248856db
  - name: bytesize
    version: 1.1.0
    sha: 6c58ec36aac5066d5ca17df51b3e70279f5670a72102f5752cb7e7c856adfc70
  - name: cargo
    version: 0.64.0
    sha: 7019448b7d0ffe19d4ab26a340d2efe6da8cf86c8cc01a352b90853e31cd8f7c
  - name: cargo-platform
    version: 0.1.2
    sha: cbdb825da8a5df079a43676dbe042702f1707b1109f713a01420fbb4cc71fa27
  - name: cargo-util
    version: 0.2.1
    sha: eb66f33d96c58d1eef3a4744556ce0fae012b01165a3f171169a15cb4efc9633
  - name: cbindgen
    version: 0.24.3
    sha: a6358dedf60f4d9b8db43ad187391afe959746101346fe51bb978126bec61dfb
  - name: cc
    version: 1.0.73
    sha: 2fff2a6927b3bb87f9595d67196a70493f627687a71d87a0d692242c33f58c11
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: clap
    version: 3.2.17
    sha: 29e724a68d9319343bb3328c9cc2dfde263f4b3142ee1059a9980580171c954b
  - name: clap_derive
    version: 3.2.17
    sha: 13547f7012c01ab4a0e8f8967730ada8f9fdf419e8b6c792788f39cf4e46eefa
  - name: clap_lex
    version: 0.2.4
    sha: 2850f2f5a82cbf437dd5af4d49848fbdfc27c157c3d010345776f952765261c5
  - name: combine
    version: 4.6.6
    sha: 35ed6e9d84f0b51a7f52daf1c7d71dd136fd7a3f41a8462b8cdb8c78d920fad4
  - name: commoncrypto
    version: 0.2.0
    sha: d056a8586ba25a1e4d61cb090900e495952c7886786fc55f909ab2f819b69007
  - name: commoncrypto-sys
    version: 0.2.0
    sha: 1fed34f46747aa73dfaa578069fd8279d2818ade2b55f38f22a9401c7f4083e2
  - name: core-foundation
    version: 0.9.3
    sha: 194a7a9e6de53fa55116934067c844d9d749312f75c6f6d0980e8c252f8c2146
  - name: core-foundation-sys
    version: 0.8.3
    sha: 5827cebf4670468b8772dd191856768aedcb1b0278a04f989f7766351917b9dc
  - name: crates-io
    version: 0.34.0
    sha: 6b4a87459133b2e708195eaab34be55039bc30e0d120658bd40794bb00b6328d
  - name: crc32fast
    version: 1.3.2
    sha: b540bd8bc810d3885c6ea91e2018302f68baba2129ab3e88f32389ee9370880d
  - name: crossbeam-utils
    version: 0.8.11
    sha: 51887d4adc7b564537b15adcfb307936f8075dfcd5f00dde9a9f1d29383682bc
  - name: crypto-hash
    version: 0.3.4
    sha: 8a77162240fd97248d19a564a565eb563a3f592b386e4136fb300909e67dddca
  - name: curl
    version: 0.4.44
    sha: 509bd11746c7ac09ebd19f0b17782eae80aadee26237658a6b4808afb5c11a22
  - name: curl-sys
    version: 0.4.56+curl-7.83.1
    sha: 6093e169dd4de29e468fa649fbae11cdcd5551c81fe5bf1b0677adad7ef3d26f
  - name: either
    version: 1.7.0
    sha: 3f107b87b6afc2a64fd13cac55fe06d6c8859f12d4b14cbcdd2c67d0976781be
  - name: env_logger
    version: 0.9.0
    sha: 0b2cf0344971ee6c64c31be0d530793fba457d322dfec2810c453d0ef228f9c3
  - name: fastrand
    version: 1.8.0
    sha: a7a407cfaa3385c4ae6b23e84623d48c2798d06e3e6a1878f7f59f17b3f86499
  - name: filetime
    version: 0.2.17
    sha: e94a7bbaa59354bc20dd75b67f23e2797b4490e9d6928203fb105c79e448c86c
  - name: flate2
    version: 1.0.24
    sha: f82b0f4c27ad9f8bfd1f3208d882da2b09c301bc1c828fd3a00d0216d2fbbff6
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
  - name: fwdansi
    version: 1.1.0
    sha: 08c1f5787fe85505d1f7777268db5103d80a7a374d2316a7ce262e57baf8f208
  - name: git2
    version: 0.14.4
    sha: d0155506aab710a86160ddb504a480d2964d7ab5b9e62419be69e0032bc5931c
  - name: git2-curl
    version: 0.15.0
    sha: 1ee51709364c341fbb6fe2a385a290fb9196753bdde2fc45447d27cd31b11b13
  - name: glob
    version: 0.3.0
    sha: 9b919933a397b79c37e33b77bb2aa3dc8eb6e165ad809e58ff75bc7db2e34574
  - name: globset
    version: 0.4.9
    sha: 0a1e17342619edbc21a964c2afbeb6c820c6a2560032872f397bb97ea127bd0a
  - name: hashbrown
    version: 0.12.3
    sha: 8a9ee70c43aaf417c914396645a0fa852624801b24ebb7ae78fe8272889ac888
  - name: heck
    version: 0.4.0
    sha: 2540771e65fc8cb83cd6e8a237f70c319bd5c29f78ed1084ba5d50eeac86f7f9
  - name: hermit-abi
    version: 0.1.19
    sha: 62b467343b94ba476dcb2500d242dadbb39557df889310ac77c5d99100aaac33
  - name: hex
    version: 0.3.2
    sha: 805026a5d0141ffc30abb3be3173848ad46a1b1664fe632428479619a3644d77
  - name: hex
    version: 0.4.3
    sha: 7f24254aa9a54b5c858eaee2f5bccdb46aaf0e486a595ed5fd8f86ba55232a70
  - name: home
    version: 0.5.3
    sha: 2456aef2e6b6a9784192ae780c0f15bc57df0e918585282325e8c8ac27737654
  - name: humantime
    version: 2.1.0
    sha: 9a3a5bfb195931eeb336b2a7b4d761daec841b97f947d34394601737a7bba5e4
  - name: idna
    version: 0.2.3
    sha: 418a0a6fab821475f634efe3ccc45c013f742efe03d853e8d3355d5cb850ecf8
  - name: ignore
    version: 0.4.18
    sha: 713f1b139373f96a2e0ce3ac931cd01ee973c3c5dd7c40c0c2efe96ad2b6751d
  - name: im-rc
    version: 15.1.0
    sha: af1955a75fa080c677d3972822ec4bad316169ab1cfc6c257a942c2265dbe5fe
  - name: indexmap
    version: 1.9.1
    sha: 10a35a97730320ffe8e2d410b5d3b69279b98d2c14bdb8b70ea89ecf7888d41e
  - name: instant
    version: 0.1.12
    sha: 7a5bbe824c507c5da5956355e86a746d82e0e1464f65d862cc5e71da70e94b2c
  - name: itertools
    version: 0.10.3
    sha: a9a9d19fa1e79b6215ff29b9d6880b706147f16e9b1dbb1e4e5947b5b02bc5e3
  - name: itoa
    version: 1.0.3
    sha: 6c8af84674fe1f223a982c933a0ee1086ac4d4052aa0fb8060c12c6ad838e754
  - name: jobserver
    version: 0.1.24
    sha: af25a77299a7f711a01975c35a6a424eb6862092cc2d6c72c4ed6cbc56dfc1fa
  - name: kstring
    version: 2.0.0
    sha: ec3066350882a1cd6d950d055997f379ac37fd39f81cd4d8ed186032eb3c5747
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: lazycell
    version: 1.3.0
    sha: 830d08ce1d1d941e6b30645f1a0eb5643013d835ce3779a5fc208261dbe10f55
  - name: libc
    version: 0.2.131
    sha: 04c3b4822ccebfa39c02fc03d1534441b22ead323fa0f48bb7ddd8e6ba076a40
  - name: libgit2-sys
    version: 0.13.4+1.4.2
    sha: d0fa6563431ede25f5cc7f6d803c6afbc1c5d3ad3d4925d12c882bf2b526f5d1
  - name: libnghttp2-sys
    version: 0.1.7+1.45.0
    sha: 57ed28aba195b38d5ff02b9170cbff627e336a20925e43b4945390401c5dc93f
  - name: libssh2-sys
    version: 0.2.23
    sha: b094a36eb4b8b8c8a7b4b8ae43b2944502be3e59cd87687595cf6b0a71b3f4ca
  - name: libz-sys
    version: 1.1.8
    sha: 9702761c3935f8cc2f101793272e202c72b99da8f4224a19ddcf1279a6450bbf
  - name: log
    version: 0.4.17
    sha: abb12e687cfb44aa40f41fc3978ef76448f9b6038cad6aef4259d3c095a2382e
  - name: matches
    version: 0.1.9
    sha: a3e378b66a060d48947b590737b30a1be76706c8dd7b8ba0f2fe3989c68a853f
  - name: memchr
    version: 2.5.0
    sha: 2dffe52ecf27772e601905b7522cb4ef790d2cc203488bbd0e2fe85fcb74566d
  - name: miniz_oxide
    version: 0.5.3
    sha: 6f5c75688da582b8ffc1f1799e9db273f32133c49e048f614d22ec3256773ccc
  - name: miow
    version: 0.3.7
    sha: b9f1c5b025cda876f66ef43a113f91ebc9f4ccef34843000e0adf6ebbab84e21
  - name: num_cpus
    version: 1.13.1
    sha: 19e64526ebdee182341572e50e9ad03965aa510cd94427a4549448f285e957a1
  - name: once_cell
    version: 1.13.0
    sha: 18a6dbe30758c9f83eb00cbea4ac95966305f5a7772f3f42ebfc7fc7eddbd8e1
  - name: opener
    version: 0.5.0
    sha: 4ea3ebcd72a54701f56345f16785a6d3ac2df7e986d273eb4395c0b01db17952
  - name: openssl
    version: 0.10.41
    sha: 618febf65336490dfcf20b73f885f5651a0c89c64c2d4a8c3662585a70bf5bd0
  - name: openssl-macros
    version: 0.1.0
    sha: b501e44f11665960c7e7fcf062c7d96a14ade4aa98116c004b2e37b5be7d736c
  - name: openssl-probe
    version: 0.1.5
    sha: ff011a302c396a5197692431fc1948019154afc178baf7d8e37367442a4601cf
  - name: openssl-src
    version: 111.22.0+1.1.1q
    sha: 8f31f0d509d1c1ae9cada2f9539ff8f37933831fd5098879e482aa687d659853
  - name: openssl-sys
    version: 0.9.75
    sha: e5f9bd0c2710541a3cda73d6f9ac4f1b240de4ae261065d309dbe73d9dceb42f
  - name: os_info
    version: 3.5.0
    sha: 5209b2162b2c140df493a93689e04f8deab3a67634f5bc7a553c0a98e5b8d399
  - name: os_str_bytes
    version: 6.3.0
    sha: 9ff7415e9ae3fff1225851df9e0d9e4e5479f947619774677a63572e55e80eff
  - name: pathdiff
    version: 0.2.1
    sha: 8835116a5c179084a830efb3adc117ab007512b535bc1a21c991d3b32a6b44dd
  - name: percent-encoding
    version: 2.1.0
    sha: d4fd5641d01c8f18a23da7b6fe29298ff4b55afcccdf78973b24cf3175fee32e
  - name: pkg-config
    version: 0.3.25
    sha: 1df8c4ec4b0627e53bdf214615ad287367e482558cf84b109250b37464dc03ae
  - name: proc-macro-error
    version: 1.0.4
    sha: da25490ff9892aab3fcf7c36f08cfb902dd3e71ca0f9f9517bea02a73a5ce38c
  - name: proc-macro-error-attr
    version: 1.0.4
    sha: a1be40180e52ecc98ad80b184934baf3d0d29f979574e439af5a55274b35f869
  - name: proc-macro2
    version: 1.0.43
    sha: 0a2ca2c61bc9f3d74d2886294ab7b9853abd9c1ad903a3ac7815c58989bb7bab
  - name: quote
    version: 1.0.21
    sha: bbe448f377a7d6961e30f5955f9b8d106c3f5e449d493ee1b125c1d43c2b5179
  - name: rand_core
    version: 0.6.3
    sha: d34f1408f55294453790c48b2f1ebbb1c5b4b7563eb1f418bcfcfdbb06ebb4e7
  - name: rand_xoshiro
    version: 0.6.0
    sha: 6f97cdb2a36ed4183de61b2f824cc45c9f1037f28afe0a322e9fff4c108b5aaa
  - name: redox_syscall
    version: 0.2.16
    sha: fb5a58c1855b4b6819d59012155603f0b22ad30cad752600aadfcb695265519a
  - name: regex
    version: 1.6.0
    sha: 4c4eb3267174b8c6c2f654116623910a0fef09c4753f8dd83db29c48a0df988b
  - name: regex-automata
    version: 0.1.10
    sha: 6c230d73fb8d8c1b9c0b3135c5142a8acee3a0558fb8db5cf1cb65f8d7862132
  - name: regex-syntax
    version: 0.6.27
    sha: a3f87b73ce11b1619a3c6332f45341e0047173771e8b8b73f87bfeefb7b56244
  - name: remove_dir_all
    version: 0.5.3
    sha: 3acd125665422973a33ac9d3dd2df85edad0f4ae9b00dafb1a05e43a9f5ef8e7
  - name: rustc-workspace-hack
    version: 1.0.0
    sha: fc71d2faa173b74b232dedc235e3ee1696581bb132fc116fa3626d6151a1a8fb
  - name: rustfix
    version: 0.6.1
    sha: ecd2853d9e26988467753bd9912c3a126f642d05d229a4b53f5752ee36c56481
  - name: ryu
    version: 1.0.11
    sha: 4501abdff3ae82a1c1b477a17252eb69cee9e66eb915c1abaa4f44d873df9f09
  - name: same-file
    version: 1.0.6
    sha: 93fc1dc3aaa9bfed95e02e6eadabb4baf7e3078b0bd1b4d7b6b0b68378900502
  - name: schannel
    version: 0.1.20
    sha: 88d6731146462ea25d9244b2ed5fd1d716d25c52e4d54aa4fb0f3c4e9854dbe2
  - name: semver
    version: 1.0.13
    sha: 93f6841e709003d68bb2deee8c343572bf446003ec20a583e76f7b15cebf3711
  - name: serde
    version: 1.0.143
    sha: 53e8e5d5b70924f74ff5c6d64d9a5acd91422117c60f48c4e07855238a254553
  - name: serde_derive
    version: 1.0.143
    sha: d3d8e8de557aee63c26b85b947f5e59b690d0454c753f3adeb5cd7835ab88391
  - name: serde_ignored
    version: 0.1.5
    sha: 82b3da7eedd967647a866f67829d1c79d184d7c4521126e9cc2c46a9585c6d21
  - name: serde_json
    version: 1.0.83
    sha: 38dd04e3c8279e75b31ef29dbdceebfe5ad89f4d0937213c53f7d49d01b3d5a7
  - name: shell-escape
    version: 0.1.5
    sha: 45bb67a18fa91266cc7807181f62f9178a6873bfad7dc788c42e6430db40184f
  - name: sized-chunks
    version: 0.6.5
    sha: 16d69225bde7a69b235da73377861095455d298f2b970996eec25ddbb42b3d1e
  - name: socket2
    version: 0.4.4
    sha: 66d72b759436ae32898a2af0a14218dbf55efde3feeb170eb623637db85ee1e0
  - name: static_assertions
    version: 1.1.0
    sha: a2eb9349b6444b326872e140eb1cf5e7c522154d69e7a0ffb0fb81c06b37543f
  - name: strip-ansi-escapes
    version: 0.1.1
    sha: 011cbb39cf7c1f62871aea3cc46e5817b0937b49e9447370c93cacbe93a766d8
  - name: strsim
    version: 0.10.0
    sha: 73473c0e59e6d5812c5dfe2a064a6444949f089e20eec9a2e5506596494e4623
  - name: syn
    version: 1.0.99
    sha: 58dbef6ec655055e20b86b15a8cc6d439cca19b667537ac6a1369572d151ab13
  - name: tar
    version: 0.4.38
    sha: 4b55807c0344e1e6c04d7c965f5289c39a8d94ae23ed5c0b57aabac549f871c6
  - name: tempfile
    version: 3.3.0
    sha: 5cdb1ef4eaeeaddc8fbd371e5017057064af0911902ef36b39801f67cc6d79e4
  - name: termcolor
    version: 1.1.3
    sha: bab24d30b911b2376f3a13cc2cd443142f0c81dda04c118693e35b3835757755
  - name: textwrap
    version: 0.15.0
    sha: b1141d4d61095b28419e22cb0bbf02755f5e54e0526f97f1e3d1d160e60885fb
  - name: thread_local
    version: 1.1.4
    sha: 5516c27b78311c50bf42c071425c560ac799b11c30b31f87e3081965fe5e0180
  - name: tinyvec
    version: 1.6.0
    sha: 87cc5ceb3875bb20c2890005a4e226a4651264a5c75edb2421b52861a0a0cb50
  - name: tinyvec_macros
    version: 0.1.0
    sha: cda74da7e1a664f795bb1f8a87ec406fb89a02522cf6e50620d016add6dbbf5c
  - name: toml
    version: 0.5.9
    sha: 8d82e1a7758622a465f8cee077614c73484dac5b836c02ff6a40d5d1010324d7
  - name: toml_edit
    version: 0.14.4
    sha: 5376256e44f2443f8896ac012507c19a012df0fe8758b55246ae51a2279db51f
  - name: typenum
    version: 1.15.0
    sha: dcf81ac59edc17cc8697ff311e8f5ef2d99fcbd9817b34cec66f90b6c3dfd987
  - name: unicode-bidi
    version: 0.3.8
    sha: 099b7128301d285f79ddd55b9a83d5e6b9e97c92e0ea0daebee7263e932de992
  - name: unicode-ident
    version: 1.0.3
    sha: c4f5b37a154999a8f3f98cc23a628d850e154479cd94decf3414696e12e31aaf
  - name: unicode-normalization
    version: 0.1.21
    sha: 854cbdc4f7bc6ae19c820d44abdc3277ac3e1b2b93db20a636825d9322fb60e6
  - name: unicode-width
    version: 0.1.9
    sha: 3ed742d4ea2bd1176e236172c8429aaf54486e7ac098db29ffe6529e0ce50973
  - name: unicode-xid
    version: 0.2.3
    sha: 957e51f3646910546462e67d5f7599b9e4fb8acdd304b087a6494730f9eebf04
  - name: url
    version: 2.2.2
    sha: a507c383b2d33b5fc35d1861e77e6b383d158b2da5e14fe51b83dfedf6fd578c
  - name: utf8parse
    version: 0.2.0
    sha: 936e4b492acfd135421d8dca4b1aa80a7bfc26e702ef3af710e0752684df5372
  - name: vcpkg
    version: 0.2.15
    sha: accd4ea62f7bb7a82fe23066fb0957d48ef677f6eeb8215f372f52e48bb32426
  - name: version_check
    version: 0.9.4
    sha: 49874b5167b65d7193b8aba1567f5c7d93d001cafc34600cee003eda787e483f
  - name: vte
    version: 0.10.1
    sha: 6cbce692ab4ca2f1f3047fcf732430249c0e971bfdd2b234cf2c47ad93af5983
  - name: vte_generate_state_changes
    version: 0.1.1
    sha: d257817081c7dffcdbab24b9e62d2def62e2ff7d00b1c20062551e6cccc145ff
  - name: walkdir
    version: 2.3.2
    sha: 808cf2735cd4b6866113f648b791c6adc5714537bc222d9347bb203386ffda56
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
  - name: windows_aarch64_msvc
    version: 0.36.1
    sha: 9bb8c3fd39ade2d67e9874ac4f3db21f0d710bee00fe7cab16949ec184eeaa47
  - name: windows_i686_gnu
    version: 0.36.1
    sha: 180e6ccf01daf4c426b846dfc66db1fc518f074baa793aa7d9b9aaeffad6a3b6
  - name: windows_i686_msvc
    version: 0.36.1
    sha: e2e7917148b2812d1eeafaeb22a97e4813dfa60a3f8f78ebe204bcc88f12f024
  - name: windows_x86_64_gnu
    version: 0.36.1
    sha: 4dcd171b8776c41b97521e5da127a2d86ad280114807d0b2ab1e462bc764d9e1
  - name: windows_x86_64_msvc
    version: 0.36.1
    sha: c811ca4a8c853ef420abd8592ba53ddbbac90410fab6903b3e79972a631f7680
