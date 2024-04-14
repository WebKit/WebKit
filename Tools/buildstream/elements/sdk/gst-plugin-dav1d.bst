kind: manual

build-depends:
- freedesktop-sdk.bst:components/rust.bst
- freedesktop-sdk.bst:components/pkg-config.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/dav1d.bst
- freedesktop-sdk.bst:components/gstreamer.bst
- freedesktop-sdk.bst:components/gstreamer-plugins-base.bst

environment:
  PATH: /usr/bin:/usr/lib/sdk/rust/bin

config:
  build-commands:
  - |
    rm -f Cargo.toml
    mv dav1d-Cargo.toml video/dav1d/Cargo.toml
    mv Cargo.lock video/dav1d/
    cd video/dav1d/
    cargo --offline build --release

  install-commands:
  - |
    install -D -m a+r -t "%{install-root}%{libdir}/gstreamer-1.0" ./video/dav1d/target/release/libgst*.so

public:
  bst:
    integration-commands:
    - |
      PAGER=this-is-not-a-pager gst-inspect-1.0 dav1ddec

sources:
- kind: git_repo
  url: gitlab_freedesktop_org:gstreamer/gst-plugins-rs.git
  track: main
  ref: gstreamer-1.23.1-158-g168af88edab195505767c92b90e70792b0c83e5c
- kind: local
  path: files/gst-plugin-dav1d/Cargo.lock
- kind: local
  path: files/gst-plugin-dav1d/dav1d-Cargo.toml
