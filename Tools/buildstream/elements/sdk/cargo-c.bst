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
  url: https://static.crates.io/crates/cargo-c/cargo-c-0.7.0+cargo-0.49.crate
  ref: e88e3f34e0139e5ee65f741122c875c4755ad1c135fd3447e22223fbb0798f35
- kind: cargo
  ref:
  - name: adler
    version: 0.2.3
    sha: ee2a4ec343196209d6594e19543ae87a39f96d5534d7174822a3ad825dd6ed7e
  - name: aho-corasick
    version: 0.7.15
    sha: 7404febffaa47dac81aa44dba71523c9d069b1bdc50a77db41195149e17f68e5
  - name: ansi_term
    version: 0.11.0
    sha: ee49baf6cb617b853aa8d93bf420db2383fab46d314482ca2803b40d5fde979b
  - name: anyhow
    version: 1.0.36
    sha: 68803225a7b13e47191bab76f2687382b60d259e8cf37f6e1893658b84bb9479
  - name: atty
    version: 0.2.14
    sha: d9b39be18770d11421cdb1b9947a45dd3f37e93092cbf377614828a319d5fee8
  - name: autocfg
    version: 1.0.1
    sha: cdb031dd78e28731d87d56cc8ffef4a8f36ca26c38fe2de700543e627f8a464a
  - name: bitflags
    version: 1.2.1
    sha: cf1de2fe8c75bc145a2f577add951f8134889b4795d47466a54a5c846d691693
  - name: bitmaps
    version: 2.1.0
    sha: 031043d04099746d8db04daf1fa424b2bc8bd69d92b25962dcde24da39ab64a2
  - name: bstr
    version: 0.2.14
    sha: 473fc6b38233f9af7baa94fb5852dca389e3d95b8e21c8e3719301462c5d9faf
  - name: bytesize
    version: 1.0.1
    sha: 81a18687293a1546b67c246452202bbbf143d239cb43494cc163da14979082da
  - name: cargo
    version: 0.49.0
    sha: ec81821e28b074a0b8bc330a446256e7ec451a8be7b7bfad3bd47947f54c6b0b
  - name: cargo-platform
    version: 0.1.1
    sha: 0226944a63d1bf35a3b5f948dd7c59e263db83695c9e8bffc4037de02e30f1d7
  - name: cbindgen
    version: 0.16.0
    sha: 9344318b9c787667b95cd2c5124f5eaf2bde35e959dd01ea04fc5b234c542c11
  - name: cc
    version: 1.0.66
    sha: 4c0496836a84f8d0495758516b8621a622beb77c0fed418570e50764093ced48
  - name: cfg-if
    version: 0.1.10
    sha: 4785bdd1c96b2a846b2bd7cc02e86b6b3dbf14e7e53446c4f54c92a361040822
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: clap
    version: 2.33.3
    sha: 37e58ac78573c40708d45522f0d80fa2f01cc4f9b4e2bf749807255454312002
  - name: commoncrypto
    version: 0.2.0
    sha: d056a8586ba25a1e4d61cb090900e495952c7886786fc55f909ab2f819b69007
  - name: commoncrypto-sys
    version: 0.2.0
    sha: 1fed34f46747aa73dfaa578069fd8279d2818ade2b55f38f22a9401c7f4083e2
  - name: core-foundation
    version: 0.9.1
    sha: 0a89e2ae426ea83155dccf10c0fa6b1463ef6d5fcb44cee0b224a408fa640a62
  - name: core-foundation-sys
    version: 0.8.2
    sha: ea221b5284a47e40033bf9b66f35f984ec0ea2931eb03505246cd27a963f981b
  - name: crates-io
    version: 0.31.1
    sha: 09f977948a46e9edf93eb3dc2d7a8dd4ce3105d36de63300befed37cdf051d4a
  - name: crc32fast
    version: 1.2.1
    sha: 81156fece84ab6a9f2afdb109ce3ae577e42b1228441eded99bd77f627953b1a
  - name: crossbeam-utils
    version: 0.7.2
    sha: c3c7c73a2d1e9fc0886a08b93e98eb643461230d5f1925e4036204d5f2e261a8
  - name: crossbeam-utils
    version: 0.8.1
    sha: 02d96d1e189ef58269ebe5b97953da3274d83a93af647c2ddd6f9dab28cedb8d
  - name: crypto-hash
    version: 0.3.4
    sha: 8a77162240fd97248d19a564a565eb563a3f592b386e4136fb300909e67dddca
  - name: curl
    version: 0.4.34
    sha: e268162af1a5fe89917ae25ba3b0a77c8da752bdc58e7dbb4f15b91fbd33756e
  - name: curl-sys
    version: 0.4.39+curl-7.74.0
    sha: 07a8ce861e7b68a0b394e814d7ee9f1b2750ff8bd10372c6ad3bacc10e86f874
  - name: env_logger
    version: 0.7.1
    sha: 44533bbbb3bb3c1fa17d9f2e4e38bbbaf8396ba82193c4cb1b6445d711445d36
  - name: filetime
    version: 0.2.13
    sha: 0c122a393ea57648015bf06fbd3d372378992e86b9ff5a7a497b076a28c79efe
  - name: flate2
    version: 1.0.19
    sha: 7411863d55df97a419aa64cb4d2f167103ea9d767e2c54a1868b7ac3f6b47129
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
    version: 1.0.0
    sha: ece68d15c92e84fa4f19d3780f1294e5ca82a78a6d515f1efaabcc144688be00
  - name: fwdansi
    version: 1.1.0
    sha: 08c1f5787fe85505d1f7777268db5103d80a7a374d2316a7ce262e57baf8f208
  - name: getrandom
    version: 0.1.15
    sha: fc587bc0ec293155d5bfa6b9891ec18a1e330c234f896ea47fbada4cadbe47e6
  - name: git2
    version: 0.13.13
    sha: 802b535f89475f22bb2b3d589de07861417e67b1a358c7ec8e5005bf4f209133
  - name: git2-curl
    version: 0.14.1
    sha: 883539cb0ea94bab3f8371a98cd8e937bbe9ee7c044499184aa4c17deb643a50
  - name: glob
    version: 0.3.0
    sha: 9b919933a397b79c37e33b77bb2aa3dc8eb6e165ad809e58ff75bc7db2e34574
  - name: globset
    version: 0.4.6
    sha: c152169ef1e421390738366d2f796655fec62621dabbd0fd476f905934061e4a
  - name: hashbrown
    version: 0.9.1
    sha: d7afe4a420e3fe79967a00898cc1f4db7c8a49a9333a29f8a4bd76a253d5cd04
  - name: heck
    version: 0.3.1
    sha: 20564e78d53d2bb135c343b3f47714a56af2061f1c928fdb541dc7b9fdd94205
  - name: hermit-abi
    version: 0.1.17
    sha: 5aca5565f760fb5b220e499d72710ed156fdb74e631659e99377d9ebfbd13ae8
  - name: hex
    version: 0.3.2
    sha: 805026a5d0141ffc30abb3be3173848ad46a1b1664fe632428479619a3644d77
  - name: hex
    version: 0.4.2
    sha: 644f9158b2f133fd50f5fb3242878846d9eb792e445c893805ff0e3824006e35
  - name: home
    version: 0.5.3
    sha: 2456aef2e6b6a9784192ae780c0f15bc57df0e918585282325e8c8ac27737654
  - name: humantime
    version: 1.3.0
    sha: df004cfca50ef23c36850aaaa59ad52cc70d0e90243c3c7737a4dd32dc7a3c4f
  - name: humantime
    version: 2.0.1
    sha: 3c1ad908cc71012b7bea4d0c53ba96a8cba9962f048fa68d143376143d863b7a
  - name: idna
    version: 0.2.0
    sha: 02e2673c30ee86b5b96a9cb52ad15718aa1f966f5ab9ad54a8b95d5ca33120a9
  - name: ignore
    version: 0.4.17
    sha: b287fb45c60bb826a0dc68ff08742b9d88a2fea13d6e0c286b3172065aaf878c
  - name: im-rc
    version: 15.0.0
    sha: 3ca8957e71f04a205cb162508f9326aea04676c8dfd0711220190d6b83664f3f
  - name: indexmap
    version: 1.6.1
    sha: 4fb1fa934250de4de8aef298d81c729a7d33d8c239daa3a7575e6b92bfc7313b
  - name: itoa
    version: 0.4.6
    sha: dc6f3ad7b9d11a0c00842ff8de1b60ee58661048eb8049ed33c73594f359d7e6
  - name: jobserver
    version: 0.1.21
    sha: 5c71313ebb9439f74b00d9d2dcec36440beaf57a6aa0623068441dd7cd81a7f2
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: lazycell
    version: 1.3.0
    sha: 830d08ce1d1d941e6b30645f1a0eb5643013d835ce3779a5fc208261dbe10f55
  - name: libc
    version: 0.2.81
    sha: 1482821306169ec4d07f6aca392a4681f66c75c9918aa49641a2595db64053cb
  - name: libgit2-sys
    version: 0.12.15+1.1.0
    sha: 48c39622f3887bc7fb2fff6d06293f64b02c1dfa281212566ac155171d13b0eb
  - name: libnghttp2-sys
    version: 0.1.5+1.42.0
    sha: 9657455ff47889b70ffd37c3e118e8cdd23fd1f9f3293a285f141070621c4c79
  - name: libssh2-sys
    version: 0.2.20
    sha: df40b13fe7ea1be9b9dffa365a51273816c345fc1811478b57ed7d964fbfc4ce
  - name: libz-sys
    version: 1.1.2
    sha: 602113192b08db8f38796c4e85c39e960c145965140e918018bcde1952429655
  - name: log
    version: 0.4.11
    sha: 4fabed175da42fed1fa0746b0ea71f412aa9d35e76e95e59b192c64b9dc2bf8b
  - name: matches
    version: 0.1.8
    sha: 7ffc5c5338469d4d3ea17d269fa8ea3512ad247247c30bd2df69e68309ed0a08
  - name: memchr
    version: 2.3.4
    sha: 0ee1c47aaa256ecabcaea351eae4a9b01ef39ed810004e298d2511ed284b1525
  - name: miniz_oxide
    version: 0.4.3
    sha: 0f2d26ec3309788e423cfbf68ad1800f061638098d76a83681af979dc4eda19d
  - name: miow
    version: 0.3.6
    sha: 5a33c1b55807fbed163481b5ba66db4b2fa6cde694a5027be10fb724206c5897
  - name: num_cpus
    version: 1.13.0
    sha: 05499f3756671c15885fee9034446956fff3f243d6077b91e5767df161f766b3
  - name: opener
    version: 0.4.1
    sha: 13117407ca9d0caf3a0e74f97b490a7e64c0ae3aa90a8b7085544d0c37b6f3ae
  - name: openssl
    version: 0.10.31
    sha: 8d008f51b1acffa0d3450a68606e6a51c123012edaacb0f4e1426bd978869187
  - name: openssl-probe
    version: 0.1.2
    sha: 77af24da69f9d9341038eba93a073b1fdaaa1b788221b00a69bce9e762cb32de
  - name: openssl-src
    version: 111.13.0+1.1.1i
    sha: 045e4dc48af57aad93d665885789b43222ae26f4886494da12d1ed58d309dcb6
  - name: openssl-sys
    version: 0.9.59
    sha: de52d8eabd217311538a39bba130d7dea1f1e118010fee7a033d966845e7d5fe
  - name: percent-encoding
    version: 2.1.0
    sha: d4fd5641d01c8f18a23da7b6fe29298ff4b55afcccdf78973b24cf3175fee32e
  - name: pkg-config
    version: 0.3.19
    sha: 3831453b3449ceb48b6d9c7ad7c96d5ea673e9b470a1dc578c2ce6521230884c
  - name: ppv-lite86
    version: 0.2.10
    sha: ac74c624d6b2d21f425f752262f42188365d7b8ff1aff74c82e45136510a4857
  - name: proc-macro-error
    version: 1.0.4
    sha: da25490ff9892aab3fcf7c36f08cfb902dd3e71ca0f9f9517bea02a73a5ce38c
  - name: proc-macro-error-attr
    version: 1.0.4
    sha: a1be40180e52ecc98ad80b184934baf3d0d29f979574e439af5a55274b35f869
  - name: proc-macro2
    version: 1.0.24
    sha: 1e0704ee1a7e00d7bb417d0770ea303c1bccbabf0ef1667dae92b5967f5f8a71
  - name: quick-error
    version: 1.2.3
    sha: a1d01941d82fa2ab50be1e79e6714289dd7cde78eba4c074bc5a4374f650dfe0
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
  - name: rand_xoshiro
    version: 0.4.0
    sha: a9fcdd2e881d02f1d9390ae47ad8e5696a9e4be7b547a1da2afbc61973217004
  - name: redox_syscall
    version: 0.1.57
    sha: 41cc0f7e4d5d4544e8861606a285bb08d3e70712ccc7d2b84d7c0ccfaf4b05ce
  - name: regex
    version: 1.4.2
    sha: 38cf2c13ed4745de91a5eb834e11c00bcc3709e773173b2ce4c56c9fbde04b9c
  - name: regex-syntax
    version: 0.6.21
    sha: 3b181ba2dcf07aaccad5448e8ead58db5b742cf85dfe035e2227f137a539a189
  - name: remove_dir_all
    version: 0.5.3
    sha: 3acd125665422973a33ac9d3dd2df85edad0f4ae9b00dafb1a05e43a9f5ef8e7
  - name: rustc-workspace-hack
    version: 1.0.0
    sha: fc71d2faa173b74b232dedc235e3ee1696581bb132fc116fa3626d6151a1a8fb
  - name: rustfix
    version: 0.5.1
    sha: f2c50b74badcddeb8f7652fa8323ce440b95286f8e4b64ebfd871c609672704e
  - name: ryu
    version: 1.0.5
    sha: 71d301d4193d031abdd79ff7e3dd721168a9572ef3fe51a1517aba235bd8f86e
  - name: same-file
    version: 1.0.6
    sha: 93fc1dc3aaa9bfed95e02e6eadabb4baf7e3078b0bd1b4d7b6b0b68378900502
  - name: schannel
    version: 0.1.19
    sha: 8f05ba609c234e60bee0d547fe94a4c7e9da733d1c962cf6e59efa4cd9c8bc75
  - name: semver
    version: 0.10.0
    sha: 394cec28fa623e00903caf7ba4fa6fb9a0e260280bb8cdbbba029611108a0190
  - name: semver-parser
    version: 0.7.0
    sha: 388a1df253eca08550bef6c72392cfe7c30914bf41df5269b68cbd6ff8f570a3
  - name: serde
    version: 1.0.118
    sha: 06c64263859d87aa2eb554587e2d23183398d617427327cf2b3d0ed8c69e4800
  - name: serde_derive
    version: 1.0.118
    sha: c84d3526699cd55261af4b941e4e725444df67aa4f9e6a3564f18030d12672df
  - name: serde_ignored
    version: 0.1.2
    sha: 1c2c7d39d14f2f2ea82239de71594782f186fd03501ac81f0ce08e674819ff2f
  - name: serde_json
    version: 1.0.60
    sha: 1500e84d27fe482ed1dc791a56eddc2f230046a040fa908c08bda1d9fb615779
  - name: shell-escape
    version: 0.1.5
    sha: 45bb67a18fa91266cc7807181f62f9178a6873bfad7dc788c42e6430db40184f
  - name: sized-chunks
    version: 0.6.2
    sha: 1ec31ceca5644fa6d444cc77548b88b67f46db6f7c71683b0f9336e671830d2f
  - name: socket2
    version: 0.3.18
    sha: 97e0e9fd577458a4f61fb91fcb559ea2afecc54c934119421f9f5d3d5b1a1057
  - name: strip-ansi-escapes
    version: 0.1.0
    sha: 9d63676e2abafa709460982ddc02a3bb586b6d15a49b75c212e06edd3933acee
  - name: strsim
    version: 0.8.0
    sha: 8ea5119cdb4c55b55d432abb513a0429384878c15dde60cc77b1c99de1a95a6a
  - name: structopt
    version: 0.3.21
    sha: 5277acd7ee46e63e5168a80734c9f6ee81b1367a7d8772a2d765df2a3705d28c
  - name: structopt-derive
    version: 0.4.14
    sha: 5ba9cdfda491b814720b6b06e0cac513d922fc407582032e8706e9f137976f90
  - name: syn
    version: 1.0.55
    sha: a571a711dddd09019ccc628e1b17fe87c59b09d513c06c026877aa708334f37a
  - name: tar
    version: 0.4.30
    sha: 489997b7557e9a43e192c527face4feacc78bfbe6eed67fd55c4c9e381cba290
  - name: tempfile
    version: 3.1.0
    sha: 7a6e24d9338a0a5be79593e2fa15a648add6138caa803e2d5bc782c371732ca9
  - name: termcolor
    version: 1.1.2
    sha: 2dfed899f0eb03f32ee8c6a0aabdb8a7949659e3466561fc0adf54e26d88c5f4
  - name: textwrap
    version: 0.11.0
    sha: d326610f408c7a4eb6f51c37c330e496b08506c9457c9d34287ecc38809fb060
  - name: thread_local
    version: 1.0.1
    sha: d40c6d1b69745a6ec6fb1ca717914848da4b44ae29d9b3080cbee91d72a69b14
  - name: tinyvec
    version: 1.1.0
    sha: ccf8dbc19eb42fba10e8feaaec282fb50e2c14b2726d6301dbfeed0f73306a6f
  - name: tinyvec_macros
    version: 0.1.0
    sha: cda74da7e1a664f795bb1f8a87ec406fb89a02522cf6e50620d016add6dbbf5c
  - name: toml
    version: 0.5.8
    sha: a31142970826733df8241ef35dc040ef98c679ab14d7c3e54d827099b3acecaa
  - name: typenum
    version: 1.12.0
    sha: 373c8a200f9e67a0c95e62a4f52fbf80c23b4381c05a17845531982fa99e6b33
  - name: unicode-bidi
    version: 0.3.4
    sha: 49f2bd0c6468a8230e1db229cff8029217cf623c767ea5d60bfbd42729ea54d5
  - name: unicode-normalization
    version: 0.1.16
    sha: a13e63ab62dbe32aeee58d1c5408d35c36c392bba5d9d3142287219721afe606
  - name: unicode-segmentation
    version: 1.7.1
    sha: bb0d2e7be6ae3a5fa87eed5fb451aff96f2573d2694942e40543ae0bbe19c796
  - name: unicode-width
    version: 0.1.8
    sha: 9337591893a19b88d8d87f2cec1e73fad5cdfd10e5a6f349f498ad6ea2ffb1e3
  - name: unicode-xid
    version: 0.2.1
    sha: f7fe0bb3479651439c9112f72b6c505038574c9fbb575ed1bf3b797fa39dd564
  - name: url
    version: 2.2.0
    sha: 5909f2b0817350449ed73e8bcd81c8c3c8d9a7a5d8acba4b27db277f1868976e
  - name: utf8parse
    version: 0.1.1
    sha: 8772a4ccbb4e89959023bc5b7cb8623a795caa7092d99f3aa9501b9484d4557d
  - name: vcpkg
    version: 0.2.11
    sha: b00bca6106a5e23f3eee943593759b7fcddb00554332e856d990c893966879fb
  - name: vec_map
    version: 0.8.2
    sha: f1bddf1187be692e79c5ffeab891132dfb0f236ed36a43c7ed39f1165ee20191
  - name: version_check
    version: 0.9.2
    sha: b5a972e5669d67ba988ce3dc826706fb0a8b01471c088cb0b6110b805cc36aed
  - name: vte
    version: 0.3.3
    sha: 4f42f536e22f7fcbb407639765c8fd78707a33109301f834a594758bedd6e8cf
  - name: walkdir
    version: 2.3.1
    sha: 777182bc735b6424e1a57516d35ed72cb8019d85c8c9bf536dccb3445c1a2f7d
  - name: wasi
    version: 0.9.0+wasi-snapshot-preview1
    sha: cccddf32554fecc6acb585f82a32a72e28b48f8c4c1883ddfeeeaa96f7d8e519
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
