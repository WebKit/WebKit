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
      PAGER=this-is-not-a-pager gst-inspect-1.0 rsrtp

sources:
- kind: tar
  url: https://static.crates.io/crates/gst-plugin-rtp/gst-plugin-rtp-0.9.0.crate
  ref: 7932ef270f58bed6f559df7da5b662b083ac0b5b5d70d8cfe96557243e66f9c6
- kind: local
  path: files/gst-plugin-rtp/Cargo.lock
- kind: cargo
  ref:
  - name: android_system_properties
    version: 0.1.5
    sha: 819e7219dbd41043ac279b19830f2efc897156490d7fd6ea916720117ee66311
  - name: anyhow
    version: 1.0.66
    sha: 216261ddc8289130e551ddcd5ce8a064710c0d064a4d2895c67151c92b5443f6
  - name: atomic_refcell
    version: 0.1.8
    sha: 73b5e5f48b927f04e952dedc932f31995a65a0bf65ec971c74436e51bf6e970d
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: bitflags
    version: 1.3.2
    sha: bef38d45163c2f1dde094a7dfd33ccf595c92905c8f8f4fdc18d06fb1037718a
  - name: bitstream-io
    version: 1.5.0
    sha: 97d524fdb78bf6dc6d2dc4c02043e4b4962ede0a17ae3e13f0ed211a7eda5897
  - name: bumpalo
    version: 3.11.1
    sha: 572f695136211188308f16ad2ca5c851a712c464060ae6974944458eb83880ba
  - name: cc
    version: 1.0.74
    sha: 581f5dba903aac52ea3feb5ec4810848460ee833876f1f9b0fdeab1f19091574
  - name: cfg-expr
    version: 0.11.0
    sha: b0357a6402b295ca3a86bc148e84df46c02e41f41fef186bda662557ef6328aa
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.22
    sha: bfd4d1b31faaa3a89d7934dbded3111da0d2ef28e3ebccdb4f0179f5929d1ef1
  - name: codespan-reporting
    version: 0.11.1
    sha: 3538270d33cc669650c4b093848450d380def10c331d38c768e34cac80576e6e
  - name: core-foundation-sys
    version: 0.8.3
    sha: 5827cebf4670468b8772dd191856768aedcb1b0278a04f989f7766351917b9dc
  - name: cxx
    version: 1.0.80
    sha: 6b7d4e43b25d3c994662706a1d4fcfc32aaa6afd287502c111b237093bb23f3a
  - name: cxx-build
    version: 1.0.80
    sha: 84f8829ddc213e2c1368e51a2564c552b65a8cb6a28f31e576270ac81d5e5827
  - name: cxxbridge-flags
    version: 1.0.80
    sha: e72537424b474af1460806647c41d4b6d35d09ef7fe031c5c2fa5766047cc56a
  - name: cxxbridge-macro
    version: 1.0.80
    sha: 309e4fb93eed90e1e14bea0da16b209f81813ba9fc7830c20ed151dd7bc0a4d7
  - name: futures-channel
    version: 0.3.25
    sha: 52ba265a92256105f45b719605a571ffe2d1f0fea3807304b522c1d778f79eed
  - name: futures-core
    version: 0.3.25
    sha: 04909a7a7e4633ae6c4a9ab280aeb86da1236243a77b694a49eacd659a4bd3ac
  - name: futures-executor
    version: 0.3.25
    sha: 7acc85df6714c176ab5edf386123fafe217be88c0840ec11f199441134a074e2
  - name: futures-macro
    version: 0.3.25
    sha: bdfb8ce053d86b91919aad980c220b1fb8401a9394410e1c289ed7e66b61835d
  - name: futures-task
    version: 0.3.25
    sha: 2ffb393ac5d9a6eaa9d3fdf37ae2776656b706e200c8e16b1bdb227f5198e6ea
  - name: futures-util
    version: 0.3.25
    sha: 197676987abd2f9cadff84926f410af1c183608d36641465df73ae8211dc65d6
  - name: gio-sys
    version: 0.16.0
    sha: 6da1bba9d3f2ab13a6e9932c40f240dc99ebc9f0bdc35cfb130d1a3df36f374c
  - name: glib
    version: 0.16.2
    sha: d5abffa711471e015eb93d65d6ea20e7e9f6f7951fc0a1042280439319b2de06
  - name: glib-macros
    version: 0.16.0
    sha: e195c1311fa6b04d7b896ea39385f6bd60ef5d25bf74a7c11c8c3f94f6c1a572
  - name: glib-sys
    version: 0.16.0
    sha: b33357bb421a77bd849f6a0bfcaf3b4b256a2577802971bb5dd522d530f27021
  - name: gobject-sys
    version: 0.16.0
    sha: 63ca11a57400f3d4fda594e002844be47900c9fb8b29e2155c6e37a1f24e51b3
  - name: gst-plugin-version-helper
    version: 0.7.4
    sha: 747ffe0e4067acfb98d6f7cbbe0a1901794587a93ab2b36c4652bc75c28d865d
  - name: gstreamer
    version: 0.19.1
    sha: 7e428081934c617115320750b7827f8f13131d9c3ae90b647c14a5d6019f47b4
  - name: gstreamer-base
    version: 0.19.1
    sha: 326674197c010e91a98d0f55a032abe22b1fd932456dbcdc3415450b4b653817
  - name: gstreamer-base-sys
    version: 0.19.0
    sha: cd55d3858fa65a99286c1cbe8db001f4ce5cff6a038f1c1253f5d99f840970de
  - name: gstreamer-check
    version: 0.19.0
    sha: 4fee30d86b4400912a48e56ded1364b33beb147c7f23ff2d65d57f2945142014
  - name: gstreamer-check-sys
    version: 0.19.0
    sha: 8cfe3391cf36f7c8c16c4897569adfe5589d99b4c3453d7c4a7ecf912a3d98fd
  - name: gstreamer-rtp
    version: 0.19.0
    sha: b9edd844db216c2f52ae2be852a94faf6a9ce9a0ec3e340f2b2291dd4c035748
  - name: gstreamer-rtp-sys
    version: 0.19.0
    sha: 7acd6516dfa557d3f00c2043026a9fdb22bbf17e1c2fd776c6471f4f2904000e
  - name: gstreamer-sys
    version: 0.19.0
    sha: fbaafc66df32b334d4aa28025fd5d83cadc971e1910205e140ea070f4ac4834f
  - name: heck
    version: 0.4.0
    sha: 2540771e65fc8cb83cd6e8a237f70c319bd5c29f78ed1084ba5d50eeac86f7f9
  - name: iana-time-zone
    version: 0.1.53
    sha: 64c122667b287044802d6ce17ee2ddf13207ed924c712de9a66a5814d5b64765
  - name: iana-time-zone-haiku
    version: 0.1.1
    sha: 0703ae284fc167426161c2e3f1da3ea71d94b21bedbcc9494e92b28e334e3dca
  - name: js-sys
    version: 0.3.60
    sha: 49409df3e3bf0856b916e2ceaca09ee28e6871cf7d9ce97a692cacfdb2a25a47
  - name: libc
    version: 0.2.137
    sha: fc7fcc620a3bff7cdd7a365be3376c97191aeaccc2a603e600951e452615bf89
  - name: link-cplusplus
    version: 1.0.7
    sha: 9272ab7b96c9046fbc5bc56c06c117cb639fe2d509df0c421cad82d2915cf369
  - name: log
    version: 0.4.17
    sha: abb12e687cfb44aa40f41fc3978ef76448f9b6038cad6aef4259d3c095a2382e
  - name: muldiv
    version: 1.0.1
    sha: 956787520e75e9bd233246045d19f42fb73242759cc57fba9611d940ae96d4b0
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
    version: 1.16.0
    sha: 86f0b0d4bf799edbc74508c1e8bf170ff5f41238e5f8225603ca7caaae2b7860
  - name: option-operations
    version: 0.5.0
    sha: 7c26d27bb1aeab65138e4bf7666045169d1717febcc9ff870166be8348b223d0
  - name: paste
    version: 1.0.9
    sha: b1de2e551fb905ac83f73f7aedf2f0cb4a0da7e35efa24a202a936269f1f18e1
  - name: pin-project-lite
    version: 0.2.9
    sha: e0a7ae3ac2f1173085d398531c705756c94a4c56843785df85a60c1a0afac116
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.26
    sha: 6ac9a59f73473f1b8d852421e59e64809f025994837ef743615c6d0c5b305160
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
    version: 1.0.47
    sha: 5ea3d908b0e36316caf9e9e2c4625cdde190a7e6f440d794667ed17a1855e725
  - name: quote
    version: 1.0.21
    sha: bbe448f377a7d6961e30f5955f9b8d106c3f5e449d493ee1b125c1d43c2b5179
  - name: scratch
    version: 1.0.2
    sha: 9c8132065adcfd6e02db789d9285a0deb2f3fcb04002865ab67d5fb103533898
  - name: serde
    version: 1.0.147
    sha: d193d69bae983fc11a79df82342761dfbf28a99fc8d203dca4c3c1b590948965
  - name: slab
    version: 0.4.7
    sha: 4614a76b2a8be0058caa9dbbaf66d988527d86d003c11a94fbd335d7661edcef
  - name: smallvec
    version: 1.10.0
    sha: a507befe795404456341dfab10cef66ead4c041f62b8b11bbb92bffe5d0953e0
  - name: syn
    version: 1.0.103
    sha: a864042229133ada95abf3b54fdc62ef5ccabe9515b64717bcb9a1919e59445d
  - name: system-deps
    version: 6.0.3
    sha: 2955b1fe31e1fa2fbd1976b71cc69a606d7d4da16f6de3333d0c92d51419aeff
  - name: termcolor
    version: 1.1.3
    sha: bab24d30b911b2376f3a13cc2cd443142f0c81dda04c118693e35b3835757755
  - name: thiserror
    version: 1.0.37
    sha: 10deb33631e3c9018b9baf9dcbbc4f737320d2b576bac10f6aefa048fa407e3e
  - name: thiserror-impl
    version: 1.0.37
    sha: 982d17546b47146b28f7c22e3d08465f6b8903d0ea13c1660d9d84a6e7adcdbb
  - name: toml
    version: 0.5.9
    sha: 8d82e1a7758622a465f8cee077614c73484dac5b836c02ff6a40d5d1010324d7
  - name: unicode-ident
    version: 1.0.5
    sha: 6ceab39d59e4c9499d4e5a8ee0e2735b891bb7308ac83dfb4e80cad195c9f6f3
  - name: unicode-width
    version: 0.1.10
    sha: c0edd1e5b14653f783770bce4a4dabb4a5108a5370a5f5d8cfe8710c361f6c8b
  - name: version-compare
    version: 0.1.0
    sha: fe88247b92c1df6b6de80ddc290f3976dbdf2f5f5d3fd049a9fb598c6dd5ca73
  - name: version_check
    version: 0.9.4
    sha: 49874b5167b65d7193b8aba1567f5c7d93d001cafc34600cee003eda787e483f
  - name: wasm-bindgen
    version: 0.2.83
    sha: eaf9f5aceeec8be17c128b2e93e031fb8a4d469bb9c4ae2d7dc1888b26887268
  - name: wasm-bindgen-backend
    version: 0.2.83
    sha: 4c8ffb332579b0557b52d268b91feab8df3615f265d5270fec2a8c95b17c1142
  - name: wasm-bindgen-macro
    version: 0.2.83
    sha: 052be0f94026e6cbc75cdefc9bae13fd6052cdcaf532fa6c45e7ae33a1e6c810
  - name: wasm-bindgen-macro-support
    version: 0.2.83
    sha: 07bc0c051dc5f23e307b13285f9d75df86bfdf816c5721e573dec1f9b8aa193c
  - name: wasm-bindgen-shared
    version: 0.2.83
    sha: 1c38c045535d93ec4f0b4defec448e4291638ee608530863b1e2ba115d4fff7f
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
