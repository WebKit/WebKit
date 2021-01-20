kind: manual

build-depends:
- freedesktop-sdk.bst:extensions/rust/rust.bst
- freedesktop-sdk.bst:components/pkg-config.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/dav1d.bst
- sdk/gstreamer.bst
- sdk/gst-plugins-base.bst

environment:
  PATH: /usr/bin:/usr/lib/sdk/rust/bin

config:
  install-commands:
  - |
    cargo --offline build --release
    install -D -m a+r -t "%{install-root}%{libdir}/gstreamer-1.0" ./target/release/libgst*

public:
  bst:
    integration-commands:
    - |
      gst-inspect-1.0 rsdav1d

sources:
- kind: tar
  url: https://static.crates.io/crates/gst-plugin-dav1d/gst-plugin-dav1d-0.6.0.crate
  ref: 9aa82892a641599130f52a6da517c486519f39099f64b351305157e79cccd65c
- kind: local
  path: files/gst-plugin-dav1d/Cargo.lock
- kind: cargo
  ref:
  - name: aho-corasick
    version: 0.7.13
    sha: 043164d8ba5c4c3035fec9bbee8647c0261d788f3474306f93bb65901cae0e86
  - name: ansi_term
    version: 0.11.0
    sha: ee49baf6cb617b853aa8d93bf420db2383fab46d314482ca2803b40d5fde979b
  - name: anyhow
    version: 1.0.32
    sha: 6b602bfe940d21c130f3895acd65221e8a61270debe89d628b9cb4e3ccb8569b
  - name: atty
    version: 0.2.14
    sha: d9b39be18770d11421cdb1b9947a45dd3f37e93092cbf377614828a319d5fee8
  - name: autocfg
    version: 1.0.1
    sha: cdb031dd78e28731d87d56cc8ffef4a8f36ca26c38fe2de700543e627f8a464a
  - name: bindgen
    version: 0.54.0
    sha: 66c0bb6167449588ff70803f4127f0684f9063097eca5016f37eb52b92c2cf36
  - name: bitflags
    version: 1.2.1
    sha: cf1de2fe8c75bc145a2f577add951f8134889b4795d47466a54a5c846d691693
  - name: cc
    version: 1.0.59
    sha: 66120af515773fb005778dc07c261bd201ec8ce50bd6e7144c927753fe013381
  - name: cexpr
    version: 0.4.0
    sha: f4aedb84272dbe89af497cf81375129abda4fc0a9e7c5d317498c15cc30c0d27
  - name: cfg-if
    version: 0.1.10
    sha: 4785bdd1c96b2a846b2bd7cc02e86b6b3dbf14e7e53446c4f54c92a361040822
  - name: chrono
    version: 0.4.15
    sha: 942f72db697d8767c22d46a598e01f2d3b475501ea43d0db4f16d90259182d0b
  - name: clang-sys
    version: 0.29.3
    sha: fe6837df1d5cba2397b835c8530f51723267e16abbf83892e9e5af4f0e5dd10a
  - name: clap
    version: 2.33.3
    sha: 37e58ac78573c40708d45522f0d80fa2f01cc4f9b4e2bf749807255454312002
  - name: dav1d
    version: 0.5.2
    sha: ea34e70b0e3ec4e2efd67339c29bd91b68e671d75c5039a2456e13ed00f0922d
  - name: dav1d-sys
    version: 0.3.2
    sha: 7d8e036230ca0dd8e43e9192e93e0ab5073bbf8be8747a4c938656d6a985bdc9
  - name: either
    version: 1.6.0
    sha: cd56b59865bce947ac5958779cfa508f6c3b9497cc762b7e24a12d11ccde2c4f
  - name: env_logger
    version: 0.7.1
    sha: 44533bbbb3bb3c1fa17d9f2e4e38bbbaf8396ba82193c4cb1b6445d711445d36
  - name: error-chain
    version: 0.10.0
    sha: d9435d864e017c3c6afeac1654189b06cdb491cf2ff73dbf0d73b0f292f42ff8
  - name: futures-channel
    version: 0.3.5
    sha: f366ad74c28cca6ba456d95e6422883cfb4b252a83bed929c83abfdbbf2967d5
  - name: futures-core
    version: 0.3.5
    sha: 59f5fff90fd5d971f936ad674802482ba441b6f09ba5e15fd8b39145582ca399
  - name: futures-executor
    version: 0.3.5
    sha: 10d6bb888be1153d3abeb9006b11b02cf5e9b209fda28693c31ae1e4e012e314
  - name: futures-macro
    version: 0.3.5
    sha: d0b5a30a4328ab5473878237c447333c093297bded83a4983d10f4deea240d39
  - name: futures-task
    version: 0.3.5
    sha: bdb66b5f09e22019b1ab0830f7785bcea8e7a42148683f99214f73f8ec21a626
  - name: futures-util
    version: 0.3.5
    sha: 8764574ff08b701a084482c3c7031349104b07ac897393010494beaa18ce32c6
  - name: glib
    version: 0.10.1
    sha: a5e0533f48640d86e8e2f3cee778a9f97588d4a0bec8be065ee51ea52346d6c1
  - name: glib-macros
    version: 0.10.1
    sha: 41486a26d1366a8032b160b59065a59fb528530a46a49f627e7048fb8c064039
  - name: glib-sys
    version: 0.10.0
    sha: b6cda4af5c2f4507b7a3535b798dca2135293f4bc3a17f399ce244ef15841c4c
  - name: glob
    version: 0.3.0
    sha: 9b919933a397b79c37e33b77bb2aa3dc8eb6e165ad809e58ff75bc7db2e34574
  - name: gobject-sys
    version: 0.10.0
    sha: 952133b60c318a62bf82ee75b93acc7e84028a093e06b9e27981c2b6fe68218c
  - name: gst-plugin-version-helper
    version: 0.2.0
    sha: cc81ae3a09f23761eb3b122f50e5daedea4b44fdf8242ec133bc87aa8e24f9b8
  - name: gstreamer
    version: 0.16.2
    sha: ce4ce1ba28d3293b8cb8c3d33f50e6da2e5cfeefa59a0d10d922ab8015791609
  - name: gstreamer-base
    version: 0.16.0
    sha: 872893487ce8876f18c63730402822804c5762869f631d8e3e6b18aafc8399f0
  - name: gstreamer-base-sys
    version: 0.9.0
    sha: c0fd5a77d39b47568fba01274dfcb28dc32382513c697009f80b89ef63fd32fd
  - name: gstreamer-sys
    version: 0.9.0
    sha: 1321f34d53bb5f60ab1aaf581e29b664b8d41601714ee1bb7dbea490b5b9ff60
  - name: gstreamer-video
    version: 0.16.0
    sha: 42a52c141d83113e6dd080347ca49ef9be296e7900e081f2b67eaad6a3d5ef26
  - name: gstreamer-video-sys
    version: 0.9.0
    sha: d6f105143a7676d2032c386c10b2d376106b5562b7a11b694b634113456f1935
  - name: heck
    version: 0.3.1
    sha: 20564e78d53d2bb135c343b3f47714a56af2061f1c928fdb541dc7b9fdd94205
  - name: hermit-abi
    version: 0.1.15
    sha: 3deed196b6e7f9e44a2ae8d94225d80302d81208b1bb673fd21fe634645c85a9
  - name: humantime
    version: 1.3.0
    sha: df004cfca50ef23c36850aaaa59ad52cc70d0e90243c3c7737a4dd32dc7a3c4f
  - name: itertools
    version: 0.9.0
    sha: 284f18f85651fe11e8a991b2adb42cb078325c996ed026d994719efcfca1d54b
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: lazycell
    version: 1.3.0
    sha: 830d08ce1d1d941e6b30645f1a0eb5643013d835ce3779a5fc208261dbe10f55
  - name: libc
    version: 0.2.76
    sha: 755456fae044e6fa1ebbbd1b3e902ae19e73097ed4ed87bb79934a867c007bc3
  - name: libloading
    version: 0.5.2
    sha: f2b111a074963af1d37a139918ac6d49ad1d0d5e47f72fd55388619691a7d753
  - name: log
    version: 0.4.11
    sha: 4fabed175da42fed1fa0746b0ea71f412aa9d35e76e95e59b192c64b9dc2bf8b
  - name: memchr
    version: 2.3.3
    sha: 3728d817d99e5ac407411fa471ff9800a778d88a24685968b36824eaf4bee400
  - name: metadeps
    version: 1.1.2
    sha: 73b122901b3a675fac8cecf68dcb2f0d3036193bc861d1ac0e1c337f7d5254c2
  - name: muldiv
    version: 0.2.1
    sha: 0419348c027fa7be448d2ae7ea0e4e04c2334c31dc4e74ab29f00a2a7ca69204
  - name: nom
    version: 5.1.2
    sha: ffb4262d26ed83a1c0a33a38fe2bb15797329c85770da05e6b828ddb782627af
  - name: num-integer
    version: 0.1.43
    sha: 8d59457e662d541ba17869cf51cf177c0b5f0cbf476c66bdc90bf1edac4f875b
  - name: num-rational
    version: 0.3.0
    sha: a5b4d7360f362cfb50dde8143501e6940b22f644be75a4cc90b2d81968908138
  - name: num-traits
    version: 0.2.12
    sha: ac267bcc07f48ee5f8935ab0d24f316fb722d7a1292e2913f0cc196b29ffd611
  - name: once_cell
    version: 1.4.1
    sha: 260e51e7efe62b592207e9e13a68e43692a7a279171d6ba57abd208bf23645ad
  - name: paste
    version: 0.1.18
    sha: 45ca20c77d80be666aef2b45486da86238fabe33e38306bd3118fe4af33fa880
  - name: paste-impl
    version: 0.1.18
    sha: d95a7db200b97ef370c8e6de0088252f7e0dfff7d047a28528e47456c0fc98b6
  - name: peeking_take_while
    version: 0.1.2
    sha: 19b17cddbe7ec3f8bc800887bab5e717348c95ea2ca0b1bf0837fb964dc67099
  - name: pin-project
    version: 0.4.23
    sha: ca4433fff2ae79342e497d9f8ee990d174071408f28f726d6d83af93e58e48aa
  - name: pin-project-internal
    version: 0.4.23
    sha: 2c0e815c3ee9a031fdf5af21c10aa17c573c9c6a566328d99e3936c34e36461f
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.18
    sha: d36492546b6af1463394d46f0c834346f31548646f6ba10849802c9c9a27ac33
  - name: pretty-hex
    version: 0.1.1
    sha: be91bcc43e73799dc46a6c194a55e7aae1d86cc867c860fd4a436019af21bd8c
  - name: proc-macro-crate
    version: 0.1.5
    sha: 1d6ea3c4595b96363c13943497db34af4460fb474a95c43f4446ad341b8c9785
  - name: proc-macro-error
    version: 1.0.4
    sha: da25490ff9892aab3fcf7c36f08cfb902dd3e71ca0f9f9517bea02a73a5ce38c
  - name: proc-macro-error-attr
    version: 1.0.4
    sha: a1be40180e52ecc98ad80b184934baf3d0d29f979574e439af5a55274b35f869
  - name: proc-macro-hack
    version: 0.5.18
    sha: 99c605b9a0adc77b7211c6b1f722dcb613d68d66859a44f3d485a6da332b0598
  - name: proc-macro-nested
    version: 0.1.6
    sha: eba180dafb9038b050a4c280019bbedf9f2467b61e5d892dcad585bb57aadc5a
  - name: proc-macro2
    version: 1.0.19
    sha: 04f5f085b5d71e2188cb8271e5da0161ad52c3f227a661a3c135fdf28e258b12
  - name: quick-error
    version: 1.2.3
    sha: a1d01941d82fa2ab50be1e79e6714289dd7cde78eba4c074bc5a4374f650dfe0
  - name: quote
    version: 1.0.7
    sha: aa563d17ecb180e500da1cfd2b028310ac758de548efdd203e18f283af693f37
  - name: regex
    version: 1.3.9
    sha: 9c3780fcf44b193bc4d09f36d2a3c87b251da4a046c87795a0d35f4f927ad8e6
  - name: regex-syntax
    version: 0.6.18
    sha: 26412eb97c6b088a6997e05f69403a802a92d520de2f8e63c2b65f9e0f47c4e8
  - name: rustc-hash
    version: 1.1.0
    sha: 08d43f7aa6b08d49f382cde6a7982047c3426db949b1424bc4b7ec9ae12c6ce2
  - name: serde
    version: 1.0.115
    sha: e54c9a88f2da7238af84b5101443f0c0d0a3bbdc455e34a5c9497b1903ed55d5
  - name: shlex
    version: 0.1.1
    sha: 7fdf1b9db47230893d76faad238fd6097fd6d6a9245cd7a4d90dbd639536bbd2
  - name: slab
    version: 0.4.2
    sha: c111b5bd5695e56cffe5129854aa230b39c93a305372fdbb2668ca2394eea9f8
  - name: strsim
    version: 0.8.0
    sha: 8ea5119cdb4c55b55d432abb513a0429384878c15dde60cc77b1c99de1a95a6a
  - name: strum
    version: 0.18.0
    sha: 57bd81eb48f4c437cadc685403cad539345bf703d78e63707418431cecd4522b
  - name: strum_macros
    version: 0.18.0
    sha: 87c85aa3f8ea653bfd3ddf25f7ee357ee4d204731f6aa9ad04002306f6e2774c
  - name: syn
    version: 1.0.39
    sha: 891d8d6567fe7c7f8835a3a98af4208f3846fba258c1bc3c31d6e506239f11f9
  - name: system-deps
    version: 1.3.2
    sha: 0f3ecc17269a19353b3558b313bba738b25d82993e30d62a18406a24aba4649b
  - name: termcolor
    version: 1.1.0
    sha: bb6bfa289a4d7c5766392812c0a1f4c1ba45afa1ad47803c11e1f407d846d75f
  - name: textwrap
    version: 0.11.0
    sha: d326610f408c7a4eb6f51c37c330e496b08506c9457c9d34287ecc38809fb060
  - name: thiserror
    version: 1.0.20
    sha: 7dfdd070ccd8ccb78f4ad66bf1982dc37f620ef696c6b5028fe2ed83dd3d0d08
  - name: thiserror-impl
    version: 1.0.20
    sha: bd80fc12f73063ac132ac92aceea36734f04a1d93c1240c6944e23a3b8841793
  - name: thread_local
    version: 1.0.1
    sha: d40c6d1b69745a6ec6fb1ca717914848da4b44ae29d9b3080cbee91d72a69b14
  - name: time
    version: 0.1.44
    sha: 6db9e6914ab8b1ae1c260a4ae7a49b6c5611b40328a735b21862567685e73255
  - name: toml
    version: 0.2.1
    sha: 736b60249cb25337bc196faa43ee12c705e426f3d55c214d73a4e7be06f92cb4
  - name: toml
    version: 0.5.6
    sha: ffc92d160b1eef40665be3a05630d003936a3bc7da7421277846c2613e92c71a
  - name: unicode-segmentation
    version: 1.6.0
    sha: e83e153d1053cbb5a118eeff7fd5be06ed99153f00dbcd8ae310c5fb2b22edc0
  - name: unicode-width
    version: 0.1.8
    sha: 9337591893a19b88d8d87f2cec1e73fad5cdfd10e5a6f349f498ad6ea2ffb1e3
  - name: unicode-xid
    version: 0.2.1
    sha: f7fe0bb3479651439c9112f72b6c505038574c9fbb575ed1bf3b797fa39dd564
  - name: vec_map
    version: 0.8.2
    sha: f1bddf1187be692e79c5ffeab891132dfb0f236ed36a43c7ed39f1165ee20191
  - name: version-compare
    version: 0.0.10
    sha: d63556a25bae6ea31b52e640d7c41d1ab27faba4ccb600013837a3d0b3994ca1
  - name: version_check
    version: 0.9.2
    sha: b5a972e5669d67ba988ce3dc826706fb0a8b01471c088cb0b6110b805cc36aed
  - name: wasi
    version: 0.10.0+wasi-snapshot-preview1
    sha: 1a143597ca7c7793eff794def352d41792a93c481eb1042423ff7ff72ba2c31f
  - name: which
    version: 3.1.1
    sha: d011071ae14a2f6671d0b74080ae0cd8ebf3a6f8c9589a2cd45f23126fe29724
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
