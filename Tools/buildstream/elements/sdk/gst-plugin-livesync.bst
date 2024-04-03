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
      PAGER=this-is-not-a-pager gst-inspect-1.0 livesync

sources:
- kind: tar
  url: crates:crates/gst-plugin-livesync/gst-plugin-livesync-0.11.2.crate
  ref: 192db10174313282962d730c01c45d072606a8b5b3c62987cf2e9335c4452d21
- kind: local
  path: files/gst-plugin-livesync/Cargo.lock
- kind: cargo
  ref:
  - name: android-tzdata
    version: 0.1.1
    sha: e999941b234f3131b00bc13c22d06e8c5ff726d1b6318ac7eb276997bbb4fef0
  - name: android_system_properties
    version: 0.1.5
    sha: 819e7219dbd41043ac279b19830f2efc897156490d7fd6ea916720117ee66311
  - name: anyhow
    version: 1.0.79
    sha: 080e9890a082662b09c1ad45f567faeeb47f22b5fb23895fbe1e651e718e25ca
  - name: async-channel
    version: 2.1.1
    sha: 1ca33f4bc4ed1babef42cad36cc1f51fa88be00420404e5b1e80ab1b18f7678c
  - name: atomic_refcell
    version: 0.1.13
    sha: 41e67cd8309bbd06cd603a9e693a784ac2e5d1e955f11286e355089fcab3047c
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: bitflags
    version: 1.3.2
    sha: bef38d45163c2f1dde094a7dfd33ccf595c92905c8f8f4fdc18d06fb1037718a
  - name: bitflags
    version: 2.4.1
    sha: 327762f6e5a765692301e5bb513e0d9fef63be86bbc14528052b1cd3e6f03e07
  - name: bumpalo
    version: 3.14.0
    sha: 7f30e7476521f6f8af1a1c4c0b8cc94f0bee37d91763d0ca2665f299b6cd8aec
  - name: cairo-rs
    version: 0.18.5
    sha: 8ca26ef0159422fb77631dc9d17b102f253b876fe1586b03b803e63a309b4ee2
  - name: cairo-sys-rs
    version: 0.18.2
    sha: 685c9fa8e590b8b3d678873528d83411db17242a73fccaed827770ea0fedda51
  - name: cc
    version: 1.0.83
    sha: f1174fb0b6ec23863f8b971027804a42614e347eafb0a95bf0b12cdae21fc4d0
  - name: cfg-expr
    version: 0.15.6
    sha: 6100bc57b6209840798d95cb2775684849d332f7bd788db2a8c8caf7ef82a41a
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.31
    sha: 7f2c685bad3eb3d45a01354cedb7d5faa66194d1d58ba6e267a8de788f79db38
  - name: concurrent-queue
    version: 2.4.0
    sha: d16048cd947b08fa32c24458a22f5dc5e835264f689f4f5653210c69fd107363
  - name: core-foundation-sys
    version: 0.8.6
    sha: 06ea2b9bc92be3c2baa9334a323ebca2d6f074ff852cd1d7b11064035cd3868f
  - name: crossbeam-utils
    version: 0.8.19
    sha: 248e3bacc7dc6baa3b21e405ee045c3047101a49145e7e9eca583ab4c2ca5345
  - name: either
    version: 1.9.0
    sha: a26ae43d7bcc3b814de94796a5e736d4029efb0ee900c12e2d54c993ad1a1e07
  - name: equivalent
    version: 1.0.1
    sha: 5443807d6dff69373d433ab9ef5378ad8df50ca6298caf15de6e52e24aaf54d5
  - name: event-listener
    version: 4.0.3
    sha: 67b215c49b2b248c855fb73579eb1f4f26c38ffdc12973e20e07b91d78d5646e
  - name: event-listener-strategy
    version: 0.4.0
    sha: 958e4d70b6d5e81971bebec42271ec641e7ff4e170a6fa605f2b8a8b65cb97d3
  - name: field-offset
    version: 0.3.6
    sha: 38e2275cc4e4fc009b0669731a1e5ab7ebf11f469eaede2bab9309a5b4d6057f
  - name: futures-channel
    version: 0.3.30
    sha: eac8f7d7865dcb88bd4373ab671c8cf4508703796caa2b1985a9ca867b3fcb78
  - name: futures-core
    version: 0.3.30
    sha: dfc6580bb841c5a68e9ef15c77ccc837b40a7504914d52e47b8b0e9bbda25a1d
  - name: futures-executor
    version: 0.3.30
    sha: a576fc72ae164fca6b9db127eaa9a9dda0d61316034f33a0a0d4eda41f02b01d
  - name: futures-io
    version: 0.3.30
    sha: a44623e20b9681a318efdd71c299b6b222ed6f231972bfe2f224ebad6311f0c1
  - name: futures-macro
    version: 0.3.30
    sha: 87750cf4b7a4c0625b1529e4c543c2182106e4dedc60a2a6455e00d212c489ac
  - name: futures-task
    version: 0.3.30
    sha: 38d84fa142264698cdce1a9f9172cf383a0c82de1bddcf3092901442c4097004
  - name: futures-util
    version: 0.3.30
    sha: 3d6401deb83407ab3da39eba7e33987a73c3df0c82b4bb5813ee871c19c41d48
  - name: gdk-pixbuf
    version: 0.18.5
    sha: 50e1f5f1b0bfb830d6ccc8066d18db35c487b1b2b1e8589b5dfe9f07e8defaec
  - name: gdk-pixbuf-sys
    version: 0.18.0
    sha: 3f9839ea644ed9c97a34d129ad56d38a25e6756f99f3a88e15cd39c20629caf7
  - name: gdk4
    version: 0.7.3
    sha: 7edb019ad581f8ecf8ea8e4baa6df7c483a95b5a59be3140be6a9c3b0c632af6
  - name: gdk4-sys
    version: 0.7.2
    sha: dbab43f332a3cf1df9974da690b5bb0e26720ed09a228178ce52175372dcfef0
  - name: gdk4-win32
    version: 0.7.2
    sha: fe66650c41f64e41ada9f85c4bdb96e5549c669e594892240dfb147a63256057
  - name: gdk4-win32-sys
    version: 0.7.2
    sha: 4b5c187546cd3ad9f1787b46c66272d8fcdf8197bfe4f6e2647fe910c39e396d
  - name: gio
    version: 0.18.4
    sha: d4fc8f532f87b79cbc51a79748f16a6828fb784be93145a322fa14d06d354c73
  - name: gio-sys
    version: 0.18.1
    sha: 37566df850baf5e4cb0dfb78af2e4b9898d817ed9263d1090a2df958c64737d2
  - name: glib
    version: 0.18.5
    sha: 233daaf6e83ae6a12a52055f568f9d7cf4671dabb78ff9560ab6da230ce00ee5
  - name: glib-macros
    version: 0.18.5
    sha: 0bb0228f477c0900c880fd78c8759b95c7636dbd7842707f49e132378aa2acdc
  - name: glib-sys
    version: 0.18.1
    sha: 063ce2eb6a8d0ea93d2bf8ba1957e78dbab6be1c2220dd3daca57d5a9d869898
  - name: gobject-sys
    version: 0.18.0
    sha: 0850127b514d1c4a4654ead6dedadb18198999985908e6ffe4436f53c785ce44
  - name: graphene-rs
    version: 0.18.1
    sha: 3b2228cda1505613a7a956cca69076892cfbda84fc2b7a62b94a41a272c0c401
  - name: graphene-sys
    version: 0.18.1
    sha: cc4144cee8fc8788f2a9b73dc5f1d4e1189d1f95305c4cb7bd9c1af1cfa31f59
  - name: gsk4
    version: 0.7.3
    sha: 0d958e351d2f210309b32d081c832d7de0aca0b077aa10d88336c6379bd01f7e
  - name: gsk4-sys
    version: 0.7.3
    sha: 12bd9e3effea989f020e8f1ff3fa3b8c63ba93d43b899c11a118868853a56d55
  - name: gst-plugin-gtk4
    version: 0.11.3
    sha: 551340110f16646449f27453598c22d7ab6b59960380175be097056dc00b2b7d
  - name: gst-plugin-version-helper
    version: 0.8.0
    sha: 334c37a15ca23861ad8159c8ff6e20f011aa2000defc5c9159096a28842f811b
  - name: gstreamer
    version: 0.21.3
    sha: de95703f4c8e79f4f4e42279cf1ab0e5a46b7ece4a9dfcd16424164af7be9055
  - name: gstreamer-audio
    version: 0.21.3
    sha: 7a97c8a4e9c889b4a7b2feda4be70edc5c3e012ddbf97928cf0a020ca1e4212d
  - name: gstreamer-audio-sys
    version: 0.21.1
    sha: 78bd94ae8b177377855b38c3d809c686526786cdb771e6d68510509634b955d1
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
  - name: gstreamer-gl
    version: 0.21.2
    sha: 3ecfb91128263c160448a915a15e430cfdc69317b1b087316222e0693bb51b90
  - name: gstreamer-gl-sys
    version: 0.21.2
    sha: d580971b3c99a667c9739812d499e6c5cadbb92873f984cd0d1d0b4e7346f1cd
  - name: gstreamer-sys
    version: 0.21.2
    sha: 564cda782b3e6eed1b81cb4798a06794db56440fb05b422505be689f34ce3bc4
  - name: gstreamer-video
    version: 0.21.2
    sha: e85b2a4d1d3b7a98ae03806c3ed5c2db89d6b37a5f138780b48de015d68715e5
  - name: gstreamer-video-sys
    version: 0.21.2
    sha: 0302318d98e6b054501e485b6bb4ee20225823218f4a8660c182f115a33b16ee
  - name: gtk4
    version: 0.7.3
    sha: 5aeb51aa3e9728575a053e1f43543cd9992ac2477e1b186ad824fd4adfb70842
  - name: gtk4-macros
    version: 0.7.2
    sha: d57ec49cf9b657f69a05bca8027cff0a8dfd0c49e812be026fc7311f2163832f
  - name: gtk4-sys
    version: 0.7.3
    sha: 54d8c4aa23638ce9faa2caf7e2a27d4a1295af2155c8e8d28c4d4eeca7a65eb8
  - name: hashbrown
    version: 0.14.3
    sha: 290f1a1d9242c78d09ce40a5e87e7554ee637af1351968159f4952f028f75604
  - name: heck
    version: 0.4.1
    sha: 95505c38b4572b2d910cecb0281560f54b440a19336cbbcb27bf6ce6adc6f5a8
  - name: iana-time-zone
    version: 0.1.59
    sha: b6a67363e2aa4443928ce15e57ebae94fd8949958fd1223c4cfc0cd473ad7539
  - name: iana-time-zone-haiku
    version: 0.1.2
    sha: f31827a206f56af32e590ba56d5d2d085f558508192593743f16b2306495269f
  - name: indexmap
    version: 2.1.0
    sha: d530e1a18b1cb4c484e6e34556a0d948706958449fca0cab753d649f2bce3d1f
  - name: itertools
    version: 0.12.0
    sha: 25db6b064527c5d482d0423354fcd07a89a2dfe07b67892e62411946db7f07b0
  - name: js-sys
    version: 0.3.67
    sha: 9a1d36f1235bc969acba30b7f5990b864423a6068a10f7c90ae8f0112e3a59d1
  - name: libc
    version: 0.2.152
    sha: 13e3bf6590cbc649f4d1a3eefc9d5d6eb746f5200ffb04e5e142700b8faa56e7
  - name: lock_api
    version: 0.4.11
    sha: 3c168f8615b12bc01f9c17e2eb0cc07dcae1940121185446edc3744920e8ef45
  - name: log
    version: 0.4.20
    sha: b5e6163cb8c49088c2c36f57875e58ccd8c87c7427f7fbd50ea6710b2f3f2e8f
  - name: memchr
    version: 2.7.1
    sha: 523dc4f511e55ab87b694dc30d0f820d60906ef06413f93d4d7a1385599cc149
  - name: memoffset
    version: 0.9.0
    sha: 5a634b1c61a95585bd15607c6ab0c4e5b226e695ff2800ba0cdccddf208c406c
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
    version: 0.2.17
    sha: 39e3200413f237f41ab11ad6d161bc7239c84dcb631773ccd7de3dfe4b5c267c
  - name: once_cell
    version: 1.19.0
    sha: 3fdb12b2476b595f9358c5161aa467c2438859caa136dec86c26fdd2efe17b92
  - name: option-operations
    version: 0.5.0
    sha: 7c26d27bb1aeab65138e4bf7666045169d1717febcc9ff870166be8348b223d0
  - name: pango
    version: 0.18.3
    sha: 7ca27ec1eb0457ab26f3036ea52229edbdb74dee1edd29063f5b9b010e7ebee4
  - name: pango-sys
    version: 0.18.0
    sha: 436737e391a843e5933d6d9aa102cb126d501e815b83601365a948a518555dc5
  - name: parking
    version: 2.2.0
    sha: bb813b8af86854136c6922af0598d719255ecb2179515e6e7730d468f05c9cae
  - name: parking_lot
    version: 0.12.1
    sha: 3742b2c103b9f06bc9fff0a37ff4912935851bee6d36f3c02bcc755bcfec228f
  - name: parking_lot_core
    version: 0.9.9
    sha: 4c42a9226546d68acdd9c0a280d17ce19bfe27a46bf68784e4066115788d008e
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
    version: 0.3.28
    sha: 69d3587f8a9e599cc7ec2c00e331f71c4e69a5f9a4b8a6efd5b07466b9736f9a
  - name: pretty-hex
    version: 0.4.1
    sha: bbc83ee4a840062f368f9096d80077a9841ec117e17e7f700df81958f1451254
  - name: proc-macro-crate
    version: 1.3.1
    sha: 7f4c021e1093a56626774e81216a4ce732a735e5bad4868a03f3ed65ca0c3919
  - name: proc-macro-crate
    version: 2.0.1
    sha: 97dc5fea232fc28d2f597b37c4876b348a40e33f3b02cc975c8d006d78d94b1a
  - name: proc-macro-error
    version: 1.0.4
    sha: da25490ff9892aab3fcf7c36f08cfb902dd3e71ca0f9f9517bea02a73a5ce38c
  - name: proc-macro-error-attr
    version: 1.0.4
    sha: a1be40180e52ecc98ad80b184934baf3d0d29f979574e439af5a55274b35f869
  - name: proc-macro2
    version: 1.0.76
    sha: 95fc56cda0b5c3325f5fbbd7ff9fda9e02bb00bb3dac51252d2f1bfa1cb8cc8c
  - name: quote
    version: 1.0.35
    sha: 291ec9ab5efd934aaf503a6466c5d5251535d108ee747472c3977cc5acc868ef
  - name: redox_syscall
    version: 0.4.1
    sha: 4722d768eff46b75989dd134e5c353f0d6296e5aaa3132e776cbdb56be7731aa
  - name: rustc_version
    version: 0.4.0
    sha: bfa0f585226d2e68097d4f95d113b15b83a82e819ab25717ec0590d9584ef366
  - name: scopeguard
    version: 1.2.0
    sha: 94143f37725109f92c262ed2cf5e59bce7498c01bcc1502d7b9afe439a4e9f49
  - name: semver
    version: 1.0.21
    sha: b97ed7a9823b74f99c7742f5336af7be5ecd3eeafcb1507d1fa93347b1d589b0
  - name: serde
    version: 1.0.195
    sha: 63261df402c67811e9ac6def069e4786148c4563f4b50fd4bf30aa370d626b02
  - name: serde_derive
    version: 1.0.195
    sha: 46fe8f8603d81ba86327b23a2e9cdf49e1255fb94a4c5f297f6ee0547178ea2c
  - name: serde_spanned
    version: 0.6.5
    sha: eb3622f419d1296904700073ea6cc23ad690adbd66f13ea683df73298736f0c1
  - name: slab
    version: 0.4.9
    sha: 8f92a496fb766b417c996b9c5e57daf2f7ad3b0bebe1ccfca4856390e3d3bb67
  - name: smallvec
    version: 1.12.0
    sha: 2593d31f82ead8df961d8bd23a64c2ccf2eb5dd34b0a34bfb4dd54011c72009e
  - name: syn
    version: 1.0.109
    sha: 72b64191b275b66ffe2469e8af2c1cfe3bafa67b529ead792a6d0160888b4237
  - name: syn
    version: 2.0.48
    sha: 0f3531638e407dfc0814761abb7c00a5b54992b849452a0646b7f65c9f770f3f
  - name: system-deps
    version: 6.2.0
    sha: 2a2d580ff6a20c55dfb86be5f9c238f67835d0e81cbdea8bf5680e0897320331
  - name: target-lexicon
    version: 0.12.13
    sha: 69758bda2e78f098e4ccb393021a0963bb3442eac05f135c30f61b7370bbafae
  - name: thiserror
    version: 1.0.56
    sha: d54378c645627613241d077a3a79db965db602882668f9136ac42af9ecb730ad
  - name: thiserror-impl
    version: 1.0.56
    sha: fa0faa943b50f3db30a20aa7e265dbc66076993efed8463e8de414e5d06d3471
  - name: toml
    version: 0.8.2
    sha: 185d8ab0dfbb35cf1399a6344d8484209c088f75f8f68230da55d48d95d43e3d
  - name: toml_datetime
    version: 0.6.3
    sha: 7cda73e2f1397b1262d6dfdcef8aafae14d1de7748d66822d3bfeeb6d03e5e4b
  - name: toml_edit
    version: 0.19.15
    sha: 1b5bb770da30e5cbfde35a2d7b9b8a2c4b8ef89548a7a6aeab5c9a576e3e7421
  - name: toml_edit
    version: 0.20.2
    sha: 396e4d48bbb2b7554c944bde63101b5ae446cff6ec4a24227428f15eb72ef338
  - name: unicode-ident
    version: 1.0.12
    sha: 3354b9ac3fae1ff6755cb6db53683adb661634f67557942dea4facebec0fee4b
  - name: version-compare
    version: 0.1.1
    sha: 579a42fc0b8e0c63b76519a339be31bed574929511fa53c1a3acae26eb258f29
  - name: version_check
    version: 0.9.4
    sha: 49874b5167b65d7193b8aba1567f5c7d93d001cafc34600cee003eda787e483f
  - name: wasm-bindgen
    version: 0.2.90
    sha: b1223296a201415c7fad14792dbefaace9bd52b62d33453ade1c5b5f07555406
  - name: wasm-bindgen-backend
    version: 0.2.90
    sha: fcdc935b63408d58a32f8cc9738a0bffd8f05cc7c002086c6ef20b7312ad9dcd
  - name: wasm-bindgen-macro
    version: 0.2.90
    sha: 3e4c238561b2d428924c49815533a8b9121c664599558a5d9ec51f8a1740a999
  - name: wasm-bindgen-macro-support
    version: 0.2.90
    sha: bae1abb6806dc1ad9e560ed242107c0f6c84335f1749dd4e8ddb012ebd5e25a7
  - name: wasm-bindgen-shared
    version: 0.2.90
    sha: 4d91413b1c31d7539ba5ef2451af3f0b833a005eb27a631cec32bc0635a8602b
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
    version: 0.52.0
    sha: 33ab640c8d7e35bf8ba19b884ba838ceb4fba93a4e8c65a9059d08afcfc683d9
  - name: windows-sys
    version: 0.52.0
    sha: 282be5f36a8ce781fad8c8ae18fa3f9beff57ec1b52cb3de0789201425d9a33d
  - name: windows-targets
    version: 0.48.5
    sha: 9a2fa6e2155d7247be68c096456083145c183cbbbc2764150dda45a87197940c
  - name: windows-targets
    version: 0.52.0
    sha: 8a18201040b24831fbb9e4eb208f8892e1f50a37feb53cc7ff887feb8f50e7cd
  - name: windows_aarch64_gnullvm
    version: 0.48.5
    sha: 2b38e32f0abccf9987a4e3079dfb67dcd799fb61361e53e2882c3cbaf0d905d8
  - name: windows_aarch64_gnullvm
    version: 0.52.0
    sha: cb7764e35d4db8a7921e09562a0304bf2f93e0a51bfccee0bd0bb0b666b015ea
  - name: windows_aarch64_msvc
    version: 0.48.5
    sha: dc35310971f3b2dbbf3f0690a219f40e2d9afcf64f9ab7cc1be722937c26b4bc
  - name: windows_aarch64_msvc
    version: 0.52.0
    sha: bbaa0368d4f1d2aaefc55b6fcfee13f41544ddf36801e793edbbfd7d7df075ef
  - name: windows_i686_gnu
    version: 0.48.5
    sha: a75915e7def60c94dcef72200b9a8e58e5091744960da64ec734a6c6e9b3743e
  - name: windows_i686_gnu
    version: 0.52.0
    sha: a28637cb1fa3560a16915793afb20081aba2c92ee8af57b4d5f28e4b3e7df313
  - name: windows_i686_msvc
    version: 0.48.5
    sha: 8f55c233f70c4b27f66c523580f78f1004e8b5a8b659e05a4eb49d4166cca406
  - name: windows_i686_msvc
    version: 0.52.0
    sha: ffe5e8e31046ce6230cc7215707b816e339ff4d4d67c65dffa206fd0f7aa7b9a
  - name: windows_x86_64_gnu
    version: 0.48.5
    sha: 53d40abd2583d23e4718fddf1ebec84dbff8381c07cae67ff7768bbf19c6718e
  - name: windows_x86_64_gnu
    version: 0.52.0
    sha: 3d6fa32db2bc4a2f5abeacf2b69f7992cd09dca97498da74a151a3132c26befd
  - name: windows_x86_64_gnullvm
    version: 0.48.5
    sha: 0b7b52767868a23d5bab768e390dc5f5c55825b6d30b86c844ff2dc7414044cc
  - name: windows_x86_64_gnullvm
    version: 0.52.0
    sha: 1a657e1e9d3f514745a572a6846d3c7aa7dbe1658c056ed9c3344c4109a6949e
  - name: windows_x86_64_msvc
    version: 0.48.5
    sha: ed94fce61571a4006852b7389a063ab983c02eb1bb37b47f8272ce92d06d9538
  - name: windows_x86_64_msvc
    version: 0.52.0
    sha: dff9641d1cd4be8d1a070daf9e3773c5f67e78b4d9d42263020c057706765c04
  - name: winnow
    version: 0.5.34
    sha: b7cf47b659b318dccbd69cc4797a39ae128f533dce7902a1096044d1967b9c16
