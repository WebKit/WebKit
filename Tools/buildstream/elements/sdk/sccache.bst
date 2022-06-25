kind: manual

build-depends:
- freedesktop-sdk.bst:components/rust.bst
- freedesktop-sdk.bst:components/pkg-config.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libffi.bst

environment:
  PATH: /usr/bin:/usr/lib/sdk/rust/bin

config:
  build-commands:
  - |
    cargo --offline build --features="all" --release
    strip ./target/release/sccache

  install-commands:
  - |
    install -D -m a+rx -t "%{install-root}%{bindir}" ./target/release/sccache 

variables:
  strip-binaries: |
    true

sources:
- kind: git_tag
  url: github_com:mozilla/sccache
  track: v0.2.15
  ref: v0.2.15-0-g6b6d2f7d2dceefeb4f583712aa4c221db62be0bd
- kind: cargo
  ref:
  - name: addr2line
    version: 0.14.1
    sha: a55f82cfe485775d02112886f4169bde0c5894d75e79ead7eafe7e40a25e45f7
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
    version: 1.0.37
    sha: ee67c11feeac938fae061b232e38e0b6d94f97a9df10e6271319325ac4c56a86
  - name: ar
    version: 0.8.0
    sha: 450575f58f7bee32816abbff470cbc47797397c2a81e0eaced4b98436daf52e1
  - name: arrayref
    version: 0.3.6
    sha: a4c527152e37cf757a3f78aae5a06fbeefdb07ccc535c980a3208ee3060dd544
  - name: arrayvec
    version: 0.5.2
    sha: 23b62fc65de8e4e7f52534fb52b0f3ed04746ae267519eef2a83941e8085068b
  - name: ascii
    version: 0.8.7
    sha: 97be891acc47ca214468e09425d02cef3af2c94d0d82081cd02061f996802f14
  - name: assert_cmd
    version: 1.0.2
    sha: 3dc1679af9a1ab4bea16f228b05d18f8363f8327b1fa8db00d2760cfafc6b61e
  - name: async-channel
    version: 1.5.1
    sha: 59740d83946db6a5af71ae25ddf9562c2b176b2ca42cf99a455f09f4a220d6b9
  - name: async-executor
    version: 1.4.0
    sha: eb877970c7b440ead138f6321a3b5395d6061183af779340b65e20c0fede9146
  - name: async-global-executor
    version: 1.4.3
    sha: 73079b49cd26b8fd5a15f68fc7707fc78698dc2a3d61430f2a7a9430230dfa04
  - name: async-io
    version: 1.3.1
    sha: 9315f8f07556761c3e48fec2e6b276004acf426e6dc068b2c2251854d65ee0fd
  - name: async-mutex
    version: 1.4.0
    sha: 479db852db25d9dbf6204e6cb6253698f175c15726470f78af0d918e99d6156e
  - name: async-std
    version: 1.8.0
    sha: 8f9f84f1280a2b436a2c77c2582602732b6c2f4321d5494d6e799e6c367859a8
  - name: async-task
    version: 4.0.3
    sha: e91831deabf0d6d7ec49552e489aed63b7456a7a3c46cff62adad428110b0af0
  - name: async-trait
    version: 0.1.42
    sha: 8d3a45e77e34375a7923b1e8febb049bb011f064714a8e17a1a616fef01da13d
  - name: atomic-waker
    version: 1.0.0
    sha: 065374052e7df7ee4047b1160cca5e1467a12351a40b3da123c870ba0b8eda2a
  - name: atty
    version: 0.2.14
    sha: d9b39be18770d11421cdb1b9947a45dd3f37e93092cbf377614828a319d5fee8
  - name: autocfg
    version: 0.1.7
    sha: 1d49d90015b3c36167a20fe2810c5cd875ad504b39cff3d4eae7977e6b7c1cb2
  - name: autocfg
    version: 1.0.1
    sha: cdb031dd78e28731d87d56cc8ffef4a8f36ca26c38fe2de700543e627f8a464a
  - name: backtrace
    version: 0.3.55
    sha: ef5140344c85b01f9bbb4d4b7288a8aa4b3287ccef913a14bcc78a1063623598
  - name: base64
    version: 0.10.1
    sha: 0b25d992356d2eb0ed82172f5248873db5560c4721f564b13cb5193bda5e668e
  - name: base64
    version: 0.12.3
    sha: 3441f0f7b02788e948e47f457ca01f1d7e6d92c693bc132c22b087d3141c03ff
  - name: base64
    version: 0.13.0
    sha: 904dfeac50f3cdaba28fc6f57fdcddb75f49ed61346676a78c4ffe55877802fd
  - name: base64
    version: 0.9.3
    sha: 489d6c0ed21b11d038c31b6ceccca973e65d73ba3bd8ecb9a2babf5546164643
  - name: bincode
    version: 0.8.0
    sha: e103c8b299b28a9c6990458b7013dc4a8356a9b854c51b9883241f5866fac36e
  - name: bincode
    version: 1.3.1
    sha: f30d3a39baa26f9651f17b375061f3233dde33424a8b72b0dbe93a68a0bc896d
  - name: bitflags
    version: 1.2.1
    sha: cf1de2fe8c75bc145a2f577add951f8134889b4795d47466a54a5c846d691693
  - name: blake2b_simd
    version: 0.5.11
    sha: afa748e348ad3be8263be728124b24a24f268266f6f5d58af9d75f6a40b5c587
  - name: blake3
    version: 0.3.7
    sha: e9ff35b701f3914bdb8fad3368d822c766ef2858b2583198e41639b936f09d3f
  - name: block-buffer
    version: 0.9.0
    sha: 4152116fd6e9dadb291ae18fc1ec3575ed6d84c29642d97890f4b4a3417297e4
  - name: blocking
    version: 1.0.2
    sha: c5e170dbede1f740736619b776d7251cb1b9095c435c34d8ca9f57fcd2f335e9
  - name: boxfnonce
    version: 0.1.1
    sha: 5988cb1d626264ac94100be357308f29ff7cbdd3b36bda27f450a4ee3f713426
  - name: buf_redux
    version: 0.8.4
    sha: b953a6887648bb07a535631f2bc00fbdb2a2216f135552cb3f534ed136b9c07f
  - name: bufstream
    version: 0.1.4
    sha: 40e38929add23cdf8a366df9b0e088953150724bcbe5fc330b0d8eb3b328eec8
  - name: bumpalo
    version: 3.4.0
    sha: 2e8c087f005730276d1096a652e92a8bacee2e2472bcc9715a74d2bec38b5820
  - name: byteorder
    version: 1.3.4
    sha: 08c48aae112d48ed9f069b33538ea9e3e90aa263cfa3d1c24309612b1f7472de
  - name: bytes
    version: 0.4.12
    sha: 206fdffcfa2df7cbe15601ef46c813fce0965eb3286db6b56c583b814b51c81c
  - name: bytes
    version: 0.5.6
    sha: 0e4cec68f03f32e44924783795810fa50a7035d8c8ebe78580ad7e6c703fba38
  - name: bytes
    version: 1.0.0
    sha: ad1f8e949d755f9d79112b5bb46938e0ef9d3804a0b16dfab13aafcaa5f0fa72
  - name: cache-padded
    version: 1.1.1
    sha: 631ae5198c9be5e753e5cc215e1bd73c2b466a3565173db433f52bb9d3e66dba
  - name: case
    version: 0.1.0
    sha: e88b166b48e29667f5443df64df3c61dc07dc2b1a0b0d231800e07f09a33ecc1
  - name: cc
    version: 1.0.66
    sha: 4c0496836a84f8d0495758516b8621a622beb77c0fed418570e50764093ced48
  - name: cfg-if
    version: 0.1.10
    sha: 4785bdd1c96b2a846b2bd7cc02e86b6b3dbf14e7e53446c4f54c92a361040822
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: chrono
    version: 0.4.19
    sha: 670ad68c9088c2a963aaa298cb369688cf3f9465ce5e2d4ca10e6e0098a1ce73
  - name: chunked_transfer
    version: 0.3.1
    sha: 498d20a7aaf62625b9bf26e637cf7736417cde1d0c99f1d04d1170229a85cf87
  - name: clap
    version: 2.33.3
    sha: 37e58ac78573c40708d45522f0d80fa2f01cc4f9b4e2bf749807255454312002
  - name: cloudabi
    version: 0.0.3
    sha: ddfc5b9aa5d4507acaf872de71051dfd0e309860e88966e1051e462a077aac4f
  - name: combine
    version: 4.5.2
    sha: cc4369b5e4c0cddf64ad8981c0111e7df4f7078f4d6ba98fb31f2e17c4c57b7e
  - name: concurrent-queue
    version: 1.2.2
    sha: 30ed07550be01594c6026cff2a1d7fe9c8f683caa798e12b68694ac9e88286a3
  - name: conhash
    version: 0.4.0
    sha: 99d6364d028778d0d98b6014fa5882da377cd10d3492b7734d266a428e9b1fca
  - name: constant_time_eq
    version: 0.1.5
    sha: 245097e9a4535ee1e3e3931fcfcd55a796a44c643e8596ff6566d68f09b87bbc
  - name: cookie
    version: 0.12.0
    sha: 888604f00b3db336d2af898ec3c1d5d0ddf5e6d462220f2ededc33a87ac4bbd5
  - name: cookie_store
    version: 0.7.0
    sha: 46750b3f362965f197996c4448e4a0935e791bf7d6631bfce9ee0af3d24c919c
  - name: core-foundation
    version: 0.9.1
    sha: 0a89e2ae426ea83155dccf10c0fa6b1463ef6d5fcb44cee0b224a408fa640a62
  - name: core-foundation-sys
    version: 0.8.2
    sha: ea221b5284a47e40033bf9b66f35f984ec0ea2931eb03505246cd27a963f981b
  - name: counted-array
    version: 0.1.2
    sha: 384f8c53175c890920b6e0127b730709d2a173ca6c4dfdc81618ac9b46f648fe
  - name: cpuid-bool
    version: 0.1.2
    sha: 8aebca1129a03dc6dc2b127edd729435bbc4a37e1d5f4d7513165089ceb02634
  - name: crc32fast
    version: 1.2.1
    sha: 81156fece84ab6a9f2afdb109ce3ae577e42b1228441eded99bd77f627953b1a
  - name: crossbeam-deque
    version: 0.7.3
    sha: 9f02af974daeee82218205558e51ec8768b48cf524bd01d550abe5573a608285
  - name: crossbeam-epoch
    version: 0.8.2
    sha: 058ed274caafc1f60c4997b5fc07bf7dc7cca454af7c6e81edffe5f33f70dace
  - name: crossbeam-queue
    version: 0.1.2
    sha: 7c979cd6cfe72335896575c6b5688da489e420d36a27a0b9eb0c73db574b4a4b
  - name: crossbeam-queue
    version: 0.2.3
    sha: 774ba60a54c213d409d5353bda12d49cd68d14e45036a285234c8d6f91f92570
  - name: crossbeam-utils
    version: 0.6.6
    sha: 04973fa96e96579258a5091af6003abde64af786b860f18622b82e026cca60e6
  - name: crossbeam-utils
    version: 0.7.2
    sha: c3c7c73a2d1e9fc0886a08b93e98eb643461230d5f1925e4036204d5f2e261a8
  - name: crossbeam-utils
    version: 0.8.1
    sha: 02d96d1e189ef58269ebe5b97953da3274d83a93af647c2ddd6f9dab28cedb8d
  - name: crypto-mac
    version: 0.10.0
    sha: 4857fd85a0c34b3c3297875b747c1e02e06b6a0ea32dd892d8192b9ce0813ea6
  - name: crypto-mac
    version: 0.8.0
    sha: b584a330336237c1eecd3e94266efb216c56ed91225d634cb2991c5f3fd1aeab
  - name: daemonize
    version: 0.4.1
    sha: 70c24513e34f53b640819f0ac9f705b673fcf4006d7aab8778bee72ebfc89815
  - name: derive-error
    version: 0.0.3
    sha: 629f1bb3abce791912ca85a24676fff54464f7deb122906adabc90fb96e876d3
  - name: difference
    version: 2.0.0
    sha: 524cbf6897b527295dff137cec09ecf3a05f4fddffd7dfcd1585403449e74198
  - name: digest
    version: 0.9.0
    sha: d3dd60d1080a57a05ab032377049e0591415d2b31afd7028356dbf3cc6dcb066
  - name: directories
    version: 3.0.1
    sha: f8fed639d60b58d0f53498ab13d26f621fd77569cc6edb031f4cc36a2ad9da0f
  - name: dirs
    version: 1.0.5
    sha: 3fd78930633bd1c6e35c4b42b1df7b0cbc6bc191146e512bb3bedf243fcc3901
  - name: dirs-sys
    version: 0.3.5
    sha: 8e93d7f5705de3e49895a2b5e0b8855a1c27f080192ae9c32a6432d50741a57a
  - name: doc-comment
    version: 0.3.3
    sha: fea41bba32d969b513997752735605054bc0dfa92b4c56bf1189f2e174be7a10
  - name: dtoa
    version: 0.4.7
    sha: 88d7ed2934d741c6b37e33e3832298e8850b53fd2d2bea03873375596c7cea4e
  - name: either
    version: 1.6.1
    sha: e78d4f1cc4ae33bbfc157ed5d5a5ef3bc29227303d595861deb238fcec4e9457
  - name: encoding_rs
    version: 0.8.26
    sha: 801bbab217d7f79c0062f4f7205b5d4427c6d1a7bd7aafdd1475f7c59d62b283
  - name: env_logger
    version: 0.8.2
    sha: f26ecb66b4bdca6c1409b40fb255eefc2bd4f6d135dab3c3124f80ffa2a9661e
  - name: error-chain
    version: 0.12.4
    sha: 2d2f06b9cac1506ece98fe3231e3cc9c4410ec3d5b1f24ae1c8946f0742cdefc
  - name: event-listener
    version: 2.5.1
    sha: f7531096570974c3a9dcf9e4b8e1cede1ec26cf5046219fb3b9d897503b9be59
  - name: failure
    version: 0.1.8
    sha: d32e9bd16cc02eae7db7ef620b392808b89f6a5e16bb3497d159c6b92a0f4f86
  - name: failure_derive
    version: 0.1.8
    sha: aa4da3c766cd7a0db8242e326e9e4e081edd567072893ed320008189715366a4
  - name: fastrand
    version: 1.4.0
    sha: ca5faf057445ce5c9d4329e382b2ce7ca38550ef3b73a5348362d5f24e0c7fe3
  - name: filetime
    version: 0.2.13
    sha: 0c122a393ea57648015bf06fbd3d372378992e86b9ff5a7a497b076a28c79efe
  - name: flate2
    version: 1.0.19
    sha: 7411863d55df97a419aa64cb4d2f167103ea9d767e2c54a1868b7ac3f6b47129
  - name: float-cmp
    version: 0.8.0
    sha: e1267f4ac4f343772758f7b1bdcbe767c218bbab93bb432acbf5162bbf85a6c4
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
  - name: fuchsia-cprng
    version: 0.1.1
    sha: a06f77d526c1a601b7c4cdd98f54b5eaabffc14d5f2f0296febdc7f357c6d3ba
  - name: fuchsia-zircon
    version: 0.3.3
    sha: 2e9763c69ebaae630ba35f74888db465e49e259ba1bc0eda7d06f4a067615d82
  - name: fuchsia-zircon-sys
    version: 0.3.3
    sha: 3dcaa9ae7725d12cdb85b3ad99a434db70b468c09ded17e012d86b5c1010f7a7
  - name: futures
    version: 0.1.30
    sha: 4c7e4c2612746b0df8fed4ce0c69156021b704c9aefa360311c04e6e9e002eed
  - name: futures
    version: 0.3.9
    sha: c70be434c505aee38639abccb918163b63158a4b4bb791b45b7023044bdc3c9c
  - name: futures-channel
    version: 0.3.9
    sha: f01c61843314e95f96cc9245702248733a3a3d744e43e2e755e3c7af8348a0a9
  - name: futures-core
    version: 0.3.9
    sha: db8d3b0917ff63a2a96173133c02818fac4a746b0a57569d3baca9ec0e945e08
  - name: futures-cpupool
    version: 0.1.8
    sha: ab90cde24b3319636588d0c35fe03b1333857621051837ed769faefb4c2162e4
  - name: futures-executor
    version: 0.3.9
    sha: 9ee9ca2f7eb4475772cf39dd1cd06208dce2670ad38f4d9c7262b3e15f127068
  - name: futures-io
    version: 0.3.9
    sha: e37c1a51b037b80922864b8eed90692c5cd8abd4c71ce49b77146caa47f3253b
  - name: futures-lite
    version: 1.11.3
    sha: b4481d0cd0de1d204a4fa55e7d45f07b1d958abcb06714b3446438e2eff695fb
  - name: futures-macro
    version: 0.3.9
    sha: 0f8719ca0e1f3c5e34f3efe4570ef2c0610ca6da85ae7990d472e9cbfba13664
  - name: futures-sink
    version: 0.3.9
    sha: f6adabac1290109cfa089f79192fb6244ad2c3f1cc2281f3e1dd987592b71feb
  - name: futures-task
    version: 0.3.9
    sha: a92a0843a2ff66823a8f7c77bffe9a09be2b64e533562c412d63075643ec0038
  - name: futures-util
    version: 0.3.9
    sha: 036a2107cdeb57f6d7322f1b6c363dad67cd63ca3b7d1b925bdf75bd5d96cda9
  - name: generic-array
    version: 0.14.4
    sha: 501466ecc8a30d1d3b7fc9229b122b2ce8ed6e9d9223f1138d4babb253e51817
  - name: getopts
    version: 0.2.21
    sha: 14dbbfd5c71d70241ecf9e6f13737f7b5ce823821063188d7e46c41d371eebd5
  - name: getrandom
    version: 0.1.16
    sha: 8fc3cb4d91f53b50155bdcfd23f6a4c39ae1969c2ae85982b135750cccaf5fce
  - name: gimli
    version: 0.23.0
    sha: f6503fe142514ca4799d4c26297c4248239fe8838d827db6bd6065c6ed29a6ce
  - name: glob
    version: 0.3.0
    sha: 9b919933a397b79c37e33b77bb2aa3dc8eb6e165ad809e58ff75bc7db2e34574
  - name: gloo-timers
    version: 0.2.1
    sha: 47204a46aaff920a1ea58b11d03dec6f704287d27561724a4631e450654a891f
  - name: h2
    version: 0.1.26
    sha: a5b34c246847f938a410a03c5458c7fee2274436675e76d8b903c08efc29c462
  - name: hashbrown
    version: 0.9.1
    sha: d7afe4a420e3fe79967a00898cc1f4db7c8a49a9333a29f8a4bd76a253d5cd04
  - name: hermit-abi
    version: 0.1.17
    sha: 5aca5565f760fb5b220e499d72710ed156fdb74e631659e99377d9ebfbd13ae8
  - name: hmac
    version: 0.10.1
    sha: c1441c6b1e930e2817404b5046f1f989899143a12bf92de603b69f4e0aee1e15
  - name: http
    version: 0.1.21
    sha: d6ccf5ede3a895d8856620237b2f02972c1bbc78d2965ad7fe8838d4a0ed41f0
  - name: http-body
    version: 0.1.0
    sha: 6741c859c1b2463a423a1dbce98d418e6c3c3fc720fb0d45528657320920292d
  - name: httparse
    version: 1.3.4
    sha: cd179ae861f0c2e53da70d892f5f3029f9594be0c41dc5269cd371691b1dc2f9
  - name: humantime
    version: 2.0.1
    sha: 3c1ad908cc71012b7bea4d0c53ba96a8cba9962f048fa68d143376143d863b7a
  - name: hyper
    version: 0.12.35
    sha: 9dbe6ed1438e1f8ad955a4701e9a944938e9519f6888d12d8558b645e247d5f6
  - name: hyper-tls
    version: 0.3.2
    sha: 3a800d6aa50af4b5850b2b0f659625ce9504df908e9733b635720483be26174f
  - name: hyperx
    version: 0.12.0
    sha: 78e2d2253d7a17929560fc3adf48c48fc924c94fa4507e037a60e6bc55c0eda6
  - name: idna
    version: 0.1.5
    sha: 38f09e0f0b1fb55fdee1f17470ad800da77af5186a1a76c026b679358b7e844e
  - name: idna
    version: 0.2.0
    sha: 02e2673c30ee86b5b96a9cb52ad15718aa1f966f5ab9ad54a8b95d5ca33120a9
  - name: indexmap
    version: 1.6.1
    sha: 4fb1fa934250de4de8aef298d81c729a7d33d8c239daa3a7575e6b92bfc7313b
  - name: instant
    version: 0.1.9
    sha: 61124eeebbd69b8190558df225adf7e4caafce0d743919e5d6b19652314ec5ec
  - name: iovec
    version: 0.1.4
    sha: b2b3ea6ff95e175473f8ffe6a7eb7c00d054240321b84c57051175fe3c1e075e
  - name: itertools
    version: 0.10.0
    sha: 37d572918e350e82412fe766d24b15e6682fb2ed2bbe018280caa810397cb319
  - name: itertools
    version: 0.9.0
    sha: 284f18f85651fe11e8a991b2adb42cb078325c996ed026d994719efcfca1d54b
  - name: itoa
    version: 0.4.7
    sha: dd25036021b0de88a0aff6b850051563c6516d0bf53f8638938edbb9de732736
  - name: jobserver
    version: 0.1.21
    sha: 5c71313ebb9439f74b00d9d2dcec36440beaf57a6aa0623068441dd7cd81a7f2
  - name: js-sys
    version: 0.3.46
    sha: cf3d7383929f7c9c7c2d0fa596f325832df98c3704f2c60553080f7127a58175
  - name: jsonwebtoken
    version: 7.2.0
    sha: afabcc15e437a6484fc4f12d0fd63068fe457bf93f1c148d3d9649c60b103f32
  - name: kernel32-sys
    version: 0.2.2
    sha: 7507624b29483431c0ba2d82aece8ca6cdba9382bff4ddd0f7490560c056098d
  - name: kv-log-macro
    version: 1.0.7
    sha: 0de8b303297635ad57c9f5059fd9cee7a47f8e8daa09df0fcd07dd39fb22977f
  - name: language-tags
    version: 0.2.2
    sha: a91d884b6667cd606bb5a69aa0c99ba811a115fc68915e7056ec08a46e93199a
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: libc
    version: 0.2.82
    sha: 89203f3fba0a3795506acaad8ebce3c80c0af93f994d5a1d7a0b1eeb23271929
  - name: libmount
    version: 0.1.15
    sha: 23c4c2ad2d5cbd2f5a05620c3daf45930add53ec207fa99ce5eec971089dc35f
  - name: linked-hash-map
    version: 0.5.3
    sha: 8dd5a6d5999d9907cda8ed67bbd137d3af8085216c2ac62de5be860bd41f304a
  - name: local-encoding
    version: 0.2.0
    sha: e1ceb20f39ff7ae42f3ff9795f3986b1daad821caaa1e1732a0944103a5a1a66
  - name: lock_api
    version: 0.3.4
    sha: c4da24a77a3d8a6d4862d95f72e6fdb9c09a643ecdb402d754004a557f2bec75
  - name: log
    version: 0.3.9
    sha: e19e8d5c34a3e0e2223db8e060f9e8264aeeb5c5fc64a4ee9965c062211c024b
  - name: log
    version: 0.4.11
    sha: 4fabed175da42fed1fa0746b0ea71f412aa9d35e76e95e59b192c64b9dc2bf8b
  - name: matches
    version: 0.1.8
    sha: 7ffc5c5338469d4d3ea17d269fa8ea3512ad247247c30bd2df69e68309ed0a08
  - name: maybe-uninit
    version: 2.0.0
    sha: 60302e4db3a61da70c0cb7991976248362f30319e88850c487b9b95bbf059e00
  - name: md-5
    version: 0.9.1
    sha: 7b5a279bb9607f9f53c22d496eade00d138d1bdcccd07d74650387cf94942a15
  - name: md5
    version: 0.3.8
    sha: 79c56d6a0b07f9e19282511c83fc5b086364cbae4ba8c7d5f190c3d9b0425a48
  - name: memcached-rs
    version: 0.4.2
    sha: e3804643a8b556cd39d6b3ec5a181fdf36b642952d57f4c52bebd457d0ee0b1d
  - name: memchr
    version: 2.3.4
    sha: 0ee1c47aaa256ecabcaea351eae4a9b01ef39ed810004e298d2511ed284b1525
  - name: memoffset
    version: 0.5.6
    sha: 043175f069eda7b85febe4a74abbaeff828d9f8b448515d3151a14a3542811aa
  - name: mime
    version: 0.2.6
    sha: ba626b8a6de5da682e1caa06bdb42a335aee5a84db8e5046a3e8ab17ba0a3ae0
  - name: mime
    version: 0.3.16
    sha: 2a60c7ce501c71e03a9c9c0d35b861413ae925bd979cc7a4e30d060069aaac8d
  - name: mime_guess
    version: 1.8.8
    sha: 216929a5ee4dd316b1702eedf5e74548c123d370f47841ceaac38ca154690ca3
  - name: mime_guess
    version: 2.0.3
    sha: 2684d4c2e97d99848d30b324b00c8fcc7e5c897b7cbb5819b09e7c90e8baf212
  - name: miniz_oxide
    version: 0.4.3
    sha: 0f2d26ec3309788e423cfbf68ad1800f061638098d76a83681af979dc4eda19d
  - name: mio
    version: 0.6.23
    sha: 4afd66f5b91bf2a3bc13fad0e21caedac168ca4c707504e75585648ae80e4cc4
  - name: mio-named-pipes
    version: 0.1.7
    sha: 0840c1c50fd55e521b247f949c241c9997709f23bd7f023b9762cd561e935656
  - name: mio-uds
    version: 0.6.8
    sha: afcb699eb26d4332647cc848492bbc15eafb26f08d0304550d5aa1f612e066f0
  - name: miow
    version: 0.2.2
    sha: ebd808424166322d4a38da87083bfddd3ac4c131334ed55856112eb06d46944d
  - name: miow
    version: 0.3.6
    sha: 5a33c1b55807fbed163481b5ba66db4b2fa6cde694a5027be10fb724206c5897
  - name: multipart
    version: 0.15.4
    sha: adba94490a79baf2d6a23eac897157047008272fa3eecb3373ae6377b91eca28
  - name: native-tls
    version: 0.2.7
    sha: b8d96b2e1c8da3957d58100b09f102c6d9cfdfced01b7ec5a8974044bb09dbd4
  - name: nb-connect
    version: 1.0.2
    sha: 8123a81538e457d44b933a02faf885d3fe8408806b23fa700e8f01c6c3a98998
  - name: net2
    version: 0.2.37
    sha: 391630d12b68002ae1e25e8f974306474966550ad82dac6886fb8910c19568ae
  - name: nix
    version: 0.14.1
    sha: 6c722bee1037d430d0f8e687bbdbf222f27cc6e4e68d5caf630857bb2b6dbdce
  - name: nix
    version: 0.19.1
    sha: b2ccba0cfe4fdf15982d1674c69b1fd80bad427d293849982668dfe454bd61f2
  - name: normalize-line-endings
    version: 0.3.0
    sha: 61807f77802ff30975e01f4f071c8ba10c022052f98b3294119f3e615d13e5be
  - name: num-bigint
    version: 0.2.6
    sha: 090c7f9998ee0ff65aa5b723e4009f7b217707f1fb5ea551329cc4d6231fb304
  - name: num-integer
    version: 0.1.44
    sha: d2cc698a63b549a70bc047073d2949cce27cd1c7b0a4a862d08a8031bc2801db
  - name: num-traits
    version: 0.1.43
    sha: 92e5113e9fd4cc14ded8e499429f396a20f98c772a47cc8622a736e1ec843c31
  - name: num-traits
    version: 0.2.14
    sha: 9a64b1ec5cda2586e284722486d802acf1f7dbdc623e2bfc57e65ca1cd099290
  - name: num_cpus
    version: 1.13.0
    sha: 05499f3756671c15885fee9034446956fff3f243d6077b91e5767df161f766b3
  - name: number_prefix
    version: 0.4.0
    sha: 830b246a0e5f20af87141b25c173cd1b609bd7779a4617d6ec582abaf90870f3
  - name: object
    version: 0.22.0
    sha: 8d3b63360ec3cb337817c2dbd47ab4a0f170d285d8e5a2064600f3def1402397
  - name: once_cell
    version: 1.5.2
    sha: 13bd41f508810a131401606d54ac32a467c97172d74ba7662562ebba5ad07fa0
  - name: opaque-debug
    version: 0.3.0
    sha: 624a8340c38c1b80fd549087862da4ba43e08858af025b236e509b6649fc13d5
  - name: openssl
    version: 0.10.32
    sha: 038d43985d1ddca7a9900630d8cd031b56e4794eecc2e9ea39dd17aa04399a70
  - name: openssl-probe
    version: 0.1.2
    sha: 77af24da69f9d9341038eba93a073b1fdaaa1b788221b00a69bce9e762cb32de
  - name: openssl-sys
    version: 0.9.60
    sha: 921fc71883267538946025deffb622905ecad223c28efbfdef9bb59a0175f3e6
  - name: parking
    version: 2.0.0
    sha: 427c3892f9e783d91cc128285287e70a59e206ca452770ece88a76f7a3eddd72
  - name: parking_lot
    version: 0.9.0
    sha: f842b1982eb6c2fe34036a4fbfb06dd185a3f5c8edfaacdf7d1ea10b07de6252
  - name: parking_lot_core
    version: 0.6.2
    sha: b876b1b9e7ac6e1a74a6da34d25c42e17e8862aa409cbbbdcfc8d86c6f3bc62b
  - name: pem
    version: 0.8.2
    sha: f4c220d01f863d13d96ca82359d1e81e64a7c6bf0637bcde7b2349630addf0c6
  - name: percent-encoding
    version: 1.0.1
    sha: 31010dd2e1ac33d5b46a5b413495239882813e0369f8ed8a5e266f173602f831
  - name: percent-encoding
    version: 2.1.0
    sha: d4fd5641d01c8f18a23da7b6fe29298ff4b55afcccdf78973b24cf3175fee32e
  - name: phf
    version: 0.7.24
    sha: b3da44b85f8e8dfaec21adae67f95d93244b2ecf6ad2a692320598dcc8e6dd18
  - name: phf_codegen
    version: 0.7.24
    sha: b03e85129e324ad4166b06b2c7491ae27fe3ec353af72e72cd1654c7225d517e
  - name: phf_generator
    version: 0.7.24
    sha: 09364cc93c159b8b06b1f4dd8a4398984503483891b0c26b867cf431fb132662
  - name: phf_shared
    version: 0.7.24
    sha: 234f71a15de2288bcb7e3b6515828d22af7ec8598ee6d24c3b526fa0a80b67a0
  - name: pin-project-lite
    version: 0.1.11
    sha: c917123afa01924fc84bb20c4c03f004d9c38e5127e3c039bbf7f4b9c76a2f6b
  - name: pin-project-lite
    version: 0.2.1
    sha: e36743d754ccdf9954c2e352ce2d4b106e024c814f6499c2dadff80da9a442d8
  - name: pin-utils
    version: 0.1.0
    sha: 8b870d8c151b6f2fb93e84a13146138f05d02ed11c7e7c54f8826aaaf7c9f184
  - name: pkg-config
    version: 0.3.19
    sha: 3831453b3449ceb48b6d9c7ad7c96d5ea673e9b470a1dc578c2ce6521230884c
  - name: polling
    version: 2.0.2
    sha: a2a7bc6b2a29e632e45451c941832803a18cce6781db04de8a04696cdca8bde4
  - name: ppv-lite86
    version: 0.2.10
    sha: ac74c624d6b2d21f425f752262f42188365d7b8ff1aff74c82e45136510a4857
  - name: predicates
    version: 1.0.6
    sha: 73dd9b7b200044694dfede9edf907c1ca19630908443e9447e624993700c6932
  - name: predicates-core
    version: 1.0.1
    sha: fb3dbeaaf793584e29c58c7e3a82bbb3c7c06b63cea68d13b0e3cddc124104dc
  - name: predicates-tree
    version: 1.0.1
    sha: aee95d988ee893cb35c06b148c80ed2cd52c8eea927f50ba7a0be1a786aeab73
  - name: proc-macro-hack
    version: 0.5.19
    sha: dbf0c48bc1d91375ae5c3cd81e3722dff1abcf81a30960240640d223f59fe0e5
  - name: proc-macro-nested
    version: 0.1.6
    sha: eba180dafb9038b050a4c280019bbedf9f2467b61e5d892dcad585bb57aadc5a
  - name: proc-macro2
    version: 1.0.24
    sha: 1e0704ee1a7e00d7bb417d0770ea303c1bccbabf0ef1667dae92b5967f5f8a71
  - name: publicsuffix
    version: 1.5.4
    sha: 3bbaa49075179162b49acac1c6aa45fb4dafb5f13cf6794276d77bc7fd95757b
  - name: pulldown-cmark
    version: 0.0.3
    sha: 8361e81576d2e02643b04950e487ec172b687180da65c731c03cf336784e6c07
  - name: quick-error
    version: 1.2.3
    sha: a1d01941d82fa2ab50be1e79e6714289dd7cde78eba4c074bc5a4374f650dfe0
  - name: quote
    version: 0.3.15
    sha: 7a6e920b65c65f10b2ae65c831a81a073a89edd28c7cce89475bff467ab4167a
  - name: quote
    version: 1.0.8
    sha: 991431c3519a3f36861882da93630ce66b52918dcf1b8e2fd66b397fc96f28df
  - name: rand
    version: 0.4.6
    sha: 552840b97013b1a26992c11eac34bdd778e464601a4c2054b5f0bff7c6761293
  - name: rand
    version: 0.5.6
    sha: c618c47cd3ebd209790115ab837de41425723956ad3ce2e6a7f09890947cacb9
  - name: rand
    version: 0.6.5
    sha: 6d71dacdc3c88c1fde3885a3be3fbab9f35724e6ce99467f7d9c5026132184ca
  - name: rand
    version: 0.7.3
    sha: 6a6b1679d49b24bbfe0c803429aa1874472f50d9b363131f0e89fc356b544d03
  - name: rand_chacha
    version: 0.1.1
    sha: 556d3a1ca6600bfcbab7c7c91ccb085ac7fbbcd70e008a98742e7847f4f7bcef
  - name: rand_chacha
    version: 0.2.2
    sha: f4c8ed856279c9737206bf725bf36935d8666ead7aa69b52be55af369d193402
  - name: rand_core
    version: 0.3.1
    sha: 7a6fdeb83b075e8266dcc8762c22776f6877a63111121f5f8c7411e5be7eed4b
  - name: rand_core
    version: 0.4.2
    sha: 9c33a3c44ca05fa6f1807d8e6743f3824e8509beca625669633be0acbdf509dc
  - name: rand_core
    version: 0.5.1
    sha: 90bde5296fc891b0cef12a6d03ddccc162ce7b2aff54160af9338f8d40df6d19
  - name: rand_hc
    version: 0.1.0
    sha: 7b40677c7be09ae76218dc623efbf7b18e34bced3f38883af07bb75630a21bc4
  - name: rand_hc
    version: 0.2.0
    sha: ca3129af7b92a17112d59ad498c6f81eaf463253766b90396d39ea7a39d6613c
  - name: rand_isaac
    version: 0.1.1
    sha: ded997c9d5f13925be2a6fd7e66bf1872597f759fd9dd93513dd7e92e5a5ee08
  - name: rand_jitter
    version: 0.1.4
    sha: 1166d5c91dc97b88d1decc3285bb0a99ed84b05cfd0bc2341bdf2d43fc41e39b
  - name: rand_os
    version: 0.1.3
    sha: 7b75f676a1e053fc562eafbb47838d67c84801e38fc1ba459e8f180deabd5071
  - name: rand_pcg
    version: 0.1.2
    sha: abf9b09b01790cfe0364f52bf32995ea3c39f4d2dd011eac241d2914146d0b44
  - name: rand_xorshift
    version: 0.1.1
    sha: cbf7e9e623549b0e21f6e97cf8ecf247c1a8fd2e8a992ae265314300b2455d5c
  - name: rdrand
    version: 0.4.0
    sha: 678054eb77286b51581ba43620cc911abf02758c91f93f479767aed0f90458b2
  - name: redis
    version: 0.17.0
    sha: 95357caf2640abc54651b93c98a8df4fe1ccbf44b8e601ccdf43d5c1451f29ac
  - name: redox_syscall
    version: 0.1.57
    sha: 41cc0f7e4d5d4544e8861606a285bb08d3e70712ccc7d2b84d7c0ccfaf4b05ce
  - name: redox_users
    version: 0.3.5
    sha: de0737333e7a9502c789a36d7c7fa6092a49895d4faa31ca5df163857ded2e9d
  - name: regex
    version: 1.4.2
    sha: 38cf2c13ed4745de91a5eb834e11c00bcc3709e773173b2ce4c56c9fbde04b9c
  - name: regex-syntax
    version: 0.6.21
    sha: 3b181ba2dcf07aaccad5448e8ead58db5b742cf85dfe035e2227f137a539a189
  - name: remove_dir_all
    version: 0.5.3
    sha: 3acd125665422973a33ac9d3dd2df85edad0f4ae9b00dafb1a05e43a9f5ef8e7
  - name: reqwest
    version: 0.9.24
    sha: f88643aea3c1343c804950d7bf983bd2067f5ab59db6d613a08e05572f2714ab
  - name: retry
    version: 1.2.0
    sha: c15ef4789108d066d7fd85dcec330eab9b8e51244275922a9b7161afc4f46dda
  - name: ring
    version: 0.16.19
    sha: 024a1e66fea74c66c66624ee5622a7ff0e4b73a13b4f5c326ddb50c708944226
  - name: rouille
    version: 3.0.0
    sha: 112568052ec17fa26c6c11c40acbb30d3ad244bf3d6da0be181f5e7e42e5004f
  - name: rust-argon2
    version: 0.8.3
    sha: 4b18820d944b33caa75a71378964ac46f58517c92b6ae5f762636247c09e78fb
  - name: rustc-demangle
    version: 0.1.18
    sha: 6e3bad0ee36814ca07d7968269dd4b7ec89ec2da10c4bb613928d3077083c232
  - name: rustc_version
    version: 0.2.3
    sha: 138e3e0acb6c9fb258b19b67cb8abd63c00679d2851805ea151465464fe9030a
  - name: ryu
    version: 1.0.5
    sha: 71d301d4193d031abdd79ff7e3dd721168a9572ef3fe51a1517aba235bd8f86e
  - name: safemem
    version: 0.3.3
    sha: ef703b7cb59335eae2eb93ceb664c0eb7ea6bf567079d843e09420219668e072
  - name: same-file
    version: 1.0.6
    sha: 93fc1dc3aaa9bfed95e02e6eadabb4baf7e3078b0bd1b4d7b6b0b68378900502
  - name: schannel
    version: 0.1.19
    sha: 8f05ba609c234e60bee0d547fe94a4c7e9da733d1c962cf6e59efa4cd9c8bc75
  - name: scopeguard
    version: 1.1.0
    sha: d29ab0c6d3fc0ee92fe66e2d99f700eab17a8d57d1c1d3b748380fb20baa78cd
  - name: security-framework
    version: 2.0.0
    sha: c1759c2e3c8580017a484a7ac56d3abc5a6c1feadf88db2f3633f12ae4268c69
  - name: security-framework-sys
    version: 2.0.0
    sha: f99b9d5e26d2a71633cc4f2ebae7cc9f874044e0c351a27e17892d76dce5678b
  - name: selenium-rs
    version: 0.1.2
    sha: 01173ad274e14fafa534a5e660d950ca1939ccebd3955df987b7df7e4e301108
  - name: semver
    version: 0.9.0
    sha: 1d7eb9ef2c18661902cc47e535f9bc51b78acd254da71d375c2f6720d9a40403
  - name: semver-parser
    version: 0.7.0
    sha: 388a1df253eca08550bef6c72392cfe7c30914bf41df5269b68cbd6ff8f570a3
  - name: serde
    version: 1.0.118
    sha: 06c64263859d87aa2eb554587e2d23183398d617427327cf2b3d0ed8c69e4800
  - name: serde_derive
    version: 1.0.118
    sha: c84d3526699cd55261af4b941e4e725444df67aa4f9e6a3564f18030d12672df
  - name: serde_json
    version: 1.0.61
    sha: 4fceb2595057b6891a4ee808f70054bd2d12f0e97f1cbb78689b59f676df325a
  - name: serde_urlencoded
    version: 0.5.5
    sha: 642dd69105886af2efd227f75a520ec9b44a820d65bc133a9131f7d229fd165a
  - name: sha-1
    version: 0.9.2
    sha: ce3cdf1b5e620a498ee6f2a171885ac7e22f0e12089ec4b3d22b84921792507c
  - name: sha1
    version: 0.6.0
    sha: 2579985fda508104f7587689507983eadd6a6e84dd35d6d115361f530916fa0d
  - name: sha2
    version: 0.9.2
    sha: 6e7aab86fe2149bad8c507606bdb3f4ef5e7b2380eb92350f56122cca72a42a8
  - name: signal-hook-registry
    version: 1.3.0
    sha: 16f1d0fef1604ba8f7a073c7e701f213e056707210e9020af4528e0101ce11a6
  - name: simple_asn1
    version: 0.4.1
    sha: 692ca13de57ce0613a363c8c2f1de925adebc81b04c923ac60c5488bb44abe4b
  - name: siphasher
    version: 0.2.3
    sha: 0b8de496cf83d4ed58b6be86c3a275b8602f6ffe98d3024a869e124147a9a3ac
  - name: skeptic
    version: 0.4.0
    sha: 24ebf8a06f5f8bae61ae5bbc7af7aac4ef6907ae975130faba1199e5fe82256a
  - name: slab
    version: 0.4.2
    sha: c111b5bd5695e56cffe5129854aa230b39c93a305372fdbb2668ca2394eea9f8
  - name: smallvec
    version: 0.6.13
    sha: f7b0758c52e15a8b5e3691eae6cc559f08eee9406e548a4477ba4e67770a82b6
  - name: socket2
    version: 0.3.19
    sha: 122e570113d28d773067fab24266b66753f6ea915758651696b6e35e49f88d6e
  - name: spin
    version: 0.5.2
    sha: 6e63cff320ae2c57904679ba7cb63280a3dc4613885beafb148ee7bf9aa9042d
  - name: string
    version: 0.2.1
    sha: d24114bfcceb867ca7f71a0d3fe45d45619ec47a6fbfa98cb14e14250bfa5d6d
  - name: strip-ansi-escapes
    version: 0.1.0
    sha: 9d63676e2abafa709460982ddc02a3bb586b6d15a49b75c212e06edd3933acee
  - name: strsim
    version: 0.8.0
    sha: 8ea5119cdb4c55b55d432abb513a0429384878c15dde60cc77b1c99de1a95a6a
  - name: subtle
    version: 2.4.0
    sha: 1e81da0851ada1f3e9d4312c704aa4f8806f0f9d69faaf8df2f3464b4a9437c2
  - name: syn
    version: 0.11.11
    sha: d3b891b9015c88c576343b9b3e41c2c11a51c219ef067b264bd9c8aa9b441dad
  - name: syn
    version: 1.0.58
    sha: cc60a3d73ea6594cd712d830cc1f0390fd71542d8c8cd24e70cc54cdfd5e05d5
  - name: synom
    version: 0.11.3
    sha: a393066ed9010ebaed60b9eafa373d4b1baac186dd7e008555b0f702b51945b6
  - name: synstructure
    version: 0.12.4
    sha: b834f2d66f734cb897113e34aaff2f1ab4719ca946f9a7358dba8f8064148701
  - name: syslog
    version: 5.0.0
    sha: 9a5d8ef1b679c07976f3ee336a436453760c470f54b5e7237556728b8589515d
  - name: tar
    version: 0.4.30
    sha: 489997b7557e9a43e192c527face4feacc78bfbe6eed67fd55c4c9e381cba290
  - name: tempdir
    version: 0.3.7
    sha: 15f2b5fb00ccdf689e0149d1b1b3c03fead81c2b37735d812fa8bddbbf41b6d8
  - name: tempfile
    version: 3.1.0
    sha: 7a6e24d9338a0a5be79593e2fa15a648add6138caa803e2d5bc782c371732ca9
  - name: term
    version: 0.5.2
    sha: edd106a334b7657c10b7c540a0106114feadeb4dc314513e97df481d5d966f42
  - name: termcolor
    version: 1.1.2
    sha: 2dfed899f0eb03f32ee8c6a0aabdb8a7949659e3466561fc0adf54e26d88c5f4
  - name: textwrap
    version: 0.11.0
    sha: d326610f408c7a4eb6f51c37c330e496b08506c9457c9d34287ecc38809fb060
  - name: thiserror
    version: 1.0.23
    sha: 76cc616c6abf8c8928e2fdcc0dbfab37175edd8fb49a4641066ad1364fdab146
  - name: thiserror-impl
    version: 1.0.23
    sha: 9be73a2caec27583d0046ef3796c3794f868a5bc813db689eed00c7631275cd1
  - name: thread_local
    version: 1.1.0
    sha: bb9bc092d0d51e76b2b19d9d85534ffc9ec2db959a2523cdae0697e2972cd447
  - name: threadpool
    version: 1.8.1
    sha: d050e60b33d41c19108b32cea32164033a9013fe3b46cbd4457559bfbf77afaa
  - name: time
    version: 0.1.44
    sha: 6db9e6914ab8b1ae1c260a4ae7a49b6c5611b40328a735b21862567685e73255
  - name: tiny_http
    version: 0.6.2
    sha: 1661fa0a44c95d01604bd05c66732a446c657efb62b5164a7a083a3b552b4951
  - name: tinyvec
    version: 1.1.0
    sha: ccf8dbc19eb42fba10e8feaaec282fb50e2c14b2726d6301dbfeed0f73306a6f
  - name: tinyvec_macros
    version: 0.1.0
    sha: cda74da7e1a664f795bb1f8a87ec406fb89a02522cf6e50620d016add6dbbf5c
  - name: tokio
    version: 0.1.22
    sha: 5a09c0b5bb588872ab2f09afa13ee6e9dac11e10a0ec9e8e3ba39a5a5d530af6
  - name: tokio
    version: 0.2.24
    sha: 099837d3464c16a808060bb3f02263b412f6fafcb5d01c533d309985fbeebe48
  - name: tokio-buf
    version: 0.1.1
    sha: 8fb220f46c53859a4b7ec083e41dec9778ff0b1851c0942b211edb89e0ccdc46
  - name: tokio-codec
    version: 0.1.2
    sha: 25b2998660ba0e70d18684de5d06b70b70a3a747469af9dea7618cc59e75976b
  - name: tokio-compat
    version: 0.1.6
    sha: 107b625135aa7b9297dd2d99ccd6ca6ab124a5d1230778e159b9095adca4c722
  - name: tokio-current-thread
    version: 0.1.7
    sha: b1de0e32a83f131e002238d7ccde18211c0a5397f60cbfffcb112868c2e0e20e
  - name: tokio-executor
    version: 0.1.10
    sha: fb2d1b8f4548dbf5e1f7818512e9c406860678f29c300cdf0ebac72d1a3a1671
  - name: tokio-fs
    version: 0.1.7
    sha: 297a1206e0ca6302a0eed35b700d292b275256f596e2f3fea7729d5e629b6ff4
  - name: tokio-io
    version: 0.1.13
    sha: 57fc868aae093479e3131e3d165c93b1c7474109d13c90ec0dda2a1bbfff0674
  - name: tokio-named-pipes
    version: 0.1.0
    sha: 9d282d483052288b2308ba5ee795f5673b159c9bdf63c385a05609da782a5eae
  - name: tokio-process
    version: 0.2.5
    sha: 382d90f43fa31caebe5d3bc6cfd854963394fff3b8cb59d5146607aaae7e7e43
  - name: tokio-reactor
    version: 0.1.12
    sha: 09bc590ec4ba8ba87652da2068d150dcada2cfa2e07faae270a5e0409aa51351
  - name: tokio-serde
    version: 0.1.0
    sha: 894168193c4f80862a2244ff953b69145a9961a9efba39500e0970b083d0649c
  - name: tokio-serde-bincode
    version: 0.1.1
    sha: 02e35c8d60a5e87cfb30dd562a309e56f8a6d36617b0a76c87f04d5466607ca8
  - name: tokio-signal
    version: 0.2.9
    sha: d0c34c6e548f101053321cba3da7cbb87a610b85555884c41b07da2eb91aff12
  - name: tokio-sync
    version: 0.1.8
    sha: edfe50152bc8164fcc456dab7891fa9bf8beaf01c5ee7e1dd43a397c3cf87dee
  - name: tokio-tcp
    version: 0.1.4
    sha: 98df18ed66e3b72e742f185882a9e201892407957e45fbff8da17ae7a7c51f72
  - name: tokio-threadpool
    version: 0.1.18
    sha: df720b6581784c118f0eb4310796b12b1d242a7eb95f716a8367855325c25f89
  - name: tokio-timer
    version: 0.2.13
    sha: 93044f2d313c95ff1cb7809ce9a7a05735b012288a888b62d4434fd58c94f296
  - name: tokio-udp
    version: 0.1.6
    sha: e2a0b10e610b39c38b031a2fcab08e4b82f16ece36504988dcbd81dbba650d82
  - name: tokio-uds
    version: 0.2.7
    sha: ab57a4ac4111c8c9dbcf70779f6fc8bc35ae4b2454809febac840ad19bd7e4e0
  - name: tokio-util
    version: 0.3.1
    sha: be8242891f2b6cbef26a2d7e8605133c2c554cd35b3e4948ea892d6d68436499
  - name: toml
    version: 0.5.8
    sha: a31142970826733df8241ef35dc040ef98c679ab14d7c3e54d827099b3acecaa
  - name: tower
    version: 0.1.1
    sha: dc72f33b6a72c75c9df0037afce313018bae845f0ec7fdb9201b8768427a917f
  - name: tower-buffer
    version: 0.1.2
    sha: 3c7b83e1ccf5b23dd109dd6ae2c07b8e2beec7a51a21f29da2dba576317370e0
  - name: tower-discover
    version: 0.1.0
    sha: 73a7632286f78164d65d18fd0e570307acde9362489aa5c8c53e6315cc2bde47
  - name: tower-layer
    version: 0.1.0
    sha: 0ddf07e10c07dcc8f41da6de036dc66def1a85b70eb8a385159e3908bb258328
  - name: tower-limit
    version: 0.1.3
    sha: c21ba835a08fd54b63cd91ae0548a7b6e2a91075147dfa3dc8e1a940c1b6f18f
  - name: tower-load-shed
    version: 0.1.0
    sha: 04fbaf5bfb63d84204db87b9b2aeec61549613f2bbb8706dcc36f5f3ea8cd769
  - name: tower-retry
    version: 0.1.0
    sha: 09e80588125061f276ed2a7b0939988b411e570a2dbb2965b1382ef4f71036f7
  - name: tower-service
    version: 0.2.0
    sha: 2cc0c98637d23732f8de6dfd16494c9f1559c3b9e20b4a46462c8f9b9e827bfa
  - name: tower-timeout
    version: 0.1.1
    sha: 5c06bbc2fbd056f810940a8c6f0cc194557d36da3c22999a755a7a6612447da9
  - name: tower-util
    version: 0.1.0
    sha: 4792342fac093db5d2558655055a89a04ca909663467a4310c7739d9f8b64698
  - name: tracing
    version: 0.1.22
    sha: 9f47026cdc4080c07e49b37087de021820269d996f581aac150ef9e5583eefe3
  - name: tracing-attributes
    version: 0.1.11
    sha: 80e0ccfc3378da0cce270c946b676a376943f5cd16aeba64568e7939806f4ada
  - name: tracing-core
    version: 0.1.17
    sha: f50de3927f93d202783f4513cda820ab47ef17f624b03c096e86ef00c67e6b5f
  - name: treeline
    version: 0.1.0
    sha: a7f741b240f1a48843f9b8e0444fb55fb2a4ff67293b50a9179dfd5ea67f8d41
  - name: try-lock
    version: 0.2.3
    sha: 59547bce71d9c38b83d9c0e92b6066c4253371f15005def0c30d9657f50c7642
  - name: try_from
    version: 0.3.2
    sha: 283d3b89e1368717881a9d51dad843cc435380d8109c9e47d38780a324698d8b
  - name: twoway
    version: 0.1.8
    sha: 59b11b2b5241ba34be09c3cc85a36e56e48f9888862e19cedf23336d35316ed1
  - name: typenum
    version: 1.12.0
    sha: 373c8a200f9e67a0c95e62a4f52fbf80c23b4381c05a17845531982fa99e6b33
  - name: unicase
    version: 1.4.2
    sha: 7f4765f83163b74f957c797ad9253caf97f103fb064d3999aea9568d09fc8a33
  - name: unicase
    version: 2.6.0
    sha: 50f37be617794602aabbeee0be4f259dc1778fabe05e2d67ee8f79326d5cb4f6
  - name: unicode-bidi
    version: 0.3.4
    sha: 49f2bd0c6468a8230e1db229cff8029217cf623c767ea5d60bfbd42729ea54d5
  - name: unicode-normalization
    version: 0.1.16
    sha: a13e63ab62dbe32aeee58d1c5408d35c36c392bba5d9d3142287219721afe606
  - name: unicode-width
    version: 0.1.8
    sha: 9337591893a19b88d8d87f2cec1e73fad5cdfd10e5a6f349f498ad6ea2ffb1e3
  - name: unicode-xid
    version: 0.0.4
    sha: 8c1f860d7d29cf02cb2f3f359fd35991af3d30bac52c57d265a3c461074cb4dc
  - name: unicode-xid
    version: 0.2.1
    sha: f7fe0bb3479651439c9112f72b6c505038574c9fbb575ed1bf3b797fa39dd564
  - name: unix_socket
    version: 0.5.0
    sha: 6aa2700417c405c38f5e6902d699345241c28c0b7ade4abaad71e35a87eb1564
  - name: untrusted
    version: 0.7.1
    sha: a156c684c91ea7d62626509bce3cb4e1d9ed5c4d978f7b4352658f96a4c26b4a
  - name: url
    version: 1.7.2
    sha: dd4e7c0d531266369519a4aa4f399d748bd37043b00bde1e4ff1f60a120b355a
  - name: url
    version: 2.2.0
    sha: 5909f2b0817350449ed73e8bcd81c8c3c8d9a7a5d8acba4b27db277f1868976e
  - name: utf8parse
    version: 0.1.1
    sha: 8772a4ccbb4e89959023bc5b7cb8623a795caa7092d99f3aa9501b9484d4557d
  - name: uuid
    version: 0.7.4
    sha: 90dbc611eb48397705a6b0f6e917da23ae517e4d127123d2cf7674206627d32a
  - name: uuid
    version: 0.8.1
    sha: 9fde2f6a4bea1d6e007c4ad38c6839fa71cbb63b6dbf5b595aa38dc9b1093c11
  - name: vcpkg
    version: 0.2.11
    sha: b00bca6106a5e23f3eee943593759b7fcddb00554332e856d990c893966879fb
  - name: vec-arena
    version: 1.0.0
    sha: eafc1b9b2dfc6f5529177b62cf806484db55b32dc7c9658a118e11bbeb33061d
  - name: vec_map
    version: 0.8.2
    sha: f1bddf1187be692e79c5ffeab891132dfb0f236ed36a43c7ed39f1165ee20191
  - name: version-compare
    version: 0.0.11
    sha: 1c18c859eead79d8b95d09e4678566e8d70105c4e7b251f707a03df32442661b
  - name: version_check
    version: 0.1.5
    sha: 914b1a6776c4c929a602fafd8bc742e06365d4bcbe48c30f9cca5824f70dc9dd
  - name: version_check
    version: 0.9.2
    sha: b5a972e5669d67ba988ce3dc826706fb0a8b01471c088cb0b6110b805cc36aed
  - name: void
    version: 1.0.2
    sha: 6a02e4885ed3bc0f2de90ea6dd45ebcbb66dacffe03547fadbb0eeae2770887d
  - name: vte
    version: 0.3.3
    sha: 4f42f536e22f7fcbb407639765c8fd78707a33109301f834a594758bedd6e8cf
  - name: wait-timeout
    version: 0.2.0
    sha: 9f200f5b12eb75f8c1ed65abd4b2db8a6e1b138a20de009dacee265a2498f3f6
  - name: waker-fn
    version: 1.1.0
    sha: 9d5b2c62b4012a3e1eca5a7e077d13b3bf498c4073e33ccd58626607748ceeca
  - name: walkdir
    version: 2.3.1
    sha: 777182bc735b6424e1a57516d35ed72cb8019d85c8c9bf536dccb3445c1a2f7d
  - name: want
    version: 0.2.0
    sha: b6395efa4784b027708f7451087e647ec73cc74f5d9bc2e418404248d679a230
  - name: wasi
    version: 0.10.0+wasi-snapshot-preview1
    sha: 1a143597ca7c7793eff794def352d41792a93c481eb1042423ff7ff72ba2c31f
  - name: wasi
    version: 0.9.0+wasi-snapshot-preview1
    sha: cccddf32554fecc6acb585f82a32a72e28b48f8c4c1883ddfeeeaa96f7d8e519
  - name: wasm-bindgen
    version: 0.2.69
    sha: 3cd364751395ca0f68cafb17666eee36b63077fb5ecd972bbcd74c90c4bf736e
  - name: wasm-bindgen-backend
    version: 0.2.69
    sha: 1114f89ab1f4106e5b55e688b828c0ab0ea593a1ea7c094b141b14cbaaec2d62
  - name: wasm-bindgen-futures
    version: 0.4.19
    sha: 1fe9756085a84584ee9457a002b7cdfe0bfff169f45d2591d8be1345a6780e35
  - name: wasm-bindgen-macro
    version: 0.2.69
    sha: 7a6ac8995ead1f084a8dea1e65f194d0973800c7f571f6edd70adf06ecf77084
  - name: wasm-bindgen-macro-support
    version: 0.2.69
    sha: b5a48c72f299d80557c7c62e37e7225369ecc0c963964059509fbafe917c7549
  - name: wasm-bindgen-shared
    version: 0.2.69
    sha: 7e7811dd7f9398f14cc76efd356f98f03aa30419dea46aa810d71e819fc97158
  - name: web-sys
    version: 0.3.46
    sha: 222b1ef9334f92a21d3fb53dc3fd80f30836959a90f9274a626d7e06315ba3c3
  - name: wepoll-sys
    version: 3.0.1
    sha: 0fcb14dea929042224824779fbc82d9fab8d2e6d3cbc0ac404de8edf489e77ff
  - name: which
    version: 4.0.2
    sha: 87c14ef7e1b8b8ecfc75d5eca37949410046e66f15d185c01d70824f1f8111ef
  - name: winapi
    version: 0.2.8
    sha: 167dc9d6949a9b857f3451275e911c3f44255842c1f7a76f33c55103a909087a
  - name: winapi
    version: 0.3.9
    sha: 5c839a674fcd7a98952e593242ea400abe93992746761e38641405d28b00f419
  - name: winapi-build
    version: 0.1.1
    sha: 2d315eee3b34aca4797b2da6b13ed88266e6d612562a0c46390af8299fc699bc
  - name: winapi-i686-pc-windows-gnu
    version: 0.4.0
    sha: ac3b87c63620426dd9b991e5ce0329eff545bccbbb34f3be09ff6fb6ab51b7b6
  - name: winapi-util
    version: 0.1.5
    sha: 70ec6ce85bb158151cae5e5c87f95a8e97d2c0c4b001223f33a334e3ce5de178
  - name: winapi-x86_64-pc-windows-gnu
    version: 0.4.0
    sha: 712e227841d057c1ee1cd2fb22fa7e5a5461ae8e48fa2ca79ec42cfc1931183f
  - name: winreg
    version: 0.6.2
    sha: b2986deb581c4fe11b621998a5e53361efe6b48a151178d0cd9eeffa4dc6acc9
  - name: ws2_32-sys
    version: 0.2.1
    sha: d59cefebd0c892fa2dd6de581e937301d8552cb44489cdff035c6187cb63fa5e
  - name: xattr
    version: 0.2.2
    sha: 244c3741f4240ef46274860397c7c74e50eb23624996930e484c16679633a54c
  - name: zip
    version: 0.5.9
    sha: cc2896475a242c41366941faa27264df2cb935185a92e059a004d0048feb2ac5
  - name: zstd
    version: 0.6.0+zstd.1.4.8
    sha: d4e44664feba7f2f1a9f300c1f6157f2d1bfc3c15c6f3cf4beabf3f5abe9c237
  - name: zstd-safe
    version: 3.0.0+zstd.1.4.8
    sha: d9447afcd795693ad59918c7bbffe42fdd6e467d708f3537e3dc14dc598c573f
  - name: zstd-sys
    version: 1.4.19+zstd.1.4.8
    sha: ec24a9273d24437afb8e71b16f3d9a5d569193cccdb7896213b59f552f387674
