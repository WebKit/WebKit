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
  url: crates:crates/cargo-c/cargo-c-0.9.24+cargo-0.73.0.crate
  ref: 841e2368144d7240cc27004e0fbcd8c6192661dbf0c7ec64ecb5c1e9d0705986
- kind: cargo
  ref:
  - name: adler
    version: 1.0.2
    sha: f26201604c87b1e01bd3d98f8d5d9a8fcbb815e8cedb41ffccbeb4bf593a35fe
  - name: ahash
    version: 0.8.3
    sha: 2c99f64d1e06488f620f932677e24bc6e2897582980441ae90a671415bd7ec2f
  - name: aho-corasick
    version: 1.0.4
    sha: 6748e8def348ed4d14996fa801f4122cd763fff530258cdc03f64b25f89d3a5a
  - name: anstream
    version: 0.5.0
    sha: b1f58811cfac344940f1a400b6e6231ce35171f614f26439e80f8c1465c5cc0c
  - name: anstyle
    version: 1.0.2
    sha: 15c4c2c83f81532e5845a733998b6971faca23490340a418e9b72a3ec9de12ea
  - name: anstyle-parse
    version: 0.2.1
    sha: 938874ff5980b03a87c5524b3ae5b59cf99b1d6bc836848df7bc5ada9643c333
  - name: anstyle-query
    version: 1.0.0
    sha: 5ca11d4be1bab0c8bc8734a9aa7bf4ee8316d462a08c6ac5052f888fef5b494b
  - name: anstyle-wincon
    version: 2.1.0
    sha: 58f54d10c6dfa51283a066ceab3ec1ab78d13fae00aa49243a45e4571fb79dfd
  - name: anyhow
    version: 1.0.75
    sha: a4668cab20f66d8d020e1fbc0ebe47217433c1b6c8f2040faf858554e394ace6
  - name: arc-swap
    version: 1.6.0
    sha: bddcadddf5e9015d310179a59bb28c4d4b9920ad0f11e8e14dbadf654890c9a6
  - name: arrayvec
    version: 0.5.2
    sha: 23b62fc65de8e4e7f52534fb52b0f3ed04746ae267519eef2a83941e8085068b
  - name: autocfg
    version: 1.1.0
    sha: d468802bab17cbc0cc575e9b053f41e72aa36bfa6b7f55e3529ffa43161b97fa
  - name: base16ct
    version: 0.2.0
    sha: 4c7f02d4ea65f2c1853089ffd8d2787bdbc63de2f0d29dedbcf8ccdfa0ccd4cf
  - name: base64
    version: 0.21.3
    sha: 414dcefbc63d77c526a76b3afcf6fbb9b5e2791c19c3aa2297733208750c6e53
  - name: base64ct
    version: 1.6.0
    sha: 8c3c1a368f70d6cf7302d78f8f7093da241fb8e8807c05cc9e51a125895a6d5b
  - name: bitflags
    version: 1.3.2
    sha: bef38d45163c2f1dde094a7dfd33ccf595c92905c8f8f4fdc18d06fb1037718a
  - name: bitflags
    version: 2.4.0
    sha: b4682ae6287fcf752ecaabbfcc7b6f9b72aa33933dc23a554d853aea8eea8635
  - name: bitmaps
    version: 2.1.0
    sha: 031043d04099746d8db04daf1fa424b2bc8bd69d92b25962dcde24da39ab64a2
  - name: block-buffer
    version: 0.10.4
    sha: 3078c7629b62d3f0439517fa394996acacc5cbc91c5a20d8c658e77abd503a71
  - name: bstr
    version: 1.6.0
    sha: 6798148dccfbff0fae41c7574d2fa8f1ef3492fba0face179de5d8d447d67b05
  - name: btoi
    version: 0.4.3
    sha: 9dd6407f73a9b8b6162d8a2ef999fe6afd7cc15902ebf42c5cd296addf17e0ad
  - name: bumpalo
    version: 3.13.0
    sha: a3e2c3daef883ecc1b5d58c15adae93470a91d425f3532ba1695849656af3fc1
  - name: bytes
    version: 1.4.0
    sha: 89b2fd2a0dcf38d7971e2194b6b6eebab45ae01067456a7fd93d5547a61b70be
  - name: bytesize
    version: 1.3.0
    sha: a3e368af43e418a04d52505cf3dbc23dda4e3407ae2fa99fd0e4f308ce546acc
  - name: cargo
    version: 0.73.1
    sha: 77a6fe1f5394d14b81d2f3f605832a3ce35ed0bf120bc7ef437ce27fd4929c6a
  - name: cargo-platform
    version: 0.1.3
    sha: 2cfa25e60aea747ec7e1124f238816749faa93759c6ff5b31f1ccdda137f4479
  - name: cargo-util
    version: 0.2.5
    sha: dd54c8b94a0c851d687924460637361c355afafa72d973fe8644499fbdee8fae
  - name: cbindgen
    version: 0.25.0
    sha: faeaa693e5a727975a79211b8f35c0cb09b031fdb6eaa4a788bc6713d01488ca
  - name: cc
    version: 1.0.83
    sha: f1174fb0b6ec23863f8b971027804a42614e347eafb0a95bf0b12cdae21fc4d0
  - name: cfg-if
    version: 1.0.0
    sha: baf1de4339761588bc0619e3cbc0120ee582ebb74b53b4efbf79117bd2da40fd
  - name: clap
    version: 4.4.0
    sha: 1d5f1946157a96594eb2d2c10eb7ad9a2b27518cb3000209dec700c35df9197d
  - name: clap_builder
    version: 4.4.0
    sha: 78116e32a042dd73c2901f0dc30790d20ff3447f3e3472fad359e8c3d282bcd6
  - name: clap_derive
    version: 4.4.0
    sha: c9fd1a5729c4548118d7d70ff234a44868d00489a4b6597b0b020918a0e91a1a
  - name: clap_lex
    version: 0.5.1
    sha: cd7cc57abe963c6d3b9d8be5b06ba7c8957a930305ca90304f24ef040aa6f961
  - name: clru
    version: 0.6.1
    sha: b8191fa7302e03607ff0e237d4246cc043ff5b3cb9409d995172ba3bea16b807
  - name: colorchoice
    version: 1.0.0
    sha: acbf1af155f9b9ef647e42cdc158db4b64a1b61f743629225fde6f3e0be2a7c7
  - name: const-oid
    version: 0.9.5
    sha: 28c122c3980598d243d63d9a704629a2d748d101f278052ff068be5a4423ab6f
  - name: core-foundation
    version: 0.9.3
    sha: 194a7a9e6de53fa55116934067c844d9d749312f75c6f6d0980e8c252f8c2146
  - name: core-foundation-sys
    version: 0.8.4
    sha: e496a50fda8aacccc86d7529e2c1e0892dbd0f898a6b5645b5561b89c3210efa
  - name: cpufeatures
    version: 0.2.9
    sha: a17b76ff3a4162b0b27f354a0c87015ddad39d35f9c0c36607a3bdd175dde1f1
  - name: crates-io
    version: 0.37.0
    sha: 876aa69b4afca5f2eb5e23daa3445930faf829bcb67075a20ffa884f11f8c57c
  - name: crc32fast
    version: 1.3.2
    sha: b540bd8bc810d3885c6ea91e2018302f68baba2129ab3e88f32389ee9370880d
  - name: crossbeam-channel
    version: 0.5.8
    sha: a33c2bf77f2df06183c3aa30d1e96c0695a313d4f9c453cc3762a6db39f99200
  - name: crossbeam-utils
    version: 0.8.16
    sha: 5a22b2d63d4d1dc0b7f1b6b2747dd0088008a9be28b6ddf0b1e7d335e3037294
  - name: crypto-bigint
    version: 0.5.2
    sha: cf4c2f4e1afd912bc40bfd6fed5d9dc1f288e0ba01bfcc835cc5bc3eb13efe15
  - name: crypto-common
    version: 0.1.6
    sha: 1bfb12502f3fc46cca1bb51ac28df9d618d813cdc3d2f25b9fe775a34af26bb3
  - name: ct-codecs
    version: 1.1.1
    sha: f3b7eb4404b8195a9abb6356f4ac07d8ba267045c8d6d220ac4dc992e6cc75df
  - name: curl
    version: 0.4.44
    sha: 509bd11746c7ac09ebd19f0b17782eae80aadee26237658a6b4808afb5c11a22
  - name: curl-sys
    version: 0.4.65+curl-8.2.1
    sha: 961ba061c9ef2fe34bbd12b807152d96f0badd2bebe7b90ce6c8c8b7572a0986
  - name: der
    version: 0.7.8
    sha: fffa369a668c8af7dbf8b5e56c9f744fbd399949ed171606040001947de40b1c
  - name: deranged
    version: 0.3.8
    sha: f2696e8a945f658fd14dc3b87242e6b80cd0f36ff04ea560fa39082368847946
  - name: digest
    version: 0.10.7
    sha: 9ed9a281f7bc9b7576e61468ba615a66a5c8cfdff42420a70aa82701a3b1e292
  - name: dunce
    version: 1.0.4
    sha: 56ce8c6da7551ec6c462cbaf3bfbc75131ebbfa1c944aeaa9dab51ca1c5f0c3b
  - name: ecdsa
    version: 0.16.8
    sha: a4b1e0c257a9e9f25f90ff76d7a68360ed497ee519c8e428d1825ef0000799d4
  - name: ed25519-compact
    version: 2.0.4
    sha: 6a3d382e8464107391c8706b4c14b087808ecb909f6c15c34114bc42e53a9e4c
  - name: either
    version: 1.9.0
    sha: a26ae43d7bcc3b814de94796a5e736d4029efb0ee900c12e2d54c993ad1a1e07
  - name: elliptic-curve
    version: 0.13.5
    sha: 968405c8fdc9b3bf4df0a6638858cc0b52462836ab6b1c87377785dd09cf1c0b
  - name: env_logger
    version: 0.10.0
    sha: 85cdab6a89accf66733ad5a1693a4dcced6aeff64602b634530dd73c1f3ee9f0
  - name: equivalent
    version: 1.0.1
    sha: 5443807d6dff69373d433ab9ef5378ad8df50ca6298caf15de6e52e24aaf54d5
  - name: errno
    version: 0.3.2
    sha: 6b30f669a7961ef1631673d2766cc92f52d64f7ef354d4fe0ddfd30ed52f0f4f
  - name: errno-dragonfly
    version: 0.1.2
    sha: aa68f1b12764fab894d2755d2518754e71b4fd80ecfb822714a1206c2aab39bf
  - name: faster-hex
    version: 0.8.0
    sha: e9042d281a5eec0f2387f8c3ea6c4514e2cf2732c90a85aaf383b761ee3b290d
  - name: fastrand
    version: 2.0.0
    sha: 6999dc1837253364c2ebb0704ba97994bd874e8f195d665c50b7548f6ea92764
  - name: ff
    version: 0.13.0
    sha: ded41244b729663b1e574f1b4fb731469f69f79c17667b5d776b16cda0479449
  - name: fiat-crypto
    version: 0.1.20
    sha: e825f6987101665dea6ec934c09ec6d721de7bc1bf92248e1d5810c8cd636b77
  - name: filetime
    version: 0.2.22
    sha: d4029edd3e734da6fe05b6cd7bd2960760a616bd2ddd0d59a0124746d6272af0
  - name: flate2
    version: 1.0.27
    sha: c6c98ee8095e9d1dcbf2fcc6d95acccb90d1c81db1e44725c6a984b1dbdfb010
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
    version: 1.2.0
    sha: a62bc1cf6f830c2ec14a513a9fb124d0a213a629668a4186f329db21fe045652
  - name: fwdansi
    version: 1.1.0
    sha: 08c1f5787fe85505d1f7777268db5103d80a7a374d2316a7ce262e57baf8f208
  - name: generic-array
    version: 0.14.7
    sha: 85649ca51fd72272d7821adaf274ad91c288277713d9c18820d8499a7ff69e9a
  - name: getrandom
    version: 0.2.10
    sha: be4136b2a15dd319360be1c07d9933517ccf0be8f16bf62a3bee4f0d618df427
  - name: git2
    version: 0.17.2
    sha: 7b989d6a7ca95a362cf2cfc5ad688b3a467be1f87e480b8dad07fee8c79b0044
  - name: git2-curl
    version: 0.18.0
    sha: f8f8b7432b72928cff76f69e59ed5327f94a52763731e71274960dee72fe5f8c
  - name: gix
    version: 0.45.1
    sha: bf2a03ec66ee24d1b2bae3ab718f8d14f141613810cb7ff6756f7db667f1cd82
  - name: gix-actor
    version: 0.21.0
    sha: 9fe73f9f6be1afbf1bd5be919a9636fa560e2f14d42262a934423ed6760cd838
  - name: gix-attributes
    version: 0.13.1
    sha: 78b79590ac382f80d87e06416f5fcac6fee5d83dcb152a00ed0bdbaa988acc31
  - name: gix-bitmap
    version: 0.2.7
    sha: 0ccab4bc576844ddb51b78d81b4a42d73e6229660fa614dfc3d3999c874d1959
  - name: gix-chunk
    version: 0.4.4
    sha: 5b42ea64420f7994000130328f3c7a2038f639120518870436d31b8bde704493
  - name: gix-command
    version: 0.2.9
    sha: 0f28f654184b5f725c5737c7e4f466cbd8f0102ac352d5257eeab19647ee4256
  - name: gix-commitgraph
    version: 0.16.0
    sha: e8490ae1b3d55c47e6a71d247c082304a2f79f8d0332c1a2f5693d42a2021a09
  - name: gix-config
    version: 0.23.0
    sha: 51f310120ae1ba8f0ca52fb22876ce9bad5b15c8ffb3eb7302e4b64a3b9f681c
  - name: gix-config-value
    version: 0.12.5
    sha: 6e874f41437441c02991dcea76990b9058fadfc54b02ab4dd06ab2218af43897
  - name: gix-credentials
    version: 0.15.0
    sha: c6f89fea8acd28f5ef8fa5042146f1637afd4d834bc8f13439d8fd1e5aca0d65
  - name: gix-date
    version: 0.5.1
    sha: bc164145670e9130a60a21670d9b6f0f4f8de04e5dd256c51fa5a0340c625902
  - name: gix-diff
    version: 0.30.1
    sha: 9029ad0083cc286a4bd2f5b3bf66bb66398abc26f2731a2824cd5edfc41a0e33
  - name: gix-discover
    version: 0.19.0
    sha: aba9c6c0d1f2b2efe65581de73de4305004612d49c83773e783202a7ef204f46
  - name: gix-features
    version: 0.30.0
    sha: 3a8c493409bf6060d408eec9bbdd1b12ea351266b50012e2a522f75dfc7b8314
  - name: gix-fs
    version: 0.2.0
    sha: 30da8997008adb87f94e15beb7ee229f8a48e97af585a584bfee4a5a1880aab5
  - name: gix-glob
    version: 0.8.0
    sha: cd0ade1e80ab1f079703d1824e1daf73009096386aa7fd2f0477f6e4ac0a558e
  - name: gix-hash
    version: 0.11.4
    sha: 4b422ff2ad9a0628baaad6da468cf05385bf3f5ab495ad5a33cce99b9f41092f
  - name: gix-hashtable
    version: 0.2.4
    sha: 385f4ce6ecf3692d313ca3aa9bd3b3d8490de53368d6d94bedff3af8b6d9c58d
  - name: gix-ignore
    version: 0.3.0
    sha: fc6f7f101a0ccce808dbf7008ba131dede94e20257e7bde7a44cbb2f8c775625
  - name: gix-index
    version: 0.17.0
    sha: 616ba958fabfb11263fa042c35690d48a6c7be4e9277e2c7e24ff263b3fe7b82
  - name: gix-lock
    version: 6.0.0
    sha: 3ec5d5e6f07316d3553aa7425e3ecd935ec29882556021fe1696297a448af8d2
  - name: gix-mailmap
    version: 0.13.0
    sha: 4653701922c920e009f1bc4309feaff14882ade017770788f9a150928da3fa6a
  - name: gix-negotiate
    version: 0.2.1
    sha: 945c3ef1e912e44a5f405fc9e924edf42000566a1b257ed52cb1293300f6f08c
  - name: gix-object
    version: 0.30.0
    sha: 8926c8f51c44dec3e709cb5dbc93deb9e8d4064c43c9efc54c158dcdfe8446c7
  - name: gix-odb
    version: 0.46.0
    sha: 4b234d806278eeac2f907c8b5a105c4ba537230c1a9d9236d822bf0db291f8f3
  - name: gix-pack
    version: 0.36.0
    sha: 7d2a14cb3156037eedb17d6cb7209b7180522b8949b21fd0fe3184c0a1d0af88
  - name: gix-packetline
    version: 0.16.5
    sha: 2a374cb5eba089e3c123df4d996eb00da411bb90ec92cb35bffeeb2d22ee106a
  - name: gix-path
    version: 0.8.4
    sha: 18609c8cbec8508ea97c64938c33cd305b75dfc04a78d0c3b78b8b3fd618a77c
  - name: gix-prompt
    version: 0.5.5
    sha: 2c22decaf4a063ccae2b2108820c8630c01bd6756656df3fe464b32b8958a5ea
  - name: gix-protocol
    version: 0.33.2
    sha: 92a17058b45c461f0847528c5fb6ee6e76115e026979eb2d2202f98ee94f6c24
  - name: gix-quote
    version: 0.4.7
    sha: 475c86a97dd0127ba4465fbb239abac9ea10e68301470c9791a6dd5351cdc905
  - name: gix-ref
    version: 0.30.0
    sha: ebdd999256f4ce8a5eefa89999879c159c263f3493a951d62aa5ce42c0397e1c
  - name: gix-refspec
    version: 0.11.0
    sha: 72bfd622abc86dd8ad1ec51b9eb77b4f1a766b94e3a1b87cf4a022c5b5570cf4
  - name: gix-revision
    version: 0.15.2
    sha: 5044f56cd7a487ce9b034cbe0252ae0b6b47ff56ca3dabd79bc30214d0932cd7
  - name: gix-revwalk
    version: 0.1.0
    sha: bc2623ba8747914f151f5e12b65adac576ab459dbed5f50a36c7a3e9cbf2d3ca
  - name: gix-sec
    version: 0.8.4
    sha: 9615cbd6b456898aeb942cd75e5810c382fbfc48dbbff2fa23ebd2d33dcbe9c7
  - name: gix-tempfile
    version: 6.0.0
    sha: b3785cb010e9dc5c446dfbf02bc1119fc17d3a48a27c029efcb3a3c32953eb10
  - name: gix-trace
    version: 0.1.3
    sha: 96b6d623a1152c3facb79067d6e2ecdae48130030cf27d6eb21109f13bd7b836
  - name: gix-transport
    version: 0.32.0
    sha: 64a39ffed9a9078ed700605e064b15d7c6ae50aa65e7faa36ca6919e8081df15
  - name: gix-traverse
    version: 0.26.0
    sha: b0842e984cb4bf26339dc559f3a1b8bf8cdb83547799b2b096822a59f87f33d9
  - name: gix-url
    version: 0.19.0
    sha: f1663df25ac42047a2547618d2a6979a26f478073f6306997429235d2cd4c863
  - name: gix-utils
    version: 0.1.5
    sha: b85d89dc728613e26e0ed952a19583744e7f5240fcd4aa30d6c824ffd8b52f0f
  - name: gix-validate
    version: 0.7.7
    sha: ba9b3737b2cef3dcd014633485f0034b0f1a931ee54aeb7d8f87f177f3c89040
  - name: gix-worktree
    version: 0.18.0
    sha: d388ad962e8854402734a7387af8790f6bdbc8d05349052dab16ca4a0def50f6
  - name: glob
    version: 0.3.1
    sha: d2fabcfbdc87f4758337ca535fb41a6d701b65693ce38287d856d1674551ec9b
  - name: globset
    version: 0.4.13
    sha: 759c97c1e17c55525b57192c06a267cda0ac5210b222d6b82189a2338fa1c13d
  - name: group
    version: 0.13.0
    sha: f0f9ef7462f7c099f518d754361858f86d8a07af53ba9af0fe635bbccb151a63
  - name: hashbrown
    version: 0.12.3
    sha: 8a9ee70c43aaf417c914396645a0fa852624801b24ebb7ae78fe8272889ac888
  - name: hashbrown
    version: 0.14.0
    sha: 2c6201b9ff9fd90a5a3bac2e56a830d0caa509576f0e503818ee82c181b3437a
  - name: heck
    version: 0.4.1
    sha: 95505c38b4572b2d910cecb0281560f54b440a19336cbbcb27bf6ce6adc6f5a8
  - name: hermit-abi
    version: 0.3.2
    sha: 443144c8cdadd93ebf52ddb4056d257f5b52c04d3c804e657d19eb73fc33668b
  - name: hex
    version: 0.4.3
    sha: 7f24254aa9a54b5c858eaee2f5bccdb46aaf0e486a595ed5fd8f86ba55232a70
  - name: hkdf
    version: 0.12.3
    sha: 791a029f6b9fc27657f6f188ec6e5e43f6911f6f878e0dc5501396e09809d437
  - name: hmac
    version: 0.12.1
    sha: 6c49c37c09c17a53d937dfbb742eb3a961d65a994e6bcdcf37e7399d0cc8ab5e
  - name: home
    version: 0.5.5
    sha: 5444c27eef6923071f7ebcc33e3444508466a76f7a2b93da00ed6e19f30c1ddb
  - name: http-auth
    version: 0.1.8
    sha: 5430cacd7a1f9a02fbeb350dfc81a0e5ed42d81f3398cb0ba184017f85bdcfbc
  - name: humantime
    version: 2.1.0
    sha: 9a3a5bfb195931eeb336b2a7b4d761daec841b97f947d34394601737a7bba5e4
  - name: idna
    version: 0.4.0
    sha: 7d20d6b07bfbc108882d88ed8e37d39636dcc260e15e30c45e6ba089610b917c
  - name: ignore
    version: 0.4.20
    sha: dbe7873dab538a9a44ad79ede1faf5f30d49f9a5c883ddbab48bce81b64b7492
  - name: im-rc
    version: 15.1.0
    sha: af1955a75fa080c677d3972822ec4bad316169ab1cfc6c257a942c2265dbe5fe
  - name: imara-diff
    version: 0.1.5
    sha: e98c1d0ad70fc91b8b9654b1f33db55e59579d3b3de2bffdced0fdb810570cb8
  - name: indexmap
    version: 1.9.3
    sha: bd070e393353796e801d209ad339e89596eb4c8d430d18ede6a1cced8fafbd99
  - name: indexmap
    version: 2.0.0
    sha: d5477fe2230a79769d8dc68e0eabf5437907c0457a5614a9e8dddb67f65eb65d
  - name: io-close
    version: 0.3.7
    sha: 9cadcf447f06744f8ce713d2d6239bb5bde2c357a452397a9ed90c625da390bc
  - name: io-lifetimes
    version: 1.0.11
    sha: eae7b9aee968036d54dce06cebaefd919e4472e753296daccd6d344e3e2df0c2
  - name: is-terminal
    version: 0.4.9
    sha: cb0889898416213fab133e1d33a0e5858a48177452750691bde3666d0fdbaf8b
  - name: itertools
    version: 0.10.5
    sha: b0fd2260e829bddf4cb6ea802289de2f86d6a7a690192fbe91b3f46e0f2c8473
  - name: itertools
    version: 0.11.0
    sha: b1c173a5686ce8bfa551b3563d0c2170bf24ca44da99c7ca4bfdab5418c3fe57
  - name: itoa
    version: 1.0.9
    sha: af150ab688ff2122fcef229be89cb50dd66af9e01a4ff320cc137eecc9bacc38
  - name: jobserver
    version: 0.1.26
    sha: 936cfd212a0155903bcbc060e316fb6cc7cbf2e1907329391ebadc1fe0ce77c2
  - name: js-sys
    version: 0.3.64
    sha: c5f195fe497f702db0f318b07fdd68edb16955aed830df8363d837542f8f935a
  - name: kstring
    version: 2.0.0
    sha: ec3066350882a1cd6d950d055997f379ac37fd39f81cd4d8ed186032eb3c5747
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: lazycell
    version: 1.3.0
    sha: 830d08ce1d1d941e6b30645f1a0eb5643013d835ce3779a5fc208261dbe10f55
  - name: libc
    version: 0.2.147
    sha: b4668fb0ea861c1df094127ac5f1da3409a82116a4ba74fca2e58ef927159bb3
  - name: libgit2-sys
    version: 0.15.2+1.6.4
    sha: a80df2e11fb4a61f4ba2ab42dbe7f74468da143f1a75c74e11dee7c813f694fa
  - name: libnghttp2-sys
    version: 0.1.8+1.55.1
    sha: 4fae956c192dadcdb5dace96db71fa0b827333cce7c7b38dc71446f024d8a340
  - name: libssh2-sys
    version: 0.3.0
    sha: 2dc8a030b787e2119a731f1951d6a773e2280c660f8ec4b0f5e1505a386e71ee
  - name: libz-sys
    version: 1.1.12
    sha: d97137b25e321a73eef1418d1d5d2eda4d77e12813f8e6dead84bc52c5870a7b
  - name: linux-raw-sys
    version: 0.3.8
    sha: ef53942eb7bf7ff43a617b3e2c1c4a5ecf5944a7c1bc12d7ee39bbb15e5c1519
  - name: linux-raw-sys
    version: 0.4.5
    sha: 57bcfdad1b858c2db7c38303a6d2ad4dfaf5eb53dfeb0910128b2c26d6158503
  - name: lock_api
    version: 0.4.10
    sha: c1cc9717a20b1bb222f333e6a92fd32f7d8a18ddc5a3191a11af45dcbf4dcd16
  - name: log
    version: 0.4.20
    sha: b5e6163cb8c49088c2c36f57875e58ccd8c87c7427f7fbd50ea6710b2f3f2e8f
  - name: maybe-async
    version: 0.2.7
    sha: 0f1b8c13cb1f814b634a96b2c725449fe7ed464a7b8781de8688be5ffbd3f305
  - name: memchr
    version: 2.5.0
    sha: 2dffe52ecf27772e601905b7522cb4ef790d2cc203488bbd0e2fe85fcb74566d
  - name: memmap2
    version: 0.5.10
    sha: 83faa42c0a078c393f6b29d5db232d8be22776a891f8f56e5284faee4a20b327
  - name: minimal-lexical
    version: 0.2.1
    sha: 68354c5c6bd36d73ff3feceb05efa59b6acb7626617f4962be322a825e61f79a
  - name: miniz_oxide
    version: 0.7.1
    sha: e7810e0be55b428ada41041c41f32c9f1a42817901b4ccf45fa3d4b6561e74c7
  - name: miow
    version: 0.5.0
    sha: 52ffbca2f655e33c08be35d87278e5b18b89550a37dbd598c20db92f6a471123
  - name: nom
    version: 7.1.3
    sha: d273983c5a657a70a3e8f2a01329822f3b8c8172b73826411a55751e404a0a4a
  - name: num-traits
    version: 0.2.16
    sha: f30b0abd723be7e2ffca1272140fac1a2f084c77ec3e123c192b66af1ee9e6c2
  - name: num_threads
    version: 0.1.6
    sha: 2819ce041d2ee131036f4fc9d6ae7ae125a3a40e97ba64d04fe799ad9dabbb44
  - name: once_cell
    version: 1.18.0
    sha: dd8b5dd2ae5ed71462c540258bedcb51965123ad7e7ccf4b9a8cafaa4a63576d
  - name: opener
    version: 0.5.2
    sha: 293c15678e37254c15bd2f092314abb4e51d7fdde05c2021279c12631b54f005
  - name: openssl
    version: 0.10.56
    sha: 729b745ad4a5575dd06a3e1af1414bd330ee561c01b3899eb584baeaa8def17e
  - name: openssl-macros
    version: 0.1.1
    sha: a948666b637a0f465e8564c73e89d4dde00d72d4d473cc972f390fc3dcee7d9c
  - name: openssl-probe
    version: 0.1.5
    sha: ff011a302c396a5197692431fc1948019154afc178baf7d8e37367442a4601cf
  - name: openssl-src
    version: 111.27.0+1.1.1v
    sha: 06e8f197c82d7511c5b014030c9b1efeda40d7d5f99d23b4ceed3524a5e63f02
  - name: openssl-sys
    version: 0.9.91
    sha: 866b5f16f90776b9bb8dc1e1802ac6f0513de3a7a7465867bfbc563dc737faac
  - name: ordered-float
    version: 2.10.0
    sha: 7940cf2ca942593318d07fcf2596cdca60a85c9e7fab408a5e21a4f9dcd40d87
  - name: orion
    version: 0.17.5
    sha: b11468cc6afd61a126fe3f91cc4cc8a0dbe7917d0a4b5e8357ba91cc47444462
  - name: os_info
    version: 3.7.0
    sha: 006e42d5b888366f1880eda20371fedde764ed2213dc8496f49622fa0c99cd5e
  - name: p384
    version: 0.13.0
    sha: 70786f51bcc69f6a4c0360e063a4cac5419ef7c5cd5b3c99ad70f3be5ba79209
  - name: parking_lot
    version: 0.12.1
    sha: 3742b2c103b9f06bc9fff0a37ff4912935851bee6d36f3c02bcc755bcfec228f
  - name: parking_lot_core
    version: 0.9.8
    sha: 93f00c865fe7cabf650081affecd3871070f26767e7b2070a3ffae14c654b447
  - name: pasetors
    version: 0.6.7
    sha: ba765699a309908d55950919a3445e9491453e89b2587b1b2abe4143a48894c0
  - name: pathdiff
    version: 0.2.1
    sha: 8835116a5c179084a830efb3adc117ab007512b535bc1a21c991d3b32a6b44dd
  - name: pem-rfc7468
    version: 0.7.0
    sha: 88b39c9bfcfc231068454382784bb460aae594343fb030d46e9f50a645418412
  - name: percent-encoding
    version: 2.3.0
    sha: 9b2a4787296e9989611394c33f193f676704af1686e70b8f8033ab5ba9a35a94
  - name: pkcs8
    version: 0.10.2
    sha: f950b2377845cebe5cf8b5165cb3cc1a5e0fa5cfa3e1f7f55707d8fd82e0a7b7
  - name: pkg-config
    version: 0.3.27
    sha: 26072860ba924cbfa98ea39c8c19b4dd6a4a25423dbdf219c1eca91aa0cf6964
  - name: ppv-lite86
    version: 0.2.17
    sha: 5b40af805b3121feab8a3c29f04d8ad262fa8e0561883e7653e024ae4479e6de
  - name: primeorder
    version: 0.13.2
    sha: 3c2fcef82c0ec6eefcc179b978446c399b3cdf73c392c35604e399eee6df1ee3
  - name: proc-macro2
    version: 1.0.66
    sha: 18fb31db3f9bddb2ea821cde30a9f70117e3f119938b5ee630b7403aa6e2ead9
  - name: prodash
    version: 25.0.2
    sha: 1d67eb4220992a4a052a4bb03cf776e493ecb1a3a36bab551804153d63486af7
  - name: pulldown-cmark
    version: 0.9.3
    sha: 77a1a2f1f0a7ecff9c31abbe177637be0e97a0aef46cf8738ece09327985d998
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
  - name: rand_xoshiro
    version: 0.6.0
    sha: 6f97cdb2a36ed4183de61b2f824cc45c9f1037f28afe0a322e9fff4c108b5aaa
  - name: redox_syscall
    version: 0.3.5
    sha: 567664f262709473930a4bf9e51bf2ebf3348f2e748ccc50dea20646858f8f29
  - name: regex
    version: 1.9.4
    sha: 12de2eff854e5fa4b1295edd650e227e9d8fb0c9e90b12e7f36d6a6811791a29
  - name: regex-automata
    version: 0.3.7
    sha: 49530408a136e16e5b486e883fbb6ba058e8e4e8ae6621a77b048b314336e629
  - name: regex-syntax
    version: 0.7.5
    sha: dbb5fb1acd8a1a18b3dd5be62d25485eb770e05afb408a9627d14d451bae12da
  - name: rfc6979
    version: 0.4.0
    sha: f8dd2a808d456c4a54e300a23e9f5a67e122c3024119acbfd73e3bf664491cb2
  - name: rustfix
    version: 0.6.1
    sha: ecd2853d9e26988467753bd9912c3a126f642d05d229a4b53f5752ee36c56481
  - name: rustix
    version: 0.37.23
    sha: 4d69718bf81c6127a49dc64e44a742e8bb9213c0ff8869a22c308f84c1d4ab06
  - name: rustix
    version: 0.38.9
    sha: 9bfe0f2582b4931a45d1fa608f8a8722e8b3c7ac54dd6d5f3b3212791fedef49
  - name: ryu
    version: 1.0.15
    sha: 1ad4cc8da4ef723ed60bced201181d83791ad433213d8c24efffda1eec85d741
  - name: same-file
    version: 1.0.6
    sha: 93fc1dc3aaa9bfed95e02e6eadabb4baf7e3078b0bd1b4d7b6b0b68378900502
  - name: schannel
    version: 0.1.22
    sha: 0c3733bf4cf7ea0880754e19cb5a462007c4a8c1914bff372ccc95b464f1df88
  - name: scopeguard
    version: 1.2.0
    sha: 94143f37725109f92c262ed2cf5e59bce7498c01bcc1502d7b9afe439a4e9f49
  - name: sec1
    version: 0.7.3
    sha: d3e97a565f76233a6003f9f5c54be1d9c5bdfa3eccfb189469f11ec4901c47dc
  - name: semver
    version: 1.0.18
    sha: b0293b4b29daaf487284529cc2f5675b8e57c61f70167ba415a463651fd6a918
  - name: serde
    version: 1.0.188
    sha: cf9e0fcba69a370eed61bcf2b728575f726b50b55cba78064753d708ddc7549e
  - name: serde-value
    version: 0.7.0
    sha: f3a1a3341211875ef120e117ea7fd5228530ae7e7036a779fdc9117be6b3282c
  - name: serde_derive
    version: 1.0.188
    sha: 4eca7ac642d82aa35b60049a6eccb4be6be75e599bd2e9adb5f875a737654af2
  - name: serde_ignored
    version: 0.1.9
    sha: 80c31d5c53fd39f208e770f5a20a0bb214dee2a8d0d8adba18e19ad95a482ca5
  - name: serde_json
    version: 1.0.105
    sha: 693151e1ac27563d6dbcec9dee9fbd5da8539b20fa14ad3752b2e6d363ace360
  - name: serde_spanned
    version: 0.6.3
    sha: 96426c9936fd7a0124915f9185ea1d20aa9445cc9821142f0a73bc9207a2e186
  - name: sha1
    version: 0.10.5
    sha: f04293dc80c3993519f2d7f6f511707ee7094fe0c6d3406feb330cdb3540eba3
  - name: sha1_smol
    version: 1.0.0
    sha: ae1a47186c03a32177042e55dbc5fd5aee900b8e0069a8d70fba96a9375cd012
  - name: sha2
    version: 0.10.7
    sha: 479fb9d862239e610720565ca91403019f2f00410f1864c5aa7479b950a76ed8
  - name: shell-escape
    version: 0.1.5
    sha: 45bb67a18fa91266cc7807181f62f9178a6873bfad7dc788c42e6430db40184f
  - name: signal-hook
    version: 0.3.17
    sha: 8621587d4798caf8eb44879d42e56b9a93ea5dcd315a6487c357130095b62801
  - name: signal-hook-registry
    version: 1.4.1
    sha: d8229b473baa5980ac72ef434c4415e70c4b5e71b423043adb4ba059f89c99a1
  - name: signature
    version: 2.1.0
    sha: 5e1788eed21689f9cf370582dfc467ef36ed9c707f073528ddafa8d83e3b8500
  - name: sized-chunks
    version: 0.6.5
    sha: 16d69225bde7a69b235da73377861095455d298f2b970996eec25ddbb42b3d1e
  - name: smallvec
    version: 1.11.0
    sha: 62bb4feee49fdd9f707ef802e22365a35de4b7b299de4763d44bfea899442ff9
  - name: socket2
    version: 0.4.9
    sha: 64a4a911eed85daf18834cfaa86a79b7d266ff93ff5ba14005426219480ed662
  - name: spki
    version: 0.7.2
    sha: 9d1e996ef02c474957d681f1b05213dfb0abab947b446a62d37770b23500184a
  - name: static_assertions
    version: 1.1.0
    sha: a2eb9349b6444b326872e140eb1cf5e7c522154d69e7a0ffb0fb81c06b37543f
  - name: strip-ansi-escapes
    version: 0.1.1
    sha: 011cbb39cf7c1f62871aea3cc46e5817b0937b49e9447370c93cacbe93a766d8
  - name: strsim
    version: 0.10.0
    sha: 73473c0e59e6d5812c5dfe2a064a6444949f089e20eec9a2e5506596494e4623
  - name: subtle
    version: 2.5.0
    sha: 81cdd64d312baedb58e21336b31bc043b77e01cc99033ce76ef539f78e965ebc
  - name: syn
    version: 1.0.109
    sha: 72b64191b275b66ffe2469e8af2c1cfe3bafa67b529ead792a6d0160888b4237
  - name: syn
    version: 2.0.29
    sha: c324c494eba9d92503e6f1ef2e6df781e78f6a7705a0202d9801b198807d518a
  - name: tar
    version: 0.4.40
    sha: b16afcea1f22891c49a00c751c7b63b2233284064f11a200fc624137c51e2ddb
  - name: tempfile
    version: 3.8.0
    sha: cb94d2f3cc536af71caac6b6fcebf65860b347e7ce0cc9ebe8f70d3e521054ef
  - name: termcolor
    version: 1.2.0
    sha: be55cf8942feac5c765c2c993422806843c9a9a45d4d5c407ad6dd2ea95eb9b6
  - name: terminal_size
    version: 0.2.6
    sha: 8e6bf6f19e9f8ed8d4048dc22981458ebcf406d67e94cd422e5ecd73d63b3237
  - name: thiserror
    version: 1.0.47
    sha: 97a802ec30afc17eee47b2855fc72e0c4cd62be9b4efe6591edde0ec5bd68d8f
  - name: thiserror-impl
    version: 1.0.47
    sha: 6bb623b56e39ab7dcd4b1b98bb6c8f8d907ed255b18de254088016b27a8ee19b
  - name: thread_local
    version: 1.1.7
    sha: 3fdd6f064ccff2d6567adcb3873ca630700f00b5ad3f060c25b5dcfd9a4ce152
  - name: time
    version: 0.3.28
    sha: 17f6bb557fd245c28e6411aa56b6403c689ad95061f50e4be16c274e70a17e48
  - name: time-core
    version: 0.1.1
    sha: 7300fbefb4dadc1af235a9cef3737cea692a9d97e1b9cbcd4ebdae6f8868e6fb
  - name: time-macros
    version: 0.2.14
    sha: 1a942f44339478ef67935ab2bbaec2fb0322496cf3cbe84b261e06ac3814c572
  - name: tinyvec
    version: 1.6.0
    sha: 87cc5ceb3875bb20c2890005a4e226a4651264a5c75edb2421b52861a0a0cb50
  - name: tinyvec_macros
    version: 0.1.1
    sha: 1f3ccbac311fea05f86f61904b462b55fb3df8837a366dfc601a0161d0532f20
  - name: toml
    version: 0.5.11
    sha: f4f7f0dd8d50a853a531c426359045b1998f04219d88799810762cd4ad314234
  - name: toml
    version: 0.7.6
    sha: c17e963a819c331dcacd7ab957d80bc2b9a9c1e71c804826d2f283dd65306542
  - name: toml_datetime
    version: 0.6.3
    sha: 7cda73e2f1397b1262d6dfdcef8aafae14d1de7748d66822d3bfeeb6d03e5e4b
  - name: toml_edit
    version: 0.19.14
    sha: f8123f27e969974a3dfba720fdb560be359f57b44302d280ba72e76a74480e8a
  - name: typenum
    version: 1.16.0
    sha: 497961ef93d974e23eb6f433eb5fe1b7930b659f06d12dec6fc44a8f554c0bba
  - name: unicase
    version: 2.7.0
    sha: f7d2d4dafb69621809a81864c9c1b864479e1235c0dd4e199924b9742439ed89
  - name: unicode-bidi
    version: 0.3.13
    sha: 92888ba5573ff080736b3648696b70cafad7d250551175acbaa4e0385b3e1460
  - name: unicode-bom
    version: 2.0.2
    sha: 98e90c70c9f0d4d1ee6d0a7d04aa06cb9bbd53d8cfbdd62a0269a7c2eb640552
  - name: unicode-ident
    version: 1.0.11
    sha: 301abaae475aa91687eb82514b328ab47a211a533026cb25fc3e519b86adfc3c
  - name: unicode-normalization
    version: 0.1.22
    sha: 5c5713f0fc4b5db668a2ac63cdb7bb4469d8c9fed047b1d0292cc7b0ce2ba921
  - name: unicode-width
    version: 0.1.10
    sha: c0edd1e5b14653f783770bce4a4dabb4a5108a5370a5f5d8cfe8710c361f6c8b
  - name: unicode-xid
    version: 0.2.4
    sha: f962df74c8c05a667b5ee8bcf162993134c104e96440b663c8daa176dc772d8c
  - name: url
    version: 2.4.0
    sha: 50bff7831e19200a85b17131d085c25d7811bc4e186efdaf54bbd132994a88cb
  - name: utf8parse
    version: 0.2.1
    sha: 711b9620af191e0cdc7468a8d14e709c3dcdb115b36f838e601583af800a370a
  - name: vcpkg
    version: 0.2.15
    sha: accd4ea62f7bb7a82fe23066fb0957d48ef677f6eeb8215f372f52e48bb32426
  - name: version_check
    version: 0.9.4
    sha: 49874b5167b65d7193b8aba1567f5c7d93d001cafc34600cee003eda787e483f
  - name: vte
    version: 0.10.1
    sha: 6cbce692ab4ca2f1f3047fcf732430249c0e971bfdd2b234cf2c47ad93af5983
  - name: vte_generate_state_changes
    version: 0.1.1
    sha: d257817081c7dffcdbab24b9e62d2def62e2ff7d00b1c20062551e6cccc145ff
  - name: walkdir
    version: 2.3.3
    sha: 36df944cda56c7d8d8b7496af378e6b16de9284591917d307c9b4d313c44e698
  - name: wasi
    version: 0.11.0+wasi-snapshot-preview1
    sha: 9c8d87e72b64a3b4db28d11ce29237c246188f4f51057d65a7eab63b7987e423
  - name: wasm-bindgen
    version: 0.2.87
    sha: 7706a72ab36d8cb1f80ffbf0e071533974a60d0a308d01a5d0375bf60499a342
  - name: wasm-bindgen-backend
    version: 0.2.87
    sha: 5ef2b6d3c510e9625e5fe6f509ab07d66a760f0885d858736483c32ed7809abd
  - name: wasm-bindgen-macro
    version: 0.2.87
    sha: dee495e55982a3bd48105a7b947fd2a9b4a8ae3010041b9e0faab3f9cd028f1d
  - name: wasm-bindgen-macro-support
    version: 0.2.87
    sha: 54681b18a46765f095758388f2d0cf16eb8d4169b639ab575a8f5693af210c7b
  - name: wasm-bindgen-shared
    version: 0.2.87
    sha: ca6ad05a4870b2bf5fe995117d3728437bd27d7cd5f06f13c17443ef369775a1
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
  - name: windows-sys
    version: 0.42.0
    sha: 5a3e1820f08b8513f676f7ab6c1f99ff312fb97b553d30ff4dd86f9f15728aa7
  - name: windows-sys
    version: 0.48.0
    sha: 677d2418bec65e3338edb076e806bc1ec15693c5d0104683f2efe857f61056a9
  - name: windows-targets
    version: 0.48.5
    sha: 9a2fa6e2155d7247be68c096456083145c183cbbbc2764150dda45a87197940c
  - name: windows_aarch64_gnullvm
    version: 0.42.2
    sha: 597a5118570b68bc08d8d59125332c54f1ba9d9adeedeef5b99b02ba2b0698f8
  - name: windows_aarch64_gnullvm
    version: 0.48.5
    sha: 2b38e32f0abccf9987a4e3079dfb67dcd799fb61361e53e2882c3cbaf0d905d8
  - name: windows_aarch64_msvc
    version: 0.42.2
    sha: e08e8864a60f06ef0d0ff4ba04124db8b0fb3be5776a5cd47641e942e58c4d43
  - name: windows_aarch64_msvc
    version: 0.48.5
    sha: dc35310971f3b2dbbf3f0690a219f40e2d9afcf64f9ab7cc1be722937c26b4bc
  - name: windows_i686_gnu
    version: 0.42.2
    sha: c61d927d8da41da96a81f029489353e68739737d3beca43145c8afec9a31a84f
  - name: windows_i686_gnu
    version: 0.48.5
    sha: a75915e7def60c94dcef72200b9a8e58e5091744960da64ec734a6c6e9b3743e
  - name: windows_i686_msvc
    version: 0.42.2
    sha: 44d840b6ec649f480a41c8d80f9c65108b92d89345dd94027bfe06ac444d1060
  - name: windows_i686_msvc
    version: 0.48.5
    sha: 8f55c233f70c4b27f66c523580f78f1004e8b5a8b659e05a4eb49d4166cca406
  - name: windows_x86_64_gnu
    version: 0.42.2
    sha: 8de912b8b8feb55c064867cf047dda097f92d51efad5b491dfb98f6bbb70cb36
  - name: windows_x86_64_gnu
    version: 0.48.5
    sha: 53d40abd2583d23e4718fddf1ebec84dbff8381c07cae67ff7768bbf19c6718e
  - name: windows_x86_64_gnullvm
    version: 0.42.2
    sha: 26d41b46a36d453748aedef1486d5c7a85db22e56aff34643984ea85514e94a3
  - name: windows_x86_64_gnullvm
    version: 0.48.5
    sha: 0b7b52767868a23d5bab768e390dc5f5c55825b6d30b86c844ff2dc7414044cc
  - name: windows_x86_64_msvc
    version: 0.42.2
    sha: 9aec5da331524158c6d1a4ac0ab1541149c0b9505fde06423b02f5ef0106b9f0
  - name: windows_x86_64_msvc
    version: 0.48.5
    sha: ed94fce61571a4006852b7389a063ab983c02eb1bb37b47f8272ce92d06d9538
  - name: winnow
    version: 0.5.15
    sha: 7c2e3184b9c4e92ad5167ca73039d0c42476302ab603e2fec4487511f38ccefc
  - name: zeroize
    version: 1.6.0
    sha: 2a0956f1ba7c7909bfb66c2e9e4124ab6f6482560f6628b5aaeba39207c9aad9
