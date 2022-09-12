kind: manual

build-depends:
- freedesktop-sdk.bst:components/rust.bst
- freedesktop-sdk.bst:components/pkg-config.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/dav1d.bst
- sdk/gstreamer.bst
- sdk/gst-plugins-base.bst

environment:
  PATH: /usr/bin:/usr/lib/sdk/rust/bin

config:
  build-commands:
  - |
    cargo --offline build --release

  install-commands:
  - |
    install -D -m a+r -t "%{install-root}%{libdir}/gstreamer-1.0" ./target/release/libgst*

public:
  bst:
    integration-commands:
    - |
      PAGER=this-is-not-a-pager gst-inspect-1.0 rsdav1d

sources:
- kind: tar
  url: https://static.crates.io/crates/gst-plugin-dav1d/gst-plugin-dav1d-0.8.2.crate
  ref: 3e3e08dda28832b180539967966ed262c55b62a3809b85cdf611e0d3ca36ba55
- kind: local
  path: files/gst-plugin-dav1d/Cargo.lock
- kind: patch
  path: patches/gst-plugin-dav1d-fix-build-for-dav1d-1-0.diff
- kind: cargo
  ref:
  - name: aho-corasick
    version: 0.7.18
    sha: 1e37cfd5e7657ada45f742d6e99ca5788580b5c529dc78faf11ece6dc702656f
  - name: android_system_properties
    version: 0.1.5
    sha: 819e7219dbd41043ac279b19830f2efc897156490d7fd6ea916720117ee66311
  - name: ansi_term
    version: 0.12.1
    sha: d52a9bb7ec0cf484c551830a7ce27bd20d67eac647e1befb56b0be4ee39a55d2
  - name: anyhow
    version: 1.0.63
    sha: a26fa4d7e3f2eebadf743988fc8aec9fa9a9e82611acafd77c1462ed6262440a
  - name: atomic_refcell
    version: 0.1.8
    sha: 73b5e5f48b927f04e952dedc932f31995a65a0bf65ec971c74436e51bf6e970d
  - name: atty
    version: 0.2.14
    sha: d9b39be18770d11421cdb1b9947a45dd3f37e93092cbf377614828a319d5fee8
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: bindgen
    version: 0.59.2
    sha: 2bd2a9a458e8f4304c52c43ebb0cfbd520289f8379a52e329a38afda99bf8eb8
  - name: bitflags
    version: 1.3.2
    sha: bef38d45163c2f1dde094a7dfd33ccf595c92905c8f8f4fdc18d06fb1037718a
  - name: bumpalo
    version: 3.11.0
    sha: c1ad822118d20d2c234f427000d5acc36eabe1e29a348c89b63dd60b13f28e5d
  - name: cexpr
    version: 0.6.0
    sha: 6fac387a98bb7c37292057cffc56d62ecb629900026402633ae9160df93a8766
  - name: cfg-expr
    version: 0.10.3
    sha: 0aacacf4d96c24b2ad6eb8ee6df040e4f27b0d0b39a5710c30091baa830485db
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.22
    sha: bfd4d1b31faaa3a89d7934dbded3111da0d2ef28e3ebccdb4f0179f5929d1ef1
  - name: clang-sys
    version: 1.3.3
    sha: 5a050e2153c5be08febd6734e29298e844fdb0fa21aeddd63b4eb7baa106c69b
  - name: clap
    version: 2.34.0
    sha: a0610544180c38b88101fecf2dd634b174a62eef6946f84dfc6a7127512b381c
  - name: core-foundation-sys
    version: 0.8.3
    sha: 5827cebf4670468b8772dd191856768aedcb1b0278a04f989f7766351917b9dc
  - name: dav1d
    version: 0.8.0
    sha: d9669fd63fb7d46c2faadff42a0d168819735a2be83903e7973f09721ed42a8a
  - name: dav1d-sys
    version: 0.5.0
    sha: 237e2b43eb9a661b07147ec8dd483e7fe337977791d32c69c82f9a0e6c3be501
  - name: either
    version: 1.8.0
    sha: 90e5c1c8368803113bf0c9584fc495a58b86dc8a29edbf8fe877d21d9507e797
  - name: env_logger
    version: 0.9.0
    sha: 0b2cf0344971ee6c64c31be0d530793fba457d322dfec2810c453d0ef228f9c3
  - name: futures-channel
    version: 0.3.24
    sha: 30bdd20c28fadd505d0fd6712cdfcb0d4b5648baf45faef7f852afb2399bb050
  - name: futures-core
    version: 0.3.24
    sha: 4e5aa3de05362c3fb88de6531e6296e85cde7739cccad4b9dfeeb7f6ebce56bf
  - name: futures-executor
    version: 0.3.24
    sha: 9ff63c23854bee61b6e9cd331d523909f238fc7636290b96826e9cfa5faa00ab
  - name: futures-task
    version: 0.3.24
    sha: a6508c467c73851293f390476d4491cf4d227dbabcd4170f3bb6044959b294f1
  - name: futures-util
    version: 0.3.24
    sha: 44fb6cb1be61cc1d2e43b262516aafcf63b241cffdb1d3fa115f91d9c7b09c90
  - name: glib
    version: 0.15.12
    sha: edb0306fbad0ab5428b0ca674a23893db909a98582969c9b537be4ced78c505d
  - name: glib-macros
    version: 0.15.11
    sha: 25a68131a662b04931e71891fb14aaf65ee4b44d08e8abc10f49e77418c86c64
  - name: glib-sys
    version: 0.15.10
    sha: ef4b192f8e65e9cf76cbf4ea71fa8e3be4a0e18ffe3d68b8da6836974cc5bad4
  - name: glob
    version: 0.3.0
    sha: 9b919933a397b79c37e33b77bb2aa3dc8eb6e165ad809e58ff75bc7db2e34574
  - name: gobject-sys
    version: 0.15.10
    sha: 0d57ce44246becd17153bd035ab4d32cfee096a657fc01f2231c9278378d1e0a
  - name: gst-plugin-version-helper
    version: 0.7.3
    sha: 3a6a4dd1cb931cc6b49af354a68f21b3aee46b5b07370215d942f3a71542123f
  - name: gstreamer
    version: 0.18.8
    sha: d66363bacf5e4f6eb281564adc2902e44c52ae5c45082423e7439e9012b75456
  - name: gstreamer-base
    version: 0.18.0
    sha: 224f35f36582407caf58ded74854526beeecc23d0cf64b8d1c3e00584ed6863f
  - name: gstreamer-base-sys
    version: 0.18.0
    sha: a083493c3c340e71fa7c66eebda016e9fafc03eb1b4804cf9b2bad61994b078e
  - name: gstreamer-sys
    version: 0.18.0
    sha: e3517a65d3c2e6f8905b456eba5d53bda158d664863aef960b44f651cb7d33e2
  - name: gstreamer-video
    version: 0.18.7
    sha: 9418adfc72dafa1ad9eb106527ce4804887d101027c4528ec28c7d29cc899519
  - name: gstreamer-video-sys
    version: 0.18.3
    sha: 33331b1675e73b5b000c796354278eca7fdde9327015971d9f41afe28b96e0dc
  - name: heck
    version: 0.4.0
    sha: 2540771e65fc8cb83cd6e8a237f70c319bd5c29f78ed1084ba5d50eeac86f7f9
  - name: hermit-abi
    version: 0.1.19
    sha: 62b467343b94ba476dcb2500d242dadbb39557df889310ac77c5d99100aaac33
  - name: humantime
    version: 2.1.0
    sha: 9a3a5bfb195931eeb336b2a7b4d761daec841b97f947d34394601737a7bba5e4
  - name: iana-time-zone
    version: 0.1.47
    sha: 4c495f162af0bf17656d0014a0eded5f3cd2f365fdd204548c2869db89359dc7
  - name: js-sys
    version: 0.3.59
    sha: 258451ab10b34f8af53416d1fdab72c22e805f0c92a1136d59470ec0b11138b2
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: lazycell
    version: 1.3.0
    sha: 830d08ce1d1d941e6b30645f1a0eb5643013d835ce3779a5fc208261dbe10f55
  - name: libc
    version: 0.2.132
    sha: 8371e4e5341c3a96db127eb2465ac681ced4c433e01dd0e938adbef26ba93ba5
  - name: libloading
    version: 0.7.3
    sha: efbc0f03f9a775e9f6aed295c6a1ba2253c5757a9e03d55c6caa46a681abcddd
  - name: log
    version: 0.4.17
    sha: abb12e687cfb44aa40f41fc3978ef76448f9b6038cad6aef4259d3c095a2382e
  - name: memchr
    version: 2.5.0
    sha: 2dffe52ecf27772e601905b7522cb4ef790d2cc203488bbd0e2fe85fcb74566d
  - name: minimal-lexical
    version: 0.2.1
    sha: 68354c5c6bd36d73ff3feceb05efa59b6acb7626617f4962be322a825e61f79a
  - name: muldiv
    version: 1.0.0
    sha: b5136edda114182728ccdedb9f5eda882781f35fa6e80cc360af12a8932507f3
  - name: nom
    version: 7.1.1
    sha: a8903e5a29a317527874d0402f867152a3d21c908bb0b933e416c65e301d4c36
  - name: num-integer
    version: 0.1.45
    sha: 225d3389fb3509a24c93f5c29eb6bde2586b98d9f016636dff58d7c6f7569cd9
  - name: num-rational
    version: 0.4.1
    sha: 0638a1c9d0a3c0914158145bc76cff373a75a627e6ecbfb71cbe6f453a5a19b0
  - name: num-traits
    version: 0.2.15
    sha: 578ede34cf02f8924ab9447f50c28075b4d3e5b269972345e7e0372b38c6cdcd
  - name: once_cell
    version: 1.14.0
    sha: 2f7254b99e31cad77da24b08ebf628882739a608578bb1bcdfc1f9c21260d7c0
  - name: option-operations
    version: 0.4.1
    sha: 42b01597916c91a493b1e8a2fde64fec1764be3259abc1f06efc99c274f150a2
  - name: paste
    version: 1.0.9
    sha: b1de2e551fb905ac83f73f7aedf2f0cb4a0da7e35efa24a202a936269f1f18e1
  - name: peeking_take_while
    version: 0.1.2
    sha: 19b17cddbe7ec3f8bc800887bab5e717348c95ea2ca0b1bf0837fb964dc67099
  - name: pin-project-lite
    version: 0.2.9
    sha: e0a7ae3ac2f1173085d398531c705756c94a4c56843785df85a60c1a0afac116
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.25
    sha: 1df8c4ec4b0627e53bdf214615ad287367e482558cf84b109250b37464dc03ae
  - name: pretty-hex
    version: 0.3.0
    sha: c6fa0831dd7cc608c38a5e323422a0077678fa5744aa2be4ad91c4ece8eec8d5
  - name: proc-macro-crate
    version: 1.2.1
    sha: eda0fc3b0fb7c975631757e14d9049da17374063edb6ebbcbc54d880d4fe94e9
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
  - name: regex
    version: 1.6.0
    sha: 4c4eb3267174b8c6c2f654116623910a0fef09c4753f8dd83db29c48a0df988b
  - name: regex-syntax
    version: 0.6.27
    sha: a3f87b73ce11b1619a3c6332f45341e0047173771e8b8b73f87bfeefb7b56244
  - name: rustc-hash
    version: 1.1.0
    sha: 08d43f7aa6b08d49f382cde6a7982047c3426db949b1424bc4b7ec9ae12c6ce2
  - name: serde
    version: 1.0.144
    sha: 0f747710de3dcd43b88c9168773254e809d8ddbdf9653b84e2554ab219f17860
  - name: shlex
    version: 1.1.0
    sha: 43b2853a4d09f215c24cc5489c992ce46052d359b5109343cbafbf26bc62f8a3
  - name: slab
    version: 0.4.7
    sha: 4614a76b2a8be0058caa9dbbaf66d988527d86d003c11a94fbd335d7661edcef
  - name: smallvec
    version: 1.9.0
    sha: 2fd0db749597d91ff862fd1d55ea87f7855a744a8425a64695b6fca237d1dad1
  - name: strsim
    version: 0.8.0
    sha: 8ea5119cdb4c55b55d432abb513a0429384878c15dde60cc77b1c99de1a95a6a
  - name: syn
    version: 1.0.99
    sha: 58dbef6ec655055e20b86b15a8cc6d439cca19b667537ac6a1369572d151ab13
  - name: system-deps
    version: 6.0.2
    sha: a1a45a1c4c9015217e12347f2a411b57ce2c4fc543913b14b6fe40483328e709
  - name: termcolor
    version: 1.1.3
    sha: bab24d30b911b2376f3a13cc2cd443142f0c81dda04c118693e35b3835757755
  - name: textwrap
    version: 0.11.0
    sha: d326610f408c7a4eb6f51c37c330e496b08506c9457c9d34287ecc38809fb060
  - name: thiserror
    version: 1.0.33
    sha: 3d0a539a918745651435ac7db7a18761589a94cd7e94cd56999f828bf73c8a57
  - name: thiserror-impl
    version: 1.0.33
    sha: c251e90f708e16c49a16f4917dc2131e75222b72edfa9cb7f7c58ae56aae0c09
  - name: toml
    version: 0.5.9
    sha: 8d82e1a7758622a465f8cee077614c73484dac5b836c02ff6a40d5d1010324d7
  - name: unicode-ident
    version: 1.0.3
    sha: c4f5b37a154999a8f3f98cc23a628d850e154479cd94decf3414696e12e31aaf
  - name: unicode-width
    version: 0.1.9
    sha: 3ed742d4ea2bd1176e236172c8429aaf54486e7ac098db29ffe6529e0ce50973
  - name: vec_map
    version: 0.8.2
    sha: f1bddf1187be692e79c5ffeab891132dfb0f236ed36a43c7ed39f1165ee20191
  - name: version-compare
    version: 0.1.0
    sha: fe88247b92c1df6b6de80ddc290f3976dbdf2f5f5d3fd049a9fb598c6dd5ca73
  - name: version_check
    version: 0.9.4
    sha: 49874b5167b65d7193b8aba1567f5c7d93d001cafc34600cee003eda787e483f
  - name: wasm-bindgen
    version: 0.2.82
    sha: fc7652e3f6c4706c8d9cd54832c4a4ccb9b5336e2c3bd154d5cccfbf1c1f5f7d
  - name: wasm-bindgen-backend
    version: 0.2.82
    sha: 662cd44805586bd52971b9586b1df85cdbbd9112e4ef4d8f41559c334dc6ac3f
  - name: wasm-bindgen-macro
    version: 0.2.82
    sha: b260f13d3012071dfb1512849c033b1925038373aea48ced3012c09df952c602
  - name: wasm-bindgen-macro-support
    version: 0.2.82
    sha: 5be8e654bdd9b79216c2929ab90721aa82faf65c48cdf08bdc4e7f51357b80da
  - name: wasm-bindgen-shared
    version: 0.2.82
    sha: 6598dd0bd3c7d51095ff6531a5b23e02acdc81804e30d8f07afb77b7215a140a
  - name: which
    version: 4.3.0
    sha: 1c831fbbee9e129a8cf93e7747a82da9d95ba8e16621cae60ec2cdc849bacb7b
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