- kind: cargo
  ref:
  - name: android-tzdata
    version: 0.1.1
    sha: e999941b234f3131b00bc13c22d06e8c5ff726d1b6318ac7eb276997bbb4fef0
  - name: android_system_properties
    version: 0.1.5
    sha: 819e7219dbd41043ac279b19830f2efc897156490d7fd6ea916720117ee66311
  - name: atomic_refcell
    version: 0.1.13
    sha: 41e67cd8309bbd06cd603a9e693a784ac2e5d1e955f11286e355089fcab3047c
  - name: autocfg
    version: 1.2.0
    sha: f1fdabc7756949593fe60f30ec81974b613357de856987752631dea1e3394c80
  - name: av-data
    version: 0.4.2
    sha: d75b98a3525d00f920df9a2d44cc99b9cc5b7dc70d7fbb612cd755270dbe6552
  - name: bitflags
    version: 2.5.0
    sha: cf4b9d6a944f767f8e5e0db018570623c85f3d925ac718db4e06d0187adb21c1
  - name: bumpalo
    version: 3.16.0
    sha: 79296716171880943b8470b5f8d03aa55eb2e645a4874bdbb28adb49162e012c
  - name: byte-slice-cast
    version: 1.2.2
    sha: c3ac9f8b63eca6fd385229b3675f6cc0dc5c8a5c8a54a59d4f52ffd670d87b0c
  - name: bytes
    version: 1.6.0
    sha: 514de17de45fdb8dc022b1a7975556c53c86f9f0aa5f534b98977b171857c2c9
  - name: cc
    version: 1.0.92
    sha: 2678b2e3449475e95b0aa6f9b506a28e61b3dc8996592b983695e8ebb58a8b41
  - name: cfg-expr
    version: 0.15.8
    sha: d067ad48b8650848b989a59a86c6c36a995d02d2bf778d45c3c5d57bc2718f02
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.37
    sha: 8a0d04d43504c61aa6c7531f1871dd0d418d91130162063b789da00fd7057a5e
  - name: core-foundation-sys
    version: 0.8.6
    sha: 06ea2b9bc92be3c2baa9334a323ebca2d6f074ff852cd1d7b11064035cd3868f
  - name: dav1d
    version: 0.10.3
    sha: 0d4b54a40baf633a71c6f0fb49494a7e4ee7bc26f3e727212b6cb915aa1ea1e1
  - name: dav1d-sys
    version: 0.8.2
    sha: 6ecb1c5e8f4dc438eedc1b534a54672fb0e0a56035dae6b50162787bd2c50e95
  - name: either
    version: 1.11.0
    sha: a47c1c47d2f5964e29c61246e81db715514cd532db6b5116a25ea3c03d6780a2
  - name: equivalent
    version: 1.0.1
    sha: 5443807d6dff69373d433ab9ef5378ad8df50ca6298caf15de6e52e24aaf54d5
  - name: futures-channel
    version: 0.3.30
    sha: eac8f7d7865dcb88bd4373ab671c8cf4508703796caa2b1985a9ca867b3fcb78
  - name: futures-core
    version: 0.3.30
    sha: dfc6580bb841c5a68e9ef15c77ccc837b40a7504914d52e47b8b0e9bbda25a1d
  - name: futures-executor
    version: 0.3.30
    sha: a576fc72ae164fca6b9db127eaa9a9dda0d61316034f33a0a0d4eda41f02b01d
  - name: futures-macro
    version: 0.3.30
    sha: 87750cf4b7a4c0625b1529e4c543c2182106e4dedc60a2a6455e00d212c489ac
  - name: futures-task
    version: 0.3.30
    sha: 38d84fa142264698cdce1a9f9172cf383a0c82de1bddcf3092901442c4097004
  - name: futures-util
    version: 0.3.30
    sha: 3d6401deb83407ab3da39eba7e33987a73c3df0c82b4bb5813ee871c19c41d48
  - name: gio-sys
    version: 0.19.0
    sha: bcf8e1d9219bb294636753d307b030c1e8a032062cba74f493c431a5c8b81ce4
  - name: glib
    version: 0.19.4
    sha: ae1407b2ce171e654720be10d57d4054d3ff2f10a13d5b37e6819b41439832f7
  - name: glib-macros
    version: 0.19.4
    sha: d8bba315e8ce8aa59631545358450f4962557e89b5f7db7442e7153b47037f71
  - name: glib-sys
    version: 0.19.0
    sha: 630f097773d7c7a0bb3258df4e8157b47dc98bbfa0e60ad9ab56174813feced4
  - name: gobject-sys
    version: 0.19.0
    sha: c85e2b1080b9418dd0c58b498da3a5c826030343e0ef07bde6a955d28de54979
  - name: gst-plugin-version-helper
    version: 0.8.2
    sha: 4e5e874f1660252fd2ec81c602066df3633b3a6fcbe2b196f7f93c27cf069b2a
  - name: gstreamer
    version: 0.22.4
    sha: fc6f2d7dcde4b42b5297d25e9f51914cacfa148c99ba6ddabebf006fb2b18c20
  - name: gstreamer-base
    version: 0.22.0
    sha: 514c71195b53c7eced4842b66ca9149833e41cf6a1d949e45e2ca4a4fa929850
  - name: gstreamer-base-sys
    version: 0.22.0
    sha: 286591e0f85bbda1adf9bab6f21d015acd9ca0a4d4acb61da65e3d0487e23c4e
  - name: gstreamer-sys
    version: 0.22.2
    sha: d5ddf526b3bf90ea627224c804f00b8bcb0452e3b447978b4d5092f8e8ff5918
  - name: gstreamer-video
    version: 0.22.4
    sha: 5fc0a96481ecfcdd120d5057bb7ab5a6f6cd392cc34ba1e8b86cac3ba082f788
  - name: gstreamer-video-sys
    version: 0.22.1
    sha: c1ea7996ba44fbbf563aeeda96e24259efc9f06b407854d837ee58e260d7ba78
  - name: hashbrown
    version: 0.14.3
    sha: 290f1a1d9242c78d09ce40a5e87e7554ee637af1351968159f4952f028f75604
  - name: heck
    version: 0.5.0
    sha: 2304e00983f87ffb38b55b444b5e3b60a884b5d30c0fca7d82fe33449bbe55ea
  - name: hermit-abi
    version: 0.3.9
    sha: d231dfb89cfffdbc30e7fc41579ed6066ad03abda9e567ccafae602b97ec5024
  - name: iana-time-zone
    version: 0.1.60
    sha: e7ffbb5a1b541ea2561f8c41c087286cc091e21e556a4f09a8f6cbf17b69b141
  - name: iana-time-zone-haiku
    version: 0.1.2
    sha: f31827a206f56af32e590ba56d5d2d085f558508192593743f16b2306495269f
  - name: indexmap
    version: 2.2.6
    sha: 168fb715dda47215e360912c096649d23d58bf392ac62f73919e831745e40f26
  - name: itertools
    version: 0.12.1
    sha: ba291022dbbd398a455acf126c1e341954079855bc60dfdda641363bd6922569
  - name: js-sys
    version: 0.3.69
    sha: 29c15563dc2726973df627357ce0c9ddddbea194836909d655df6a75d2cf296d
  - name: libc
    version: 0.2.153
    sha: 9c198f91728a82281a64e1f4f9eeb25d82cb32a5de251c6bd1b5154d63a8e7bd
  - name: log
    version: 0.4.21
    sha: 90ed8c1e510134f979dbc4f070f87d4313098b704861a105fe34231c70a3901c
  - name: memchr
    version: 2.7.2
    sha: 6c8640c5d730cb13ebd907d8d04b52f55ac9a2eec55b440c8892f40d56c76c1d
  - name: muldiv
    version: 1.0.1
    sha: 956787520e75e9bd233246045d19f42fb73242759cc57fba9611d940ae96d4b0
  - name: num-bigint
    version: 0.4.4
    sha: 608e7659b5c3d7cba262d894801b9ec9d00de989e8a82bd4bef91d08da45cdc0
  - name: num-derive
    version: 0.4.2
    sha: ed3955f1a9c7c0c15e092f9c887db08b1fc683305fdf6eb6684f22555355e202
  - name: num-integer
    version: 0.1.46
    sha: 7969661fd2958a5cb096e56c8e1ad0444ac2bbcd0061bd28660485a44879858f
  - name: num-rational
    version: 0.4.1
    sha: 0638a1c9d0a3c0914158145bc76cff373a75a627e6ecbfb71cbe6f453a5a19b0
  - name: num-traits
    version: 0.2.18
    sha: da0df0e5185db44f69b44f26786fe401b6c293d1907744beaa7fa62b2e5a517a
  - name: num_cpus
    version: 1.16.0
    sha: 4161fcb6d602d4d2081af7c3a45852d875a03dd337a6bfdd6e06407b61342a43
  - name: once_cell
    version: 1.19.0
    sha: 3fdb12b2476b595f9358c5161aa467c2438859caa136dec86c26fdd2efe17b92
  - name: option-operations
    version: 0.5.0
    sha: 7c26d27bb1aeab65138e4bf7666045169d1717febcc9ff870166be8348b223d0
  - name: paste
    version: 1.0.14
    sha: de3145af08024dea9fa9914f381a17b8fc6034dfb00f3a84013f7ff43f29ed4c
  - name: pin-project-lite
    version: 0.2.14
    sha: bda66fc9667c18cb2758a2ac84d1167245054bcf85d5d1aaa6923f45801bdd02
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.30
    sha: d231b230927b5e4ad203db57bbcbee2802f6bce620b1e4a9024a07d94e2907ec
  - name: proc-macro-crate
    version: 3.1.0
    sha: 6d37c51ca738a55da99dc0c4a34860fd675453b8b36209178c2249bb13651284
  - name: proc-macro2
    version: 1.0.79
    sha: e835ff2298f5721608eb1a980ecaee1aef2c132bf95ecc026a11b7bf3c01c02e
  - name: quote
    version: 1.0.36
    sha: 0fa76aaf39101c457836aec0ce2316dbdc3ab723cdda1c6bd4e6ad4208acaca7
  - name: serde
    version: 1.0.197
    sha: 3fb1c873e1b9b056a4dc4c0c198b24c3ffa059243875552b2bd0933b1aee4ce2
  - name: serde_derive
    version: 1.0.197
    sha: 7eb0b34b42edc17f6b7cac84a52a1c5f0e1bb2227e997ca9011ea3dd34e8610b
  - name: serde_spanned
    version: 0.6.5
    sha: eb3622f419d1296904700073ea6cc23ad690adbd66f13ea683df73298736f0c1
  - name: slab
    version: 0.4.9
    sha: 8f92a496fb766b417c996b9c5e57daf2f7ad3b0bebe1ccfca4856390e3d3bb67
  - name: smallvec
    version: 1.13.2
    sha: 3c5e1a9a646d36c3599cd173a41282daf47c44583ad367b8e6837255952e5c67
  - name: static_assertions
    version: 1.1.0
    sha: a2eb9349b6444b326872e140eb1cf5e7c522154d69e7a0ffb0fb81c06b37543f
  - name: syn
    version: 2.0.58
    sha: 44cfb93f38070beee36b3fef7d4f5a16f27751d94b187b666a5cc5e9b0d30687
  - name: system-deps
    version: 6.2.2
    sha: a3e535eb8dded36d55ec13eddacd30dec501792ff23a0b1682c38601b8cf2349
  - name: target-lexicon
    version: 0.12.14
    sha: e1fc403891a21bcfb7c37834ba66a547a8f402146eba7265b5a6d88059c9ff2f
  - name: thiserror
    version: 1.0.58
    sha: 03468839009160513471e86a034bb2c5c0e4baae3b43f79ffc55c4a5427b3297
  - name: thiserror-impl
    version: 1.0.58
    sha: c61f3ba182994efc43764a46c018c347bc492c79f024e705f46567b418f6d4f7
  - name: toml
    version: 0.8.12
    sha: e9dd1545e8208b4a5af1aa9bbd0b4cf7e9ea08fabc5d0a5c67fcaafa17433aa3
  - name: toml_datetime
    version: 0.6.5
    sha: 3550f4e9685620ac18a50ed434eb3aec30db8ba93b0287467bca5826ea25baf1
  - name: toml_edit
    version: 0.21.1
    sha: 6a8534fd7f78b5405e860340ad6575217ce99f38d4d5c8f2442cb5ecb50090e1
  - name: toml_edit
    version: 0.22.9
    sha: 8e40bb779c5187258fd7aad0eb68cb8706a0a81fa712fbea808ab43c4b8374c4
  - name: unicode-ident
    version: 1.0.12
    sha: 3354b9ac3fae1ff6755cb6db53683adb661634f67557942dea4facebec0fee4b
  - name: version-compare
    version: 0.2.0
    sha: 852e951cb7832cb45cb1169900d19760cfa39b82bc0ea9c0e5a14ae88411c98b
  - name: wasm-bindgen
    version: 0.2.92
    sha: 4be2531df63900aeb2bca0daaaddec08491ee64ceecbee5076636a3b026795a8
  - name: wasm-bindgen-backend
    version: 0.2.92
    sha: 614d787b966d3989fa7bb98a654e369c762374fd3213d212cfc0251257e747da
  - name: wasm-bindgen-macro
    version: 0.2.92
    sha: a1f8823de937b71b9460c0c34e25f3da88250760bec0ebac694b49997550d726
  - name: wasm-bindgen-macro-support
    version: 0.2.92
    sha: e94f17b526d0a461a191c78ea52bbce64071ed5c04c9ffe424dcb38f74171bb7
  - name: wasm-bindgen-shared
    version: 0.2.92
    sha: af190c94f2773fdb3729c55b007a722abb5384da03bc0986df4c289bf5567e96
  - name: windows-core
    version: 0.52.0
    sha: 33ab640c8d7e35bf8ba19b884ba838ceb4fba93a4e8c65a9059d08afcfc683d9
  - name: windows-sys
    version: 0.52.0
    sha: 282be5f36a8ce781fad8c8ae18fa3f9beff57ec1b52cb3de0789201425d9a33d
  - name: windows-targets
    version: 0.52.5
    sha: 6f0713a46559409d202e70e28227288446bf7841d3211583a4b53e3f6d96e7eb
  - name: windows_aarch64_gnullvm
    version: 0.52.5
    sha: 7088eed71e8b8dda258ecc8bac5fb1153c5cffaf2578fc8ff5d61e23578d3263
  - name: windows_aarch64_msvc
    version: 0.52.5
    sha: 9985fd1504e250c615ca5f281c3f7a6da76213ebd5ccc9561496568a2752afb6
  - name: windows_i686_gnu
    version: 0.52.5
    sha: 88ba073cf16d5372720ec942a8ccbf61626074c6d4dd2e745299726ce8b89670
  - name: windows_i686_gnullvm
    version: 0.52.5
    sha: 87f4261229030a858f36b459e748ae97545d6f1ec60e5e0d6a3d32e0dc232ee9
  - name: windows_i686_msvc
    version: 0.52.5
    sha: db3c2bf3d13d5b658be73463284eaf12830ac9a26a90c717b7f771dfe97487bf
  - name: windows_x86_64_gnu
    version: 0.52.5
    sha: 4e4246f76bdeff09eb48875a0fd3e2af6aada79d409d33011886d3e1581517d9
  - name: windows_x86_64_gnullvm
    version: 0.52.5
    sha: 852298e482cd67c356ddd9570386e2862b5673c85bd5f88df9ab6802b334c596
  - name: windows_x86_64_msvc
    version: 0.52.5
    sha: bec47e5bfd1bff0eeaf6d8b485cc1074891a197ab4225d504cb7a1ab88b02bf0
  - name: winnow
    version: 0.5.40
    sha: f593a95398737aeed53e489c785df13f3618e41dbcd6718c6addbf1395aa6876
  - name: winnow
    version: 0.6.6
    sha: f0c976aaaa0e1f90dbb21e9587cdaf1d9679a1cde8875c0d6bd83ab96a208352
