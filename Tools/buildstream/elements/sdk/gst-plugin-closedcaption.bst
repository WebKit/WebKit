kind: manual

build-depends:
- freedesktop-sdk.bst:components/rust.bst
- freedesktop-sdk.bst:components/pkg-config.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
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
      PAGER=this-is-not-a-pager gst-inspect-1.0 cea608tott

sources:
- kind: tar
  url: https://static.crates.io/crates/gst-plugin-closedcaption/gst-plugin-closedcaption-0.6.0.crate
  ref: 0650e6cd4be2f6c45516799a0b82e4bcf70eb60c5e3b4aeae2bdcbd001031f55
- kind: local
  path: files/gst-plugin-closedcaption/Cargo.lock
- kind: cargo
  ref:
  - name: ansi_term
    version: 0.11.0
    sha: ee49baf6cb617b853aa8d93bf420db2383fab46d314482ca2803b40d5fde979b
  - name: anyhow
    version: 1.0.37
    sha: ee67c11feeac938fae061b232e38e0b6d94f97a9df10e6271319325ac4c56a86
  - name: atomic_refcell
    version: 0.1.6
    sha: 3bc31dce067eab974c815a9deb95f6217806de7b53685d7fc31f8ccf3fb2539f
  - name: autocfg
    version: 1.0.1
    sha: cdb031dd78e28731d87d56cc8ffef4a8f36ca26c38fe2de700543e627f8a464a
  - name: bitflags
    version: 1.2.1
    sha: cf1de2fe8c75bc145a2f577add951f8134889b4795d47466a54a5c846d691693
  - name: bytes
    version: 1.0.0
    sha: ad1f8e949d755f9d79112b5bb46938e0ef9d3804a0b16dfab13aafcaa5f0fa72
  - name: cairo-rs
    version: 0.9.1
    sha: c5c0f2e047e8ca53d0ff249c54ae047931d7a6ebe05d00af73e0ffeb6e34bdb8
  - name: cairo-sys-rs
    version: 0.10.0
    sha: 2ed2639b9ad5f1d6efa76de95558e11339e7318426d84ac4890b86c03e828ca7
  - name: cc
    version: 1.0.66
    sha: 4c0496836a84f8d0495758516b8621a622beb77c0fed418570e50764093ced48
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.19
    sha: 670ad68c9088c2a963aaa298cb369688cf3f9465ce5e2d4ca10e6e0098a1ce73
  - name: combine
    version: 4.5.1
    sha: 01199925a18b00193570e3d70cfe57dcb647eb167c29851983e76fc39e2fee39
  - name: ctor
    version: 0.1.17
    sha: 373c88d9506e2e9230f6107701b7d8425f4cb3f6df108ec3042a26e936666da5
  - name: difference
    version: 2.0.0
    sha: 524cbf6897b527295dff137cec09ecf3a05f4fddffd7dfcd1585403449e74198
  - name: either
    version: 1.6.1
    sha: e78d4f1cc4ae33bbfc157ed5d5a5ef3bc29227303d595861deb238fcec4e9457
  - name: futures-channel
    version: 0.3.8
    sha: 4b7109687aa4e177ef6fe84553af6280ef2778bdb7783ba44c9dc3399110fe64
  - name: futures-core
    version: 0.3.8
    sha: 847ce131b72ffb13b6109a221da9ad97a64cbe48feb1028356b836b47b8f1748
  - name: futures-executor
    version: 0.3.8
    sha: 4caa2b2b68b880003057c1dd49f1ed937e38f22fcf6c212188a121f08cf40a65
  - name: futures-macro
    version: 0.3.8
    sha: 77408a692f1f97bcc61dc001d752e00643408fbc922e4d634c655df50d595556
  - name: futures-task
    version: 0.3.8
    sha: 7c554eb5bf48b2426c4771ab68c6b14468b6e76cc90996f528c3338d761a4d0d
  - name: futures-util
    version: 0.3.8
    sha: d304cff4a7b99cfb7986f7d43fbe93d175e72e704a8860787cc95e9ffd85cbd2
  - name: getrandom
    version: 0.1.16
    sha: 8fc3cb4d91f53b50155bdcfd23f6a4c39ae1969c2ae85982b135750cccaf5fce
  - name: glib
    version: 0.10.3
    sha: 0c685013b7515e668f1b57a165b009d4d28cb139a8a989bbd699c10dad29d0c5
  - name: glib-macros
    version: 0.10.1
    sha: 41486a26d1366a8032b160b59065a59fb528530a46a49f627e7048fb8c064039
  - name: glib-sys
    version: 0.10.1
    sha: c7e9b997a66e9a23d073f2b1abb4dbfc3925e0b8952f67efd8d9b6e168e4cdc1
  - name: gobject-sys
    version: 0.10.0
    sha: 952133b60c318a62bf82ee75b93acc7e84028a093e06b9e27981c2b6fe68218c
  - name: gst-plugin-version-helper
    version: 0.2.0
    sha: cc81ae3a09f23761eb3b122f50e5daedea4b44fdf8242ec133bc87aa8e24f9b8
  - name: gstreamer
    version: 0.16.5
    sha: 5d50f822055923f1cbede233aa5dfd4ee957cf328fb3076e330886094e11d6cf
  - name: gstreamer-base
    version: 0.16.5
    sha: bafd01c56f59cb10f4b5a10f97bb4bdf8c2b2784ae5b04da7e2d400cf6e6afcf
  - name: gstreamer-base-sys
    version: 0.9.1
    sha: a4b7b6dc2d6e160a1ae28612f602bd500b3fa474ce90bf6bb2f08072682beef5
  - name: gstreamer-check
    version: 0.16.5
    sha: aec46cf3b6dc52ca672adb0241149e1beff756c48d2f283894bdc8f2d5ae20a0
  - name: gstreamer-check-sys
    version: 0.9.1
    sha: d652c6d91269821c4062ab36f27f0f598d9962b511683385988db40666b25e84
  - name: gstreamer-sys
    version: 0.9.1
    sha: fc1f154082d01af5718c5f8a8eb4f565a4ea5586ad8833a8fc2c2aa6844b601d
  - name: gstreamer-video
    version: 0.16.6
    sha: 75fb6c5bcc0d406ca4a47938b8592121ff98e7a8f4303c24def1722df9f203d5
  - name: gstreamer-video-sys
    version: 0.9.1
    sha: 92347e46438007d6a2386302125f62cb9df6769cdacb931af5c0f12c1ee21de4
  - name: heck
    version: 0.3.2
    sha: 87cbf45460356b7deeb5e3415b5563308c0a9b057c85e12b06ad551f98d0a6ac
  - name: itertools
    version: 0.9.0
    sha: 284f18f85651fe11e8a991b2adb42cb078325c996ed026d994719efcfca1d54b
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: libc
    version: 0.2.81
    sha: 1482821306169ec4d07f6aca392a4681f66c75c9918aa49641a2595db64053cb
  - name: memchr
    version: 2.3.4
    sha: 0ee1c47aaa256ecabcaea351eae4a9b01ef39ed810004e298d2511ed284b1525
  - name: muldiv
    version: 0.2.1
    sha: 0419348c027fa7be448d2ae7ea0e4e04c2334c31dc4e74ab29f00a2a7ca69204
  - name: num-integer
    version: 0.1.44
    sha: d2cc698a63b549a70bc047073d2949cce27cd1c7b0a4a862d08a8031bc2801db
  - name: num-rational
    version: 0.3.2
    sha: 12ac428b1cb17fce6f731001d307d351ec70a6d202fc2e60f7d4c5e42d8f4f07
  - name: num-traits
    version: 0.2.14
    sha: 9a64b1ec5cda2586e284722486d802acf1f7dbdc623e2bfc57e65ca1cd099290
  - name: once_cell
    version: 1.5.2
    sha: 13bd41f508810a131401606d54ac32a467c97172d74ba7662562ebba5ad07fa0
  - name: output_vt100
    version: 0.1.2
    sha: 53cdc5b785b7a58c5aad8216b3dfa114df64b0b06ae6e1501cef91df2fbdf8f9
  - name: pango
    version: 0.9.1
    sha: 9937068580bebd8ced19975938573803273ccbcbd598c58d4906efd4ac87c438
  - name: pango-sys
    version: 0.10.0
    sha: 24d2650c8b62d116c020abd0cea26a4ed96526afda89b1c4ea567131fdefc890
  - name: pangocairo
    version: 0.10.0
    sha: 00f5ae67a05a5e023f09f64e9a71c845274d4b82dedee237b70425811885e883
  - name: pangocairo-sys
    version: 0.11.0
    sha: 94ccc97f698c2f0233b84e5ca676893a1e676785b60eec700b9c0e6dcd0feb98
  - name: paste
    version: 1.0.4
    sha: c5d65c4d95931acda4498f675e332fcbdc9a06705cd07086c510e9b6009cd1c1
  - name: pin-project
    version: 1.0.3
    sha: 5a83804639aad6ba65345661744708855f9fbcb71176ea8d28d05aeb11d975e7
  - name: pin-project-internal
    version: 1.0.3
    sha: b7bcc46b8f73443d15bc1c5fecbb315718491fa9187fa483f0e359323cde8b3a
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.19
    sha: 3831453b3449ceb48b6d9c7ad7c96d5ea673e9b470a1dc578c2ce6521230884c
  - name: ppv-lite86
    version: 0.2.10
    sha: ac74c624d6b2d21f425f752262f42188365d7b8ff1aff74c82e45136510a4857
  - name: pretty-hex
    version: 0.2.1
    sha: bc5c99d529f0d30937f6f4b8a86d988047327bb88d04d2c4afc356de74722131
  - name: pretty_assertions
    version: 0.6.1
    sha: 3f81e1644e1b54f5a68959a29aa86cde704219254669da328ecfdf6a1f09d427
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
    version: 0.5.19
    sha: dbf0c48bc1d91375ae5c3cd81e3722dff1abcf81a30960240640d223f59fe0e5
  - name: proc-macro-nested
    version: 0.1.6
    sha: eba180dafb9038b050a4c280019bbedf9f2467b61e5d892dcad585bb57aadc5a
  - name: proc-macro2
    version: 1.0.24
    sha: 1e0704ee1a7e00d7bb417d0770ea303c1bccbabf0ef1667dae92b5967f5f8a71
  - name: quote
    version: 1.0.8
    sha: 991431c3519a3f36861882da93630ce66b52918dcf1b8e2fd66b397fc96f28df
  - name: rand
    version: 0.7.3
    sha: 6a6b1679d49b24bbfe0c803429aa1874472f50d9b363131f0e89fc356b544d03
  - name: rand_chacha
    version: 0.2.2
    sha: f4c8ed856279c9737206bf725bf36935d8666ead7aa69b52be55af369d193402
  - name: rand_core
    version: 0.5.1
    sha: 90bde5296fc891b0cef12a6d03ddccc162ce7b2aff54160af9338f8d40df6d19
  - name: rand_hc
    version: 0.2.0
    sha: ca3129af7b92a17112d59ad498c6f81eaf463253766b90396d39ea7a39d6613c
  - name: rand_pcg
    version: 0.2.1
    sha: 16abd0c1b639e9eb4d7c50c0b8100b0d0f849be2349829c740fe8e6eb4816429
  - name: serde
    version: 1.0.118
    sha: 06c64263859d87aa2eb554587e2d23183398d617427327cf2b3d0ed8c69e4800
  - name: slab
    version: 0.4.2
    sha: c111b5bd5695e56cffe5129854aa230b39c93a305372fdbb2668ca2394eea9f8
  - name: strum
    version: 0.18.0
    sha: 57bd81eb48f4c437cadc685403cad539345bf703d78e63707418431cecd4522b
  - name: strum_macros
    version: 0.18.0
    sha: 87c85aa3f8ea653bfd3ddf25f7ee357ee4d204731f6aa9ad04002306f6e2774c
  - name: syn
    version: 1.0.57
    sha: 4211ce9909eb971f111059df92c45640aad50a619cf55cd76476be803c4c68e6
  - name: system-deps
    version: 1.3.2
    sha: 0f3ecc17269a19353b3558b313bba738b25d82993e30d62a18406a24aba4649b
  - name: thiserror
    version: 1.0.23
    sha: 76cc616c6abf8c8928e2fdcc0dbfab37175edd8fb49a4641066ad1364fdab146
  - name: thiserror-impl
    version: 1.0.23
    sha: 9be73a2caec27583d0046ef3796c3794f868a5bc813db689eed00c7631275cd1
  - name: time
    version: 0.1.44
    sha: 6db9e6914ab8b1ae1c260a4ae7a49b6c5611b40328a735b21862567685e73255
  - name: toml
    version: 0.5.8
    sha: a31142970826733df8241ef35dc040ef98c679ab14d7c3e54d827099b3acecaa
  - name: unicode-segmentation
    version: 1.7.1
    sha: bb0d2e7be6ae3a5fa87eed5fb451aff96f2573d2694942e40543ae0bbe19c796
  - name: unicode-xid
    version: 0.2.1
    sha: f7fe0bb3479651439c9112f72b6c505038574c9fbb575ed1bf3b797fa39dd564
  - name: uuid
    version: 0.8.1
    sha: 9fde2f6a4bea1d6e007c4ad38c6839fa71cbb63b6dbf5b595aa38dc9b1093c11
  - name: version-compare
    version: 0.0.10
    sha: d63556a25bae6ea31b52e640d7c41d1ab27faba4ccb600013837a3d0b3994ca1
  - name: version_check
    version: 0.9.2
    sha: b5a972e5669d67ba988ce3dc826706fb0a8b01471c088cb0b6110b805cc36aed
  - name: wasi
    version: 0.10.0+wasi-snapshot-preview1
    sha: 1a143597ca7c7793eff794def352d41792a93c481eb1042423ff7ff72ba2c31f
  - name: wasi
    version: 0.9.0+wasi-snapshot-preview1
    sha: cccddf32554fecc6acb585f82a32a72e28b48f8c4c1883ddfeeeaa96f7d8e519
  - name: winapi
    version: 0.3.9
    sha: 5c839a674fcd7a98952e593242ea400abe93992746761e38641405d28b00f419
  - name: winapi-i686-pc-windows-gnu
    version: 0.4.0
    sha: ac3b87c63620426dd9b991e5ce0329eff545bccbbb34f3be09ff6fb6ab51b7b6
  - name: winapi-x86_64-pc-windows-gnu
    version: 0.4.0
    sha: 712e227841d057c1ee1cd2fb22fa7e5a5461ae8e48fa2ca79ec42cfc1931183f
