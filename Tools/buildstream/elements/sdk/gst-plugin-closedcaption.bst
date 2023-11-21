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
  url: crates:crates/gst-plugin-closedcaption/gst-plugin-closedcaption-0.11.2.crate
  ref: f16df441dcc9eab4b0d48d56be16ca59c7b33d03d325705d5f787f7e904fc69c
- kind: local
  path: files/gst-plugin-closedcaption/Cargo.lock
- kind: cargo
  ref:
  - name: aho-corasick
    version: 1.1.2
    sha: b2969dcb958b36655471fc61f7e416fa76033bdd4bfed0678d8fee1e2d07a1f0
  - name: android-tzdata
    version: 0.1.1
    sha: e999941b234f3131b00bc13c22d06e8c5ff726d1b6318ac7eb276997bbb4fef0
  - name: android_system_properties
    version: 0.1.5
    sha: 819e7219dbd41043ac279b19830f2efc897156490d7fd6ea916720117ee66311
  - name: anyhow
    version: 1.0.75
    sha: a4668cab20f66d8d020e1fbc0ebe47217433c1b6c8f2040faf858554e394ace6
  - name: atomic_refcell
    version: 0.1.13
    sha: 41e67cd8309bbd06cd603a9e693a784ac2e5d1e955f11286e355089fcab3047c
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: bitflags
    version: 2.4.1
    sha: 327762f6e5a765692301e5bb513e0d9fef63be86bbc14528052b1cd3e6f03e07
  - name: bumpalo
    version: 3.14.0
    sha: 7f30e7476521f6f8af1a1c4c0b8cc94f0bee37d91763d0ca2665f299b6cd8aec
  - name: byteorder
    version: 1.5.0
    sha: 1fd0f2584146f6f2ef48085050886acf353beff7305ebd1ae69500e27c67f64b
  - name: cairo-rs
    version: 0.18.3
    sha: f33613627f0dea6a731b0605101fad59ba4f193a52c96c4687728d822605a8a1
  - name: cairo-sys-rs
    version: 0.18.2
    sha: 685c9fa8e590b8b3d678873528d83411db17242a73fccaed827770ea0fedda51
  - name: cc
    version: 1.0.83
    sha: f1174fb0b6ec23863f8b971027804a42614e347eafb0a95bf0b12cdae21fc4d0
  - name: cea708-types
    version: 0.1.0
    sha: 3a23ec736ab2aafb861ef6f22c662cbc18c85e73945f86bd9e936a20be7cc958
  - name: cfg-expr
    version: 0.15.5
    sha: 03915af431787e6ffdcc74c645077518c6b6e01f80b761e0fbbfa288536311b3
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.31
    sha: 7f2c685bad3eb3d45a01354cedb7d5faa66194d1d58ba6e267a8de788f79db38
  - name: core-foundation-sys
    version: 0.8.4
    sha: e496a50fda8aacccc86d7529e2c1e0892dbd0f898a6b5645b5561b89c3210efa
  - name: diff
    version: 0.1.13
    sha: 56254986775e3233ffa9c4d7d3faaf6d36a2c09d30b20687e9f88bc8bafc16c8
  - name: either
    version: 1.9.0
    sha: a26ae43d7bcc3b814de94796a5e736d4029efb0ee900c12e2d54c993ad1a1e07
  - name: equivalent
    version: 1.0.1
    sha: 5443807d6dff69373d433ab9ef5378ad8df50ca6298caf15de6e52e24aaf54d5
  - name: futures-channel
    version: 0.3.29
    sha: ff4dd66668b557604244583e3e1e1eada8c5c2e96a6d0d6653ede395b78bbacb
  - name: futures-core
    version: 0.3.29
    sha: eb1d22c66e66d9d72e1758f0bd7d4fd0bee04cad842ee34587d68c07e45d088c
  - name: futures-executor
    version: 0.3.29
    sha: 0f4fb8693db0cf099eadcca0efe2a5a22e4550f98ed16aba6c48700da29597bc
  - name: futures-io
    version: 0.3.29
    sha: 8bf34a163b5c4c52d0478a4d757da8fb65cabef42ba90515efee0f6f9fa45aaa
  - name: futures-macro
    version: 0.3.29
    sha: 53b153fd91e4b0147f4aced87be237c98248656bb01050b96bf3ee89220a8ddb
  - name: futures-task
    version: 0.3.29
    sha: efd193069b0ddadc69c46389b740bbccdd97203899b48d09c5f7969591d6bae2
  - name: futures-util
    version: 0.3.29
    sha: a19526d624e703a3179b3d322efec918b6246ea0fa51d41124525f00f1cc8104
  - name: getrandom
    version: 0.2.11
    sha: fe9006bed769170c11f845cf00c7c1e9092aeb3f268e007c3e760ac68008070f
  - name: gio
    version: 0.18.3
    sha: 47d809baf02bdf1b5ef4ad3bf60dd9d4977149db4612b7bbb58e56aef168193b
  - name: gio-sys
    version: 0.18.1
    sha: 37566df850baf5e4cb0dfb78af2e4b9898d817ed9263d1090a2df958c64737d2
  - name: glib
    version: 0.18.3
    sha: 58cf801b6f7829fa76db37449ab67c9c98a2b1bf21076d9113225621e61a0fa6
  - name: glib-macros
    version: 0.18.3
    sha: 72793962ceece3863c2965d7f10c8786323b17c7adea75a515809fa20ab799a5
  - name: glib-sys
    version: 0.18.1
    sha: 063ce2eb6a8d0ea93d2bf8ba1957e78dbab6be1c2220dd3daca57d5a9d869898
  - name: gobject-sys
    version: 0.18.0
    sha: 0850127b514d1c4a4654ead6dedadb18198999985908e6ffe4436f53c785ce44
  - name: gst-plugin-version-helper
    version: 0.8.0
    sha: 334c37a15ca23861ad8159c8ff6e20f011aa2000defc5c9159096a28842f811b
  - name: gstreamer
    version: 0.21.2
    sha: ed97f98d186e63e49079b26af1a1b73e70ab7a2f450eb46a136f2bffc2bf12d5
  - name: gstreamer-base
    version: 0.21.2
    sha: cb150b6904a49052237fede7cc2e6479df6ced5043d95e6af8134bc141a3167f
  - name: gstreamer-base-sys
    version: 0.21.1
    sha: f4ca701f9078fe115b29b24c80910b577f9cb5b039182f050dbadf5933594b64
  - name: gstreamer-check
    version: 0.21.0
    sha: 4232ad2e2a34fdee63ac2c54b51f8e34792aac7bef6bda7c21ac923f7d94c4e9
  - name: gstreamer-check-sys
    version: 0.21.0
    sha: dfa8fb55ce66048b7174bd3d187e14272a4cfcc0aba276947f297fe26ef87afb
  - name: gstreamer-sys
    version: 0.21.2
    sha: 564cda782b3e6eed1b81cb4798a06794db56440fb05b422505be689f34ce3bc4
  - name: gstreamer-video
    version: 0.21.2
    sha: e85b2a4d1d3b7a98ae03806c3ed5c2db89d6b37a5f138780b48de015d68715e5
  - name: gstreamer-video-sys
    version: 0.21.2
    sha: 0302318d98e6b054501e485b6bb4ee20225823218f4a8660c182f115a33b16ee
  - name: hashbrown
    version: 0.14.2
    sha: f93e7192158dbcda357bdec5fb5788eebf8bbac027f3f33e719d29135ae84156
  - name: heck
    version: 0.4.1
    sha: 95505c38b4572b2d910cecb0281560f54b440a19336cbbcb27bf6ce6adc6f5a8
  - name: iana-time-zone
    version: 0.1.58
    sha: 8326b86b6cff230b97d0d312a6c40a60726df3332e721f72a1b035f451663b20
  - name: iana-time-zone-haiku
    version: 0.1.2
    sha: f31827a206f56af32e590ba56d5d2d085f558508192593743f16b2306495269f
  - name: indexmap
    version: 2.1.0
    sha: d530e1a18b1cb4c484e6e34556a0d948706958449fca0cab753d649f2bce3d1f
  - name: itertools
    version: 0.11.0
    sha: b1c173a5686ce8bfa551b3563d0c2170bf24ca44da99c7ca4bfdab5418c3fe57
  - name: itoa
    version: 1.0.9
    sha: af150ab688ff2122fcef229be89cb50dd66af9e01a4ff320cc137eecc9bacc38
  - name: js-sys
    version: 0.3.65
    sha: 54c0c35952f67de54bb584e9fd912b3023117cbafc0a77d8f3dee1fb5f572fe8
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: libc
    version: 0.2.150
    sha: 89d92a4743f9a61002fae18374ed11e7973f530cb3a3255fb354818118b2203c
  - name: log
    version: 0.4.20
    sha: b5e6163cb8c49088c2c36f57875e58ccd8c87c7427f7fbd50ea6710b2f3f2e8f
  - name: matchers
    version: 0.1.0
    sha: 8263075bb86c5a1b1427b5ae862e8889656f126e9f77c484496e8b47cf5c5558
  - name: memchr
    version: 2.6.4
    sha: f665ee40bc4a3c5590afb1e9677db74a508659dfd71e126420da8274909a0167
  - name: minimal-lexical
    version: 0.2.1
    sha: 68354c5c6bd36d73ff3feceb05efa59b6acb7626617f4962be322a825e61f79a
  - name: muldiv
    version: 1.0.1
    sha: 956787520e75e9bd233246045d19f42fb73242759cc57fba9611d940ae96d4b0
  - name: nom
    version: 7.1.3
    sha: d273983c5a657a70a3e8f2a01329822f3b8c8172b73826411a55751e404a0a4a
  - name: nu-ansi-term
    version: 0.46.0
    sha: 77a8165726e8236064dbb45459242600304b42a5ea24ee2948e18e023bf7ba84
  - name: num-integer
    version: 0.1.45
    sha: 225d3389fb3509a24c93f5c29eb6bde2586b98d9f016636dff58d7c6f7569cd9
  - name: num-rational
    version: 0.4.1
    sha: 0638a1c9d0a3c0914158145bc76cff373a75a627e6ecbfb71cbe6f453a5a19b0
  - name: num-traits
    version: 0.2.17
    sha: 39e3200413f237f41ab11ad6d161bc7239c84dcb631773ccd7de3dfe4b5c267c
  - name: once_cell
    version: 1.18.0
    sha: dd8b5dd2ae5ed71462c540258bedcb51965123ad7e7ccf4b9a8cafaa4a63576d
  - name: option-operations
    version: 0.5.0
    sha: 7c26d27bb1aeab65138e4bf7666045169d1717febcc9ff870166be8348b223d0
  - name: overload
    version: 0.1.1
    sha: b15813163c1d831bf4a13c3610c05c0d03b39feb07f7e09fa234dac9b15aaf39
  - name: pango
    version: 0.18.3
    sha: 7ca27ec1eb0457ab26f3036ea52229edbdb74dee1edd29063f5b9b010e7ebee4
  - name: pango-sys
    version: 0.18.0
    sha: 436737e391a843e5933d6d9aa102cb126d501e815b83601365a948a518555dc5
  - name: pangocairo
    version: 0.18.0
    sha: 57036589a9cfcacf83f9e606d15813fc6bf03f0e9e69aa2b5e3bb85af86b38a5
  - name: pangocairo-sys
    version: 0.18.0
    sha: fc3c8ff676a37e7a72ec1d5fc029f91c407278083d2752784ff9f5188c108833
  - name: paste
    version: 1.0.14
    sha: de3145af08024dea9fa9914f381a17b8fc6034dfb00f3a84013f7ff43f29ed4c
  - name: pin-project-lite
    version: 0.2.13
    sha: 8afb450f006bf6385ca15ef45d71d2288452bc3683ce2e2cacc0d18e4be60b58
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.27
    sha: 26072860ba924cbfa98ea39c8c19b4dd6a4a25423dbdf219c1eca91aa0cf6964
  - name: ppv-lite86
    version: 0.2.17
    sha: 5b40af805b3121feab8a3c29f04d8ad262fa8e0561883e7653e024ae4479e6de
  - name: pretty-hex
    version: 0.3.0
    sha: c6fa0831dd7cc608c38a5e323422a0077678fa5744aa2be4ad91c4ece8eec8d5
  - name: pretty_assertions
    version: 1.4.0
    sha: af7cee1a6c8a5b9208b3cb1061f10c0cb689087b3d8ce85fb9d2dd7a29b6ba66
  - name: proc-macro-crate
    version: 2.0.0
    sha: 7e8366a6159044a37876a2b9817124296703c586a5c92e2c53751fa06d8d43e8
  - name: proc-macro-error
    version: 1.0.4
    sha: da25490ff9892aab3fcf7c36f08cfb902dd3e71ca0f9f9517bea02a73a5ce38c
  - name: proc-macro-error-attr
    version: 1.0.4
    sha: a1be40180e52ecc98ad80b184934baf3d0d29f979574e439af5a55274b35f869
  - name: proc-macro2
    version: 1.0.69
    sha: 134c189feb4956b20f6f547d2cf727d4c0fe06722b20a0eec87ed445a97f92da
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
  - name: regex
    version: 1.10.2
    sha: 380b951a9c5e80ddfd6136919eef32310721aa4aacd4889a8d39124b026ab343
  - name: regex-automata
    version: 0.1.10
    sha: 6c230d73fb8d8c1b9c0b3135c5142a8acee3a0558fb8db5cf1cb65f8d7862132
  - name: regex-automata
    version: 0.4.3
    sha: 5f804c7828047e88b2d32e2d7fe5a105da8ee3264f01902f796c8e067dc2483f
  - name: regex-syntax
    version: 0.6.29
    sha: f162c6dd7b008981e4d40210aca20b4bd0f9b60ca9271061b07f78537722f2e1
  - name: regex-syntax
    version: 0.8.2
    sha: c08c74e62047bb2de4ff487b251e4a92e24f48745648451635cec7d591162d9f
  - name: ryu
    version: 1.0.15
    sha: 1ad4cc8da4ef723ed60bced201181d83791ad433213d8c24efffda1eec85d741
  - name: serde
    version: 1.0.192
    sha: bca2a08484b285dcb282d0f67b26cadc0df8b19f8c12502c13d966bf9482f001
  - name: serde_derive
    version: 1.0.192
    sha: d6c7207fbec9faa48073f3e3074cbe553af6ea512d7c21ba46e434e70ea9fbc1
  - name: serde_json
    version: 1.0.108
    sha: 3d1c7e3eac408d115102c4c24ad393e0821bb3a5df4d506a80f85f7a742a526b
  - name: serde_spanned
    version: 0.6.4
    sha: 12022b835073e5b11e90a14f86838ceb1c8fb0325b72416845c487ac0fa95e80
  - name: sharded-slab
    version: 0.1.7
    sha: f40ca3c46823713e0d4209592e8d6e826aa57e928f09752619fc696c499637f6
  - name: slab
    version: 0.4.9
    sha: 8f92a496fb766b417c996b9c5e57daf2f7ad3b0bebe1ccfca4856390e3d3bb67
  - name: smallvec
    version: 1.11.2
    sha: 4dccd0940a2dcdf68d092b8cbab7dc0ad8fa938bf95787e1b916b0e3d0e8e970
  - name: syn
    version: 1.0.109
    sha: 72b64191b275b66ffe2469e8af2c1cfe3bafa67b529ead792a6d0160888b4237
  - name: syn
    version: 2.0.39
    sha: 23e78b90f2fcf45d3e842032ce32e3f2d1545ba6636271dcbf24fa306d87be7a
  - name: system-deps
    version: 6.2.0
    sha: 2a2d580ff6a20c55dfb86be5f9c238f67835d0e81cbdea8bf5680e0897320331
  - name: target-lexicon
    version: 0.12.12
    sha: 14c39fd04924ca3a864207c66fc2cd7d22d7c016007f9ce846cbb9326331930a
  - name: thiserror
    version: 1.0.50
    sha: f9a7210f5c9a7156bb50aa36aed4c95afb51df0df00713949448cf9e97d382d2
  - name: thiserror-impl
    version: 1.0.50
    sha: 266b2e40bc00e5a6c09c3584011e08b06f123c00362c92b975ba9843aaaa14b8
  - name: thread_local
    version: 1.1.7
    sha: 3fdd6f064ccff2d6567adcb3873ca630700f00b5ad3f060c25b5dcfd9a4ce152
  - name: toml
    version: 0.8.8
    sha: a1a195ec8c9da26928f773888e0742ca3ca1040c6cd859c919c9f59c1954ab35
  - name: toml_datetime
    version: 0.6.5
    sha: 3550f4e9685620ac18a50ed434eb3aec30db8ba93b0287467bca5826ea25baf1
  - name: toml_edit
    version: 0.20.7
    sha: 70f427fce4d84c72b5b732388bf4a9f4531b53f74e2887e3ecb2481f68f66d81
  - name: toml_edit
    version: 0.21.0
    sha: d34d383cd00a163b4a5b85053df514d45bc330f6de7737edfe0a93311d1eaa03
  - name: tracing
    version: 0.1.40
    sha: c3523ab5a71916ccf420eebdf5521fcef02141234bbc0b8a49f2fdc4544364ef
  - name: tracing-attributes
    version: 0.1.27
    sha: 34704c8d6ebcbc939824180af020566b01a7c01f80641264eba0999f6c2b6be7
  - name: tracing-core
    version: 0.1.32
    sha: c06d3da6113f116aaee68e4d601191614c9053067f9ab7f6edbcb161237daa54
  - name: tracing-log
    version: 0.2.0
    sha: ee855f1f400bd0e5c02d150ae5de3840039a3f54b025156404e34c23c03f47c3
  - name: tracing-subscriber
    version: 0.3.18
    sha: ad0f048c97dbd9faa9b7df56362b8ebcaa52adb06b498c050d2f4e32f90a7a8b
  - name: unicode-ident
    version: 1.0.12
    sha: 3354b9ac3fae1ff6755cb6db53683adb661634f67557942dea4facebec0fee4b
  - name: uuid
    version: 1.5.0
    sha: 88ad59a7560b41a70d191093a945f0b87bc1deeda46fb237479708a1d6b6cdfc
  - name: valuable
    version: 0.1.0
    sha: 830b7e5d4d90034032940e4ace0d9a9a057e7a45cd94e6c007832e39edb82f6d
  - name: version-compare
    version: 0.1.1
    sha: 579a42fc0b8e0c63b76519a339be31bed574929511fa53c1a3acae26eb258f29
  - name: version_check
    version: 0.9.4
    sha: 49874b5167b65d7193b8aba1567f5c7d93d001cafc34600cee003eda787e483f
  - name: wasi
    version: 0.11.0+wasi-snapshot-preview1
    sha: 9c8d87e72b64a3b4db28d11ce29237c246188f4f51057d65a7eab63b7987e423
  - name: wasm-bindgen
    version: 0.2.88
    sha: 7daec296f25a1bae309c0cd5c29c4b260e510e6d813c286b19eaadf409d40fce
  - name: wasm-bindgen-backend
    version: 0.2.88
    sha: e397f4664c0e4e428e8313a469aaa58310d302159845980fd23b0f22a847f217
  - name: wasm-bindgen-macro
    version: 0.2.88
    sha: 5961017b3b08ad5f3fe39f1e79877f8ee7c23c5e5fd5eb80de95abc41f1f16b2
  - name: wasm-bindgen-macro-support
    version: 0.2.88
    sha: c5353b8dab669f5e10f5bd76df26a9360c748f054f862ff5f3f8aae0c7fb3907
  - name: wasm-bindgen-shared
    version: 0.2.88
    sha: 0d046c5d029ba91a1ed14da14dca44b68bf2f124cfbaf741c54151fdb3e0750b
  - name: winapi
    version: 0.3.9
    sha: 5c839a674fcd7a98952e593242ea400abe93992746761e38641405d28b00f419
  - name: winapi-i686-pc-windows-gnu
    version: 0.4.0
    sha: ac3b87c63620426dd9b991e5ce0329eff545bccbbb34f3be09ff6fb6ab51b7b6
  - name: winapi-x86_64-pc-windows-gnu
    version: 0.4.0
    sha: 712e227841d057c1ee1cd2fb22fa7e5a5461ae8e48fa2ca79ec42cfc1931183f
  - name: windows-core
    version: 0.51.1
    sha: f1f8cf84f35d2db49a46868f947758c7a1138116f7fac3bc844f43ade1292e64
  - name: windows-targets
    version: 0.48.5
    sha: 9a2fa6e2155d7247be68c096456083145c183cbbbc2764150dda45a87197940c
  - name: windows_aarch64_gnullvm
    version: 0.48.5
    sha: 2b38e32f0abccf9987a4e3079dfb67dcd799fb61361e53e2882c3cbaf0d905d8
  - name: windows_aarch64_msvc
    version: 0.48.5
    sha: dc35310971f3b2dbbf3f0690a219f40e2d9afcf64f9ab7cc1be722937c26b4bc
  - name: windows_i686_gnu
    version: 0.48.5
    sha: a75915e7def60c94dcef72200b9a8e58e5091744960da64ec734a6c6e9b3743e
  - name: windows_i686_msvc
    version: 0.48.5
    sha: 8f55c233f70c4b27f66c523580f78f1004e8b5a8b659e05a4eb49d4166cca406
  - name: windows_x86_64_gnu
    version: 0.48.5
    sha: 53d40abd2583d23e4718fddf1ebec84dbff8381c07cae67ff7768bbf19c6718e
  - name: windows_x86_64_gnullvm
    version: 0.48.5
    sha: 0b7b52767868a23d5bab768e390dc5f5c55825b6d30b86c844ff2dc7414044cc
  - name: windows_x86_64_msvc
    version: 0.48.5
    sha: ed94fce61571a4006852b7389a063ab983c02eb1bb37b47f8272ce92d06d9538
  - name: winnow
    version: 0.5.19
    sha: 829846f3e3db426d4cee4510841b71a8e58aa2a76b1132579487ae430ccd9c7b
  - name: yansi
    version: 0.5.1
    sha: 09041cd90cf85f7f8b2df60c646f853b7f535ce68f85244eb6731cf89fa498ec
