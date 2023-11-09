kind: manual

build-depends:
- freedesktop-sdk.bst:components/rust.bst
- freedesktop-sdk.bst:components/pkg-config.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/gstreamer.bst
- freedesktop-sdk.bst:components/gstreamer-plugins-base.bst

environment:
  PATH: /usr/bin:/usr/lib/sdk/rust/bin

config:
  build-commands:
  - |
    cargo --offline build --release

  install-commands:
  - |
    install -D -m a+r -t "%{install-root}%{libdir}/gstreamer-1.0" ./target/release/libgst*.so

public:
  bst:
    integration-commands:
    - |
      PAGER=this-is-not-a-pager gst-inspect-1.0 cea608tott

sources:
- kind: tar
  url: crates:crates/gst-plugin-closedcaption/gst-plugin-closedcaption-0.10.6.crate
  ref: 81c439b8b1f91680cc32a9c152c8df2335aaacc33c7f6d457d7c62660ebf4bb4
- kind: local
  path: files/gst-plugin-closedcaption/Cargo.lock
- kind: cargo
  ref:
  - name: android_system_properties
    version: 0.1.5
    sha: 819e7219dbd41043ac279b19830f2efc897156490d7fd6ea916720117ee66311
  - name: anyhow
    version: 1.0.70
    sha: 7de8ce5e0f9f8d88245311066a578d72b7af3e7088f32783804676302df237e4
  - name: atomic_refcell
    version: 0.1.10
    sha: 79d6dc922a2792b006573f60b2648076355daeae5ce9cb59507e5908c9625d31
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: bitflags
    version: 1.3.2
    sha: bef38d45163c2f1dde094a7dfd33ccf595c92905c8f8f4fdc18d06fb1037718a
  - name: bumpalo
    version: 3.12.1
    sha: 9b1ce199063694f33ffb7dd4e0ee620741495c32833cde5aa08f02a0bf96f0c8
  - name: byteorder
    version: 1.4.3
    sha: 14c189c53d098945499cdfa7ecc63567cf3886b3332b312a5b4585d8d3a6a610
  - name: cairo-rs
    version: 0.17.0
    sha: a8af54f5d48af1226928adc1f57edd22f5df1349e7da1fc96ae15cf43db0e871
  - name: cairo-sys-rs
    version: 0.17.0
    sha: f55382a01d30e5e53f185eee269124f5e21ab526595b872751278dfbb463594e
  - name: cc
    version: 1.0.79
    sha: 50d30906286121d95be3d479533b458f87493b30a4b5f79a607db8f5d11aa91f
  - name: cfg-expr
    version: 0.15.1
    sha: c8790cf1286da485c72cf5fc7aeba308438800036ec67d89425924c4807268c9
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.24
    sha: 4e3c5919066adf22df73762e50cffcde3a758f2a848b113b586d1f86728b673b
  - name: codespan-reporting
    version: 0.11.1
    sha: 3538270d33cc669650c4b093848450d380def10c331d38c768e34cac80576e6e
  - name: core-foundation-sys
    version: 0.8.4
    sha: e496a50fda8aacccc86d7529e2c1e0892dbd0f898a6b5645b5561b89c3210efa
  - name: ctor
    version: 0.1.26
    sha: 6d2301688392eb071b0bf1a37be05c469d3cc4dbbd95df672fe28ab021e6a096
  - name: cxx
    version: 1.0.94
    sha: f61f1b6389c3fe1c316bf8a4dccc90a38208354b330925bce1f74a6c4756eb93
  - name: cxx-build
    version: 1.0.94
    sha: 12cee708e8962df2aeb38f594aae5d827c022b6460ac71a7a3e2c3c2aae5a07b
  - name: cxxbridge-flags
    version: 1.0.94
    sha: 7944172ae7e4068c533afbb984114a56c46e9ccddda550499caa222902c7f7bb
  - name: cxxbridge-macro
    version: 1.0.94
    sha: 2345488264226bf682893e25de0769f3360aac9957980ec49361b083ddaa5bc5
  - name: diff
    version: 0.1.13
    sha: 56254986775e3233ffa9c4d7d3faaf6d36a2c09d30b20687e9f88bc8bafc16c8
  - name: either
    version: 1.8.1
    sha: 7fcaabb2fef8c910e7f4c7ce9f67a1283a1715879a7c230ca9d6d1ae31f16d91
  - name: futures-channel
    version: 0.3.28
    sha: 955518d47e09b25bbebc7a18df10b81f0c766eaf4c4f1cccef2fca5f2a4fb5f2
  - name: futures-core
    version: 0.3.28
    sha: 4bca583b7e26f571124fe5b7561d49cb2868d79116cfa0eefce955557c6fee8c
  - name: futures-executor
    version: 0.3.28
    sha: ccecee823288125bd88b4d7f565c9e58e41858e47ab72e8ea2d64e93624386e0
  - name: futures-io
    version: 0.3.28
    sha: 4fff74096e71ed47f8e023204cfd0aa1289cd54ae5430a9523be060cdb849964
  - name: futures-macro
    version: 0.3.28
    sha: 89ca545a94061b6365f2c7355b4b32bd20df3ff95f02da9329b34ccc3bd6ee72
  - name: futures-task
    version: 0.3.28
    sha: 76d3d132be6c0e6aa1534069c705a74a5997a356c0dc2f86a47765e5617c5b65
  - name: futures-util
    version: 0.3.28
    sha: 26b01e40b772d54cf6c6d721c1d1abd0647a0106a12ecaa1c186273392a69533
  - name: getrandom
    version: 0.2.9
    sha: c85e1d9ab2eadba7e5040d4e09cbd6d072b76a557ad64e797c2cb9d4da21d7e4
  - name: gio
    version: 0.17.9
    sha: d14522e56c6bcb6f7a3aebc25cbcfb06776af4c0c25232b601b4383252d7cb92
  - name: gio-sys
    version: 0.17.4
    sha: 6b1d43b0d7968b48455244ecafe41192871257f5740aa6b095eb19db78e362a5
  - name: glib
    version: 0.17.9
    sha: a7f1de7cbde31ea4f0a919453a2dcece5d54d5b70e08f8ad254dc4840f5f09b6
  - name: glib-macros
    version: 0.17.9
    sha: 0a7206c5c03851ef126ea1444990e81fdd6765fb799d5bc694e4897ca01bb97f
  - name: glib-sys
    version: 0.17.4
    sha: 49f00ad0a1bf548e61adfff15d83430941d9e1bb620e334f779edd1c745680a5
  - name: gobject-sys
    version: 0.17.4
    sha: 15e75b0000a64632b2d8ca3cf856af9308e3a970844f6e9659bd197f026793d0
  - name: gst-plugin-version-helper
    version: 0.7.5
    sha: 87921209945e5dc809848a100115fad65bd127671896f0206f45e272080cc4c9
  - name: gstreamer
    version: 0.20.5
    sha: 4530401c89be6dc10d77ae1587b811cf455c97dce7abf594cb9164527c7da7fc
  - name: gstreamer-base
    version: 0.20.5
    sha: 0b8ff5dfbf7bcaf1466a385b836bad0d8da25759f121458727fdda1f771c69b3
  - name: gstreamer-base-sys
    version: 0.20.0
    sha: 26114ed96f6668380f5a1554128159e98e06c3a7a8460f216d7cd6dce28f928c
  - name: gstreamer-check
    version: 0.20.0
    sha: a60f8f0313cf6c35d54ba4dbad0634f99e290638e290f61500778b36cfa47f8a
  - name: gstreamer-check-sys
    version: 0.20.0
    sha: 650e930972d122ab4fe2c03685fec66bcd55845458a32de29dbd0114c05c8fcb
  - name: gstreamer-sys
    version: 0.20.0
    sha: e56fe047adef7d47dbafa8bc1340fddb53c325e16574763063702fc94b5786d2
  - name: gstreamer-video
    version: 0.20.4
    sha: dce97769effde2d779dc4f7037b37106457b74e53f2a711bddc90b30ffeb7e06
  - name: gstreamer-video-sys
    version: 0.20.0
    sha: 66ddb6112d438aac0004d2db6053a572f92b1c5e0e9d6ff6c71d9245f7f73e46
  - name: hashbrown
    version: 0.12.3
    sha: 8a9ee70c43aaf417c914396645a0fa852624801b24ebb7ae78fe8272889ac888
  - name: heck
    version: 0.4.1
    sha: 95505c38b4572b2d910cecb0281560f54b440a19336cbbcb27bf6ce6adc6f5a8
  - name: iana-time-zone
    version: 0.1.56
    sha: 0722cd7114b7de04316e7ea5456a0bbb20e4adb46fd27a3697adb812cff0f37c
  - name: iana-time-zone-haiku
    version: 0.1.1
    sha: 0703ae284fc167426161c2e3f1da3ea71d94b21bedbcc9494e92b28e334e3dca
  - name: indexmap
    version: 1.9.3
    sha: bd070e393353796e801d209ad339e89596eb4c8d430d18ede6a1cced8fafbd99
  - name: itoa
    version: 1.0.6
    sha: 453ad9f582a441959e5f0d088b02ce04cfe8d51a8eaf077f12ac6d3e94164ca6
  - name: js-sys
    version: 0.3.61
    sha: 445dde2150c55e483f3d8416706b97ec8e8237c307e5b7b4b8dd15e6af2a0730
  - name: libc
    version: 0.2.142
    sha: 6a987beff54b60ffa6d51982e1aa1146bc42f19bd26be28b0586f252fccf5317
  - name: link-cplusplus
    version: 1.0.8
    sha: ecd207c9c713c34f95a097a5b029ac2ce6010530c7b49d7fea24d977dede04f5
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
    version: 1.0.1
    sha: 956787520e75e9bd233246045d19f42fb73242759cc57fba9611d940ae96d4b0
  - name: nom
    version: 7.1.3
    sha: d273983c5a657a70a3e8f2a01329822f3b8c8172b73826411a55751e404a0a4a
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
    version: 1.17.1
    sha: b7e5500299e16ebb147ae15a00a942af264cf3688f47923b8fc2cd5858f23ad3
  - name: option-operations
    version: 0.5.0
    sha: 7c26d27bb1aeab65138e4bf7666045169d1717febcc9ff870166be8348b223d0
  - name: output_vt100
    version: 0.1.3
    sha: 628223faebab4e3e40667ee0b2336d34a5b960ff60ea743ddfdbcf7770bcfb66
  - name: pango
    version: 0.17.4
    sha: 52c280b82a881e4208afb3359a8e7fde27a1b272280981f1f34610bed5770d37
  - name: pango-sys
    version: 0.17.0
    sha: 4293d0f0b5525eb5c24734d30b0ed02cd02aa734f216883f376b54de49625de8
  - name: pangocairo
    version: 0.17.0
    sha: 2feeb7ea7874507f83f5e7ba869c54e321959431c8fbd70d4b735c8b15d90506
  - name: pangocairo-sys
    version: 0.17.3
    sha: f60f1be8ef08087ddcbdcc1350e06073bff11113d425d12b622b716d96b9611c
  - name: paste
    version: 1.0.12
    sha: 9f746c4065a8fa3fe23974dd82f15431cc8d40779821001404d10d2e79ca7d79
  - name: pin-project-lite
    version: 0.2.9
    sha: e0a7ae3ac2f1173085d398531c705756c94a4c56843785df85a60c1a0afac116
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.26
    sha: 6ac9a59f73473f1b8d852421e59e64809f025994837ef743615c6d0c5b305160
  - name: ppv-lite86
    version: 0.2.17
    sha: 5b40af805b3121feab8a3c29f04d8ad262fa8e0561883e7653e024ae4479e6de
  - name: pretty-hex
    version: 0.3.0
    sha: c6fa0831dd7cc608c38a5e323422a0077678fa5744aa2be4ad91c4ece8eec8d5
  - name: pretty_assertions
    version: 1.3.0
    sha: a25e9bcb20aa780fd0bb16b72403a9064d6b3f22f026946029acb941a50af755
  - name: proc-macro-crate
    version: 1.3.1
    sha: 7f4c021e1093a56626774e81216a4ce732a735e5bad4868a03f3ed65ca0c3919
  - name: proc-macro-error
    version: 1.0.4
    sha: da25490ff9892aab3fcf7c36f08cfb902dd3e71ca0f9f9517bea02a73a5ce38c
  - name: proc-macro-error-attr
    version: 1.0.4
    sha: a1be40180e52ecc98ad80b184934baf3d0d29f979574e439af5a55274b35f869
  - name: proc-macro2
    version: 1.0.56
    sha: 2b63bdb0cd06f1f4dedf69b254734f9b45af66e4a031e42a7480257d9898b435
  - name: quote
    version: 1.0.26
    sha: 4424af4bf778aae2051a77b60283332f386554255d722233d09fbfc7e30da2fc
  - name: rand
    version: 0.8.5
    sha: 34af8d1a0e25924bc5b7c43c079c942339d8f0a8b57c39049bef581b46327404
  - name: rand_chacha
    version: 0.3.1
    sha: e6c10a63a0fa32252be49d21e7709d4d4baf8d231c2dbce1eaa8141b9b127d88
  - name: rand_core
    version: 0.6.4
    sha: ec0be4795e2f6a28069bec0b5ff3e2ac9bafc99e6a9a7dc3547996c5c816922c
  - name: ryu
    version: 1.0.13
    sha: f91339c0467de62360649f8d3e185ca8de4224ff281f66000de5eb2a77a79041
  - name: scratch
    version: 1.0.5
    sha: 1792db035ce95be60c3f8853017b3999209281c24e2ba5bc8e59bf97a0c590c1
  - name: serde
    version: 1.0.160
    sha: bb2f3770c8bce3bcda7e149193a069a0f4365bda1fa5cd88e03bca26afc1216c
  - name: serde_derive
    version: 1.0.160
    sha: 291a097c63d8497e00160b166a967a4a79c64f3facdd01cbd7502231688d77df
  - name: serde_json
    version: 1.0.96
    sha: 057d394a50403bcac12672b2b18fb387ab6d289d957dab67dd201875391e52f1
  - name: serde_spanned
    version: 0.6.1
    sha: 0efd8caf556a6cebd3b285caf480045fcc1ac04f6bd786b09a6f11af30c4fcf4
  - name: slab
    version: 0.4.8
    sha: 6528351c9bc8ab22353f9d776db39a20288e8d6c37ef8cfe3317cf875eecfc2d
  - name: smallvec
    version: 1.10.0
    sha: a507befe795404456341dfab10cef66ead4c041f62b8b11bbb92bffe5d0953e0
  - name: syn
    version: 1.0.109
    sha: 72b64191b275b66ffe2469e8af2c1cfe3bafa67b529ead792a6d0160888b4237
  - name: syn
    version: 2.0.15
    sha: a34fcf3e8b60f57e6a14301a2e916d323af98b0ea63c599441eec8558660c822
  - name: system-deps
    version: 6.0.5
    sha: d0fe581ad25d11420b873cf9aedaca0419c2b411487b134d4d21065f3d092055
  - name: target-lexicon
    version: 0.12.7
    sha: fd1ba337640d60c3e96bc6f0638a939b9c9a7f2c316a1598c279828b3d1dc8c5
  - name: termcolor
    version: 1.2.0
    sha: be55cf8942feac5c765c2c993422806843c9a9a45d4d5c407ad6dd2ea95eb9b6
  - name: thiserror
    version: 1.0.40
    sha: 978c9a314bd8dc99be594bc3c175faaa9794be04a5a5e153caba6915336cebac
  - name: thiserror-impl
    version: 1.0.40
    sha: f9456a42c5b0d803c8cd86e73dd7cc9edd429499f37a3550d286d5e86720569f
  - name: time
    version: 0.1.45
    sha: 1b797afad3f312d1c66a56d11d0316f916356d11bd158fbc6ca6389ff6bf805a
  - name: toml
    version: 0.7.3
    sha: b403acf6f2bb0859c93c7f0d967cb4a75a7ac552100f9322faf64dc047669b21
  - name: toml_datetime
    version: 0.6.1
    sha: 3ab8ed2edee10b50132aed5f331333428b011c99402b5a534154ed15746f9622
  - name: toml_edit
    version: 0.19.8
    sha: 239410c8609e8125456927e6707163a3b1fdb40561e4b803bc041f466ccfdc13
  - name: unicode-ident
    version: 1.0.8
    sha: e5464a87b239f13a63a501f2701565754bae92d243d4bb7eb12f6d57d2269bf4
  - name: unicode-width
    version: 0.1.10
    sha: c0edd1e5b14653f783770bce4a4dabb4a5108a5370a5f5d8cfe8710c361f6c8b
  - name: uuid
    version: 1.3.1
    sha: 5b55a3fef2a1e3b3a00ce878640918820d3c51081576ac657d23af9fc7928fdb
  - name: version-compare
    version: 0.1.1
    sha: 579a42fc0b8e0c63b76519a339be31bed574929511fa53c1a3acae26eb258f29
  - name: version_check
    version: 0.9.4
    sha: 49874b5167b65d7193b8aba1567f5c7d93d001cafc34600cee003eda787e483f
  - name: wasi
    version: 0.10.0+wasi-snapshot-preview1
    sha: 1a143597ca7c7793eff794def352d41792a93c481eb1042423ff7ff72ba2c31f
  - name: wasi
    version: 0.11.0+wasi-snapshot-preview1
    sha: 9c8d87e72b64a3b4db28d11ce29237c246188f4f51057d65a7eab63b7987e423
  - name: wasm-bindgen
    version: 0.2.84
    sha: 31f8dcbc21f30d9b8f2ea926ecb58f6b91192c17e9d33594b3df58b2007ca53b
  - name: wasm-bindgen-backend
    version: 0.2.84
    sha: 95ce90fd5bcc06af55a641a86428ee4229e44e07033963a2290a8e241607ccb9
  - name: wasm-bindgen-macro
    version: 0.2.84
    sha: 4c21f77c0bedc37fd5dc21f897894a5ca01e7bb159884559461862ae90c0b4c5
  - name: wasm-bindgen-macro-support
    version: 0.2.84
    sha: 2aff81306fcac3c7515ad4e177f521b5c9a15f2b08f4e32d823066102f35a5f6
  - name: wasm-bindgen-shared
    version: 0.2.84
    sha: 0046fef7e28c3804e5e38bfa31ea2a0f73905319b677e57ebe37e49358989b5d
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
  - name: windows
    version: 0.48.0
    sha: e686886bc078bc1b0b600cac0147aadb815089b6e4da64016cbd754b6342700f
  - name: windows-targets
    version: 0.48.0
    sha: 7b1eb6f0cd7c80c79759c929114ef071b87354ce476d9d94271031c0497adfd5
  - name: windows_aarch64_gnullvm
    version: 0.48.0
    sha: 91ae572e1b79dba883e0d315474df7305d12f569b400fcf90581b06062f7e1bc
  - name: windows_aarch64_msvc
    version: 0.48.0
    sha: b2ef27e0d7bdfcfc7b868b317c1d32c641a6fe4629c171b8928c7b08d98d7cf3
  - name: windows_i686_gnu
    version: 0.48.0
    sha: 622a1962a7db830d6fd0a69683c80a18fda201879f0f447f065a3b7467daa241
  - name: windows_i686_msvc
    version: 0.48.0
    sha: 4542c6e364ce21bf45d69fdd2a8e455fa38d316158cfd43b3ac1c5b1b19f8e00
  - name: windows_x86_64_gnu
    version: 0.48.0
    sha: ca2b8a661f7628cbd23440e50b05d705db3686f894fc9580820623656af974b1
  - name: windows_x86_64_gnullvm
    version: 0.48.0
    sha: 7896dbc1f41e08872e9d5e8f8baa8fdd2677f29468c4e156210174edc7f7b953
  - name: windows_x86_64_msvc
    version: 0.48.0
    sha: 1a515f5799fe4961cb532f983ce2b23082366b898e52ffbce459c86f67c8378a
  - name: winnow
    version: 0.4.1
    sha: ae8970b36c66498d8ff1d66685dc86b91b29db0c7739899012f63a63814b4b28
  - name: yansi
    version: 0.5.1
    sha: 09041cd90cf85f7f8b2df60c646f853b7f535ce68f85244eb6731cf89fa498ec
