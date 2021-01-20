kind: manual

build-depends:
- freedesktop-sdk.bst:extensions/rust/rust.bst
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
  track: 0.2.13-workaround
  ref: 0.2.13-1-geac96c836e6057f3f015e775427fd2a954667c86
- kind: patch
  path: patches/sccache-no-selenium.patch
- kind: cargo
  ref:
  - name: adler32
    version: 1.0.4
    sha: 5d2e7343e7fc9de883d1b0341e0b13970f764c14101234857d2ddafa1cb1cac2
  - name: aho-corasick
    version: 0.7.6
    sha: 58fb5e95d83b38284460a5fda7d6470aa0b8844d283a0b614b8535e880800d2d
  - name: ansi_term
    version: 0.11.0
    sha: ee49baf6cb617b853aa8d93bf420db2383fab46d314482ca2803b40d5fde979b
  - name: ar
    version: 0.6.2
    sha: 579681b3fecd1e9d6b5ce6969e05f9feb913f296eddaf595be1166a5ca597bc4
  - name: arc-swap
    version: 0.4.4
    sha: d7b8a9123b8027467bce0099fe556c628a53c8d83df0507084c31e9ba2e39aff
  - name: arrayref
    version: 0.3.5
    sha: 0d382e583f07208808f6b1249e60848879ba3543f57c32277bf52d69c2f0f0ee
  - name: arrayvec
    version: 0.5.1
    sha: cff77d8686867eceff3105329d4698d96c2391c176d5d03adc90c7389162b5b8
  - name: ascii
    version: 0.8.7
    sha: 97be891acc47ca214468e09425d02cef3af2c94d0d82081cd02061f996802f14
  - name: ascii
    version: 0.9.3
    sha: eab1c04a571841102f5345a8fc0f6bb3d31c315dec879b5c6e42e40ce7ffa34e
  - name: assert_cmd
    version: 0.9.1
    sha: c5b60c276f334145cf2cec09c5bb6f63523f078c0c850909f66bca8f933cf809
  - name: atty
    version: 0.2.13
    sha: 1803c647a3ec87095e7ae7acfca019e98de5ec9a7d01343f611cf3152ed71a90
  - name: autocfg
    version: 0.1.7
    sha: 1d49d90015b3c36167a20fe2810c5cd875ad504b39cff3d4eae7977e6b7c1cb2
  - name: backtrace
    version: 0.3.40
    sha: 924c76597f0d9ca25d762c25a4d369d51267536465dc5064bdf0eb073ed477ea
  - name: backtrace-sys
    version: 0.1.32
    sha: 5d6575f128516de27e3ce99689419835fce9643a9b215a14d2b5b685be018491
  - name: base64
    version: 0.10.1
    sha: 0b25d992356d2eb0ed82172f5248873db5560c4721f564b13cb5193bda5e668e
  - name: base64
    version: 0.11.0
    sha: b41b7ea54a0c9d92199de89e20e58d49f02f8e699814ef3fdf266f6f748d15c7
  - name: base64
    version: 0.9.3
    sha: 489d6c0ed21b11d038c31b6ceccca973e65d73ba3bd8ecb9a2babf5546164643
  - name: bincode
    version: 0.8.0
    sha: e103c8b299b28a9c6990458b7013dc4a8356a9b854c51b9883241f5866fac36e
  - name: bincode
    version: 1.2.0
    sha: b8ab639324e3ee8774d296864fbc0dbbb256cf1a41c490b94cba90c082915f92
  - name: bitflags
    version: 1.2.1
    sha: cf1de2fe8c75bc145a2f577add951f8134889b4795d47466a54a5c846d691693
  - name: blake2b_simd
    version: 0.5.9
    sha: b83b7baab1e671718d78204225800d6b170e648188ac7dc992e9d6bddf87d0c0
  - name: block-buffer
    version: 0.7.3
    sha: c0940dc441f31689269e10ac70eb1002a3a1d3ad1390e030043662eb7fe4688b
  - name: block-padding
    version: 0.1.5
    sha: fa79dedbb091f449f1f39e53edf88d5dbe95f895dae6135a8d7b881fb5af73f5
  - name: buf_redux
    version: 0.6.3
    sha: b9279646319ff816b05fb5897883ece50d7d854d12b59992683d4f8a71b0f949
  - name: bufstream
    version: 0.1.4
    sha: 40e38929add23cdf8a366df9b0e088953150724bcbe5fc330b0d8eb3b328eec8
  - name: byte-tools
    version: 0.3.1
    sha: e3b5ca7a04898ad4bcd41c90c5285445ff5b791899bb1b0abdd2a2aa791211d7
  - name: byteorder
    version: 1.3.2
    sha: a7c3dd8985a7111efc5c80b44e23ecdd8c007de8ade3b96595387e812b957cf5
  - name: bytes
    version: 0.4.12
    sha: 206fdffcfa2df7cbe15601ef46c813fce0965eb3286db6b56c583b814b51c81c
  - name: c2-chacha
    version: 0.2.3
    sha: 214238caa1bf3a496ec3392968969cab8549f96ff30652c9e56885329315f6bb
  - name: case
    version: 0.1.0
    sha: e88b166b48e29667f5443df64df3c61dc07dc2b1a0b0d231800e07f09a33ecc1
  - name: cc
    version: 1.0.48
    sha: f52a465a666ca3d838ebbf08b241383421412fe7ebb463527bba275526d89f76
  - name: cfg-if
    version: 0.1.10
    sha: 4785bdd1c96b2a846b2bd7cc02e86b6b3dbf14e7e53446c4f54c92a361040822
  - name: chrono
    version: 0.4.10
    sha: 31850b4a4d6bae316f7a09e691c944c28299298837edc0a03f755618c23cbc01
  - name: chunked_transfer
    version: 0.3.1
    sha: 498d20a7aaf62625b9bf26e637cf7736417cde1d0c99f1d04d1170229a85cf87
  - name: clap
    version: 2.33.0
    sha: 5067f5bb2d80ef5d68b4c87db81601f0b75bca627bc2ef76b141d7b846a3c6d9
  - name: cloudabi
    version: 0.0.3
    sha: ddfc5b9aa5d4507acaf872de71051dfd0e309860e88966e1051e462a077aac4f
  - name: combine
    version: 3.8.1
    sha: da3da6baa321ec19e1cc41d31bf599f00c783d0517095cdaf0332e3fe8d20680
  - name: conhash
    version: 0.4.0
    sha: 99d6364d028778d0d98b6014fa5882da377cd10d3492b7734d266a428e9b1fca
  - name: constant_time_eq
    version: 0.1.4
    sha: 995a44c877f9212528ccc74b21a232f66ad69001e40ede5bcee2ac9ef2657120
  - name: cookie
    version: 0.12.0
    sha: 888604f00b3db336d2af898ec3c1d5d0ddf5e6d462220f2ededc33a87ac4bbd5
  - name: cookie_store
    version: 0.7.0
    sha: 46750b3f362965f197996c4448e4a0935e791bf7d6631bfce9ee0af3d24c919c
  - name: core-foundation
    version: 0.6.4
    sha: 25b9e03f145fd4f2bf705e07b900cd41fc636598fe5dc452fd0db1441c3f496d
  - name: core-foundation-sys
    version: 0.6.2
    sha: e7ca8a5221364ef15ce201e8ed2f609fc312682a8f4e0e3d4aa5879764e0fa3b
  - name: counted-array
    version: 0.1.2
    sha: 384f8c53175c890920b6e0127b730709d2a173ca6c4dfdc81618ac9b46f648fe
  - name: crc32fast
    version: 1.2.0
    sha: ba125de2af0df55319f41944744ad91c71113bf74a4646efff39afe1f6842db1
  - name: crossbeam-deque
    version: 0.7.2
    sha: c3aa945d63861bfe624b55d153a39684da1e8c0bc8fba932f7ee3a3c16cea3ca
  - name: crossbeam-epoch
    version: 0.8.0
    sha: 5064ebdbf05ce3cb95e45c8b086f72263f4166b29b97f6baff7ef7fe047b55ac
  - name: crossbeam-queue
    version: 0.1.2
    sha: 7c979cd6cfe72335896575c6b5688da489e420d36a27a0b9eb0c73db574b4a4b
  - name: crossbeam-utils
    version: 0.5.0
    sha: 677d453a17e8bd2b913fa38e8b9cf04bcdbb5be790aa294f2389661d72036015
  - name: crossbeam-utils
    version: 0.6.6
    sha: 04973fa96e96579258a5091af6003abde64af786b860f18622b82e026cca60e6
  - name: crossbeam-utils
    version: 0.7.0
    sha: ce446db02cdc3165b94ae73111e570793400d0794e46125cc4056c81cbb039f4
  - name: crypto-mac
    version: 0.7.0
    sha: 4434400df11d95d556bac068ddfedd482915eb18fe8bea89bc80b6e4b1c179e5
  - name: daemonize
    version: 0.3.0
    sha: 4093d27eb267d617f03c2ee25d4c3ca525b89a76154001954a11984508ffbde5
  - name: derive-error
    version: 0.0.3
    sha: 629f1bb3abce791912ca85a24676fff54464f7deb122906adabc90fb96e876d3
  - name: difference
    version: 2.0.0
    sha: 524cbf6897b527295dff137cec09ecf3a05f4fddffd7dfcd1585403449e74198
  - name: digest
    version: 0.8.1
    sha: f3d0c8c8752312f9713efd397ff63acb9f85585afbf179282e720e7704954dd5
  - name: directories
    version: 1.0.2
    sha: 72d337a64190607d4fcca2cb78982c5dd57f4916e19696b48a575fa746b6cb0f
  - name: dirs
    version: 1.0.5
    sha: 3fd78930633bd1c6e35c4b42b1df7b0cbc6bc191146e512bb3bedf243fcc3901
  - name: dtoa
    version: 0.4.4
    sha: ea57b42383d091c85abcc2706240b94ab2a8fa1fc81c10ff23c4de06e2a90b5e
  - name: either
    version: 1.5.3
    sha: bb1f6b1ce1c140482ea30ddd3335fc0024ac7ee112895426e0a629a6c20adfe3
  - name: encoding_rs
    version: 0.8.20
    sha: 87240518927716f79692c2ed85bfe6e98196d18c6401ec75355760233a7e12e9
  - name: env_logger
    version: 0.5.13
    sha: 15b0a4d2e39f8420210be8b27eeda28029729e2fd4291019455016c348240c38
  - name: error-chain
    version: 0.11.0
    sha: ff511d5dc435d703f4971bc399647c9bc38e20cb41452e3b9feb4765419ed3f3
  - name: error-chain
    version: 0.12.1
    sha: 3ab49e9dcb602294bc42f9a7dfc9bc6e936fca4418ea300dbfb84fe16de0b7d9
  - name: escargot
    version: 0.3.1
    sha: 19db1f7e74438642a5018cdf263bb1325b2e792f02dd0a3ca6d6c0f0d7b1d5a5
  - name: failure
    version: 0.1.6
    sha: f8273f13c977665c5db7eb2b99ae520952fe5ac831ae4cd09d80c4c7042b5ed9
  - name: failure_derive
    version: 0.1.6
    sha: 0bc225b78e0391e4b8683440bf2e63c2deeeb2ce5189eab46e2b68c6d3725d08
  - name: fake-simd
    version: 0.1.2
    sha: e88a8acf291dafb59c2d96e8f59828f3838bb1a70398823ade51a84de6a6deed
  - name: filetime
    version: 0.1.15
    sha: 714653f3e34871534de23771ac7b26e999651a0a228f47beb324dfdf1dd4b10f
  - name: filetime
    version: 0.2.8
    sha: 1ff6d4dab0aa0c8e6346d46052e93b13a16cf847b54ed357087c35011048cc7d
  - name: flate2
    version: 1.0.13
    sha: 6bd6d6f4752952feb71363cffc9ebac9411b75b87c6ab6058c40c8900cf43c0f
  - name: float-cmp
    version: 0.4.0
    sha: 134a8fa843d80a51a5b77d36d42bc2def9edcb0262c914861d08129fd1926600
  - name: fnv
    version: 1.0.6
    sha: 2fad85553e09a6f881f739c29f0b00b0f01357c743266d478b68951ce23285f3
  - name: foreign-types
    version: 0.3.2
    sha: f6f339eb8adc052cd2ca78910fda869aefa38d22d5cb648e6485e4d3fc06f3b1
  - name: foreign-types-shared
    version: 0.1.1
    sha: 00b0228411908ca8685dba7fc2cdd70ec9990a6e753e89b6ac91a84c40fbaf4b
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
    version: 0.1.29
    sha: 1b980f2816d6ee8673b6517b52cb0e808a180efc92e5c19d02cdda79066703ef
  - name: futures-cpupool
    version: 0.1.8
    sha: ab90cde24b3319636588d0c35fe03b1333857621051837ed769faefb4c2162e4
  - name: generic-array
    version: 0.12.3
    sha: c68f0274ae0e023facc3c97b2e00f076be70e254bc851d972503b328db79b2ec
  - name: getopts
    version: 0.2.21
    sha: 14dbbfd5c71d70241ecf9e6f13737f7b5ce823821063188d7e46c41d371eebd5
  - name: getrandom
    version: 0.1.13
    sha: e7db7ca94ed4cd01190ceee0d8a8052f08a247aa1b469a7f68c6a3b71afcf407
  - name: h2
    version: 0.1.26
    sha: a5b34c246847f938a410a03c5458c7fee2274436675e76d8b903c08efc29c462
  - name: hermit-abi
    version: 0.1.3
    sha: 307c3c9f937f38e3534b1d6447ecf090cafcc9744e4a6360e8b037b2cf5af120
  - name: hmac
    version: 0.7.1
    sha: 5dcb5e64cda4c23119ab41ba960d1e170a774c8e4b9d9e6a9bc18aabf5e59695
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
    version: 1.3.0
    sha: df004cfca50ef23c36850aaaa59ad52cc70d0e90243c3c7737a4dd32dc7a3c4f
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
    version: 1.3.0
    sha: 712d7b3ea5827fcb9d4fda14bf4da5f136f0db2ae9c8f4bd4e2d1c6fde4e6db2
  - name: iovec
    version: 0.1.4
    sha: b2b3ea6ff95e175473f8ffe6a7eb7c00d054240321b84c57051175fe3c1e075e
  - name: itertools
    version: 0.7.11
    sha: 0d47946d458e94a1b7bcabbf6521ea7c037062c81f534615abcad76e84d4970d
  - name: itoa
    version: 0.4.4
    sha: 501266b7edd0174f8530248f87f99c88fbe60ca4ef3dd486835b8d8d53136f7f
  - name: jobserver
    version: 0.1.19
    sha: 67b06c1b455f1cf4269a8cfc320ab930a810e2375a42af5075eb8a8b36405ce0
  - name: jsonwebtoken
    version: 6.0.1
    sha: a81d1812d731546d2614737bee92aa071d37e9afa1409bc374da9e5e70e70b22
  - name: kernel32-sys
    version: 0.2.2
    sha: 7507624b29483431c0ba2d82aece8ca6cdba9382bff4ddd0f7490560c056098d
  - name: language-tags
    version: 0.2.2
    sha: a91d884b6667cd606bb5a69aa0c99ba811a115fc68915e7056ec08a46e93199a
  - name: lazy_static
    version: 1.4.0
    sha: e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
  - name: libc
    version: 0.2.66
    sha: d515b1f41455adea1313a4a2ac8a8a477634fbae63cc6100e3aebb207ce61558
  - name: libmount
    version: 0.1.15
    sha: 23c4c2ad2d5cbd2f5a05620c3daf45930add53ec207fa99ce5eec971089dc35f
  - name: libz-sys
    version: 1.0.25
    sha: 2eb5e43362e38e2bca2fd5f5134c4d4564a23a5c28e9b95411652021a8675ebe
  - name: linked-hash-map
    version: 0.2.1
    sha: bda158e0dabeb97ee8a401f4d17e479d6b891a14de0bba79d5cc2d4d325b5e48
  - name: local-encoding
    version: 0.2.0
    sha: e1ceb20f39ff7ae42f3ff9795f3986b1daad821caaa1e1732a0944103a5a1a66
  - name: lock_api
    version: 0.3.2
    sha: e57b3997725d2b60dbec1297f6c2e2957cc383db1cebd6be812163f969c7d586
  - name: log
    version: 0.3.9
    sha: e19e8d5c34a3e0e2223db8e060f9e8264aeeb5c5fc64a4ee9965c062211c024b
  - name: log
    version: 0.4.8
    sha: 14b6052be84e6b71ab17edffc2eeabf5c2c3ae1fdb464aae35ac50c67a44e1f7
  - name: matches
    version: 0.1.8
    sha: 7ffc5c5338469d4d3ea17d269fa8ea3512ad247247c30bd2df69e68309ed0a08
  - name: maybe-uninit
    version: 2.0.0
    sha: 60302e4db3a61da70c0cb7991976248362f30319e88850c487b9b95bbf059e00
  - name: md-5
    version: 0.8.0
    sha: a18af3dcaf2b0219366cdb4e2af65a6101457b415c3d1a5c71dd9c2b7c77b9c8
  - name: md5
    version: 0.3.8
    sha: 79c56d6a0b07f9e19282511c83fc5b086364cbae4ba8c7d5f190c3d9b0425a48
  - name: memcached-rs
    version: 0.3.0
    sha: 0f45690519137679de66b2bdb60e37a0d4f17adde980930a05eef66929394f73
  - name: memchr
    version: 1.0.2
    sha: 148fab2e51b4f1cfc66da2a7c32981d1d3c083a803978268bb11fe4b86925e7a
  - name: memchr
    version: 2.2.1
    sha: 88579771288728879b57485cc7d6b07d648c9f0141eb955f8ab7f9d45394468e
  - name: memoffset
    version: 0.5.3
    sha: 75189eb85871ea5c2e2c15abbdd541185f63b408415e5051f5cac122d8c774b9
  - name: mime
    version: 0.2.6
    sha: ba626b8a6de5da682e1caa06bdb42a335aee5a84db8e5046a3e8ab17ba0a3ae0
  - name: mime
    version: 0.3.14
    sha: dd1d63acd1b78403cc0c325605908475dd9b9a3acbf65ed8bcab97e27014afcf
  - name: mime_guess
    version: 1.8.7
    sha: 0d977de9ee851a0b16e932979515c0f3da82403183879811bc97d50bd9cc50f7
  - name: mime_guess
    version: 2.0.1
    sha: 1a0ed03949aef72dbdf3116a383d7b38b4768e6f960528cd6a6044aa9ed68599
  - name: miniz_oxide
    version: 0.3.5
    sha: 6f3f74f726ae935c3f514300cc6773a0c9492abc5e972d42ba0c0ebb88757625
  - name: mio
    version: 0.6.21
    sha: 302dec22bcf6bae6dfb69c647187f4b4d0fb6f535521f7bc022430ce8e12008f
  - name: mio-named-pipes
    version: 0.1.6
    sha: f5e374eff525ce1c5b7687c4cef63943e7686524a387933ad27ca7ec43779cb3
  - name: mio-uds
    version: 0.6.7
    sha: 966257a94e196b11bb43aca423754d87429960a768de9414f3691d6957abf125
  - name: miow
    version: 0.2.1
    sha: 8c1f2f3b1cf331de6896aabf6e9d55dca90356cc9960cca7eaaf408a355ae919
  - name: miow
    version: 0.3.3
    sha: 396aa0f2003d7df8395cb93e09871561ccc3e785f0acb369170e8cc74ddf9226
  - name: msdos_time
    version: 0.1.6
    sha: aad9dfe950c057b1bfe9c1f2aa51583a8468ef2a5baba2ebbe06d775efeb7729
  - name: multipart
    version: 0.13.6
    sha: 92f54eb45230c3aa20864ccf0c277eeaeadcf5e437e91731db498dbf7fbe0ec6
  - name: native-tls
    version: 0.2.3
    sha: 4b2df1a4c22fd44a62147fd8f13dd0f95c9d8ca7b2610299b2a2f9cf8964274e
  - name: net2
    version: 0.2.33
    sha: 42550d9fb7b6684a6d404d9fa7250c2eb2646df731d1c06afc06dcee9e1bcf88
  - name: nix
    version: 0.11.1
    sha: becb657d662f1cd2ef38c7ad480ec6b8cf9e96b27adb543e594f9cf0f2e6065c
  - name: nix
    version: 0.14.1
    sha: 6c722bee1037d430d0f8e687bbdbf222f27cc6e4e68d5caf630857bb2b6dbdce
  - name: normalize-line-endings
    version: 0.2.2
    sha: 2e0a1a39eab95caf4f5556da9289b9e68f0aafac901b2ce80daaf020d3b733a8
  - name: num-integer
    version: 0.1.41
    sha: b85e541ef8255f6cf42bbfe4ef361305c6c135d10919ecc26126c4e5ae94bc09
  - name: num-traits
    version: 0.1.43
    sha: 92e5113e9fd4cc14ded8e499429f396a20f98c772a47cc8622a736e1ec843c31
  - name: num-traits
    version: 0.2.10
    sha: d4c81ffc11c212fa327657cb19dd85eb7419e163b5b076bede2bdb5c974c07e4
  - name: num_cpus
    version: 1.11.1
    sha: 76dac5ed2a876980778b8b85f75a71b6cbf0db0b1232ee12f826bccb00d09d72
  - name: number_prefix
    version: 0.2.8
    sha: dbf9993e59c894e3c08aa1c2712914e9e6bf1fcbfc6bef283e2183df345a4fee
  - name: opaque-debug
    version: 0.2.3
    sha: 2839e79665f131bdb5782e51f2c6c9599c133c6098982a54c794358bf432529c
  - name: openssl
    version: 0.10.26
    sha: 3a3cc5799d98e1088141b8e01ff760112bbd9f19d850c124500566ca6901a585
  - name: openssl-probe
    version: 0.1.2
    sha: 77af24da69f9d9341038eba93a073b1fdaaa1b788221b00a69bce9e762cb32de
  - name: openssl-sys
    version: 0.9.53
    sha: 465d16ae7fc0e313318f7de5cecf57b2fbe7511fd213978b457e1c96ff46736f
  - name: parking_lot
    version: 0.9.0
    sha: f842b1982eb6c2fe34036a4fbfb06dd185a3f5c8edfaacdf7d1ea10b07de6252
  - name: parking_lot_core
    version: 0.6.2
    sha: b876b1b9e7ac6e1a74a6da34d25c42e17e8862aa409cbbbdcfc8d86c6f3bc62b
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
  - name: pkg-config
    version: 0.3.17
    sha: 05da548ad6865900e60eaba7f589cc0783590a92e940c26953ff81ddbab2d677
  - name: podio
    version: 0.1.6
    sha: 780fb4b6698bbf9cf2444ea5d22411cef2953f0824b98f33cf454ec5615645bd
  - name: ppv-lite86
    version: 0.2.6
    sha: 74490b50b9fbe561ac330df47c08f3f33073d2d00c150f719147d7c54522fa1b
  - name: predicates
    version: 0.9.1
    sha: f31e7977fc111984fdac76b6ae3a4cb598008fc6fd02dfdca189bf180bd7be20
  - name: predicates-core
    version: 0.9.0
    sha: 85f80bc390d1c02a4cdaa63f27f05c3c426679eb65433d8dd65d392147e4e5c5
  - name: predicates-tree
    version: 0.9.0
    sha: 2e86df9b81bdcb0a5141aca9d2b9c5e0c558ef6626d3ae2c12912f5c9df740bd
  - name: proc-macro2
    version: 1.0.6
    sha: 9c9e470a8dc4aeae2dee2f335e8f533e2d4b347e1434e5671afc49b054592f27
  - name: publicsuffix
    version: 1.5.4
    sha: 3bbaa49075179162b49acac1c6aa45fb4dafb5f13cf6794276d77bc7fd95757b
  - name: pulldown-cmark
    version: 0.0.3
    sha: 8361e81576d2e02643b04950e487ec172b687180da65c731c03cf336784e6c07
  - name: quick-error
    version: 1.2.2
    sha: 9274b940887ce9addde99c4eee6b5c44cc494b182b97e73dc8ffdcb3397fd3f0
  - name: quote
    version: 0.3.15
    sha: 7a6e920b65c65f10b2ae65c831a81a073a89edd28c7cce89475bff467ab4167a
  - name: quote
    version: 1.0.2
    sha: 053a8c8bcc71fcce321828dc897a98ab9760bef03a4fc36693c231e5b3216cfe
  - name: rand
    version: 0.3.23
    sha: 64ac302d8f83c0c1974bf758f6b041c6c8ada916fbb44a609158ca8b064cc76c
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
    version: 0.7.2
    sha: 3ae1b169243eaf61759b8475a998f0a385e42042370f3a7dbaf35246eacc8412
  - name: rand_chacha
    version: 0.1.1
    sha: 556d3a1ca6600bfcbab7c7c91ccb085ac7fbbcd70e008a98742e7847f4f7bcef
  - name: rand_chacha
    version: 0.2.1
    sha: 03a2a90da8c7523f554344f921aa97283eadf6ac484a6d2a7d0212fa7f8d6853
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
    version: 0.9.1
    sha: f0c747d743d48233f9bc3ed3fb00cb84c1d98d8c7f54ed2d4cca9adf461a7ef3
  - name: redox_syscall
    version: 0.1.56
    sha: 2439c63f3f6139d1b57529d16bc3b8bb855230c8efcc5d3a896c8bea7c3b1e84
  - name: redox_users
    version: 0.3.1
    sha: 4ecedbca3bf205f8d8f5c2b44d83cd0690e39ee84b951ed649e9f1841132b66d
  - name: regex
    version: 1.3.1
    sha: dc220bd33bdce8f093101afe22a037b8eb0e5af33592e6a9caafff0d4cb81cbd
  - name: regex-syntax
    version: 0.6.12
    sha: 11a7e20d1cce64ef2fed88b66d347f88bd9babb82845b2b858f3edbf59a4f716
  - name: remove_dir_all
    version: 0.5.2
    sha: 4a83fa3702a688b9359eccba92d153ac33fd2e8462f9e0e3fdf155239ea7792e
  - name: reqwest
    version: 0.9.22
    sha: 2c2064233e442ce85c77231ebd67d9eca395207dec2127fe0bbedde4bd29a650
  - name: retry
    version: 0.4.0
    sha: 29460f6011a25fc70b22010e796bd98330baccaa0005cba6f90b858a510dec0d
  - name: ring
    version: 0.14.6
    sha: 426bc186e3e95cac1e4a4be125a4aca7e84c2d616ffc02244eef36e2a60a093c
  - name: rouille
    version: 2.2.0
    sha: 0845b9c39ba772da769fe2aaa4d81bfd10695a7ea051d0510702260ff4159841
  - name: rust-argon2
    version: 0.5.1
    sha: 4ca4eaef519b494d1f2848fc602d18816fed808a981aedf4f1f00ceb7c9d32cf
  - name: rustc-demangle
    version: 0.1.16
    sha: 4c691c0e608126e00913e33f0ccf3727d5fc84573623b8d65b2df340b5201783
  - name: rustc_version
    version: 0.2.3
    sha: 138e3e0acb6c9fb258b19b67cb8abd63c00679d2851805ea151465464fe9030a
  - name: ryu
    version: 1.0.2
    sha: bfa8506c1de11c9c4e4c38863ccbe02a305c8188e85a05a784c9e11e1c3910c8
  - name: safemem
    version: 0.2.0
    sha: e27a8b19b835f7aea908818e871f5cc3a5a186550c30773be987e155e8163d8f
  - name: safemem
    version: 0.3.3
    sha: ef703b7cb59335eae2eb93ceb664c0eb7ea6bf567079d843e09420219668e072
  - name: same-file
    version: 0.1.3
    sha: d931a44fdaa43b8637009e7632a02adc4f2b2e0733c08caa4cf00e8da4a117a7
  - name: schannel
    version: 0.1.16
    sha: 87f550b06b6cba9c8b8be3ee73f391990116bf527450d2556e9b9ce263b9a021
  - name: scopeguard
    version: 1.0.0
    sha: b42e15e59b18a828bbf5c58ea01debb36b9b096346de35d941dcb89009f24a0d
  - name: security-framework
    version: 0.3.4
    sha: 8ef2429d7cefe5fd28bd1d2ed41c944547d4ff84776f5935b456da44593a16df
  - name: security-framework-sys
    version: 0.3.3
    sha: e31493fc37615debb8c5090a7aeb4a9730bc61e77ab10b9af59f1a202284f895
  - name: semver
    version: 0.9.0
    sha: 1d7eb9ef2c18661902cc47e535f9bc51b78acd254da71d375c2f6720d9a40403
  - name: semver-parser
    version: 0.7.0
    sha: 388a1df253eca08550bef6c72392cfe7c30914bf41df5269b68cbd6ff8f570a3
  - name: serde
    version: 1.0.103
    sha: 1217f97ab8e8904b57dd22eb61cde455fa7446a9c1cf43966066da047c1f3702
  - name: serde_derive
    version: 1.0.103
    sha: a8c6faef9a2e64b0064f48570289b4bf8823b7581f1d6157c1b52152306651d0
  - name: serde_json
    version: 1.0.44
    sha: 48c575e0cc52bdd09b47f330f646cf59afc586e9c4e3ccd6fc1f625b8ea1dad7
  - name: serde_urlencoded
    version: 0.5.5
    sha: 642dd69105886af2efd227f75a520ec9b44a820d65bc133a9131f7d229fd165a
  - name: sha-1
    version: 0.8.1
    sha: 23962131a91661d643c98940b20fcaffe62d776a823247be80a48fcb8b6fce68
  - name: sha1
    version: 0.6.0
    sha: 2579985fda508104f7587689507983eadd6a6e84dd35d6d115361f530916fa0d
  - name: sha2
    version: 0.8.0
    sha: 7b4d8bfd0e469f417657573d8451fb33d16cfe0989359b93baf3a1ffc639543d
  - name: signal-hook
    version: 0.1.12
    sha: 7a9c17dd3ba2d36023a5c9472ecddeda07e27fd0b05436e8c1e0c8f178185652
  - name: signal-hook-registry
    version: 1.2.0
    sha: 94f478ede9f64724c5d173d7bb56099ec3e2d9fc2774aac65d34b8b890405f41
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
  - name: smallvec
    version: 1.0.0
    sha: 4ecf3b85f68e8abaa7555aa5abdb1153079387e60b718283d732f03897fcfc86
  - name: socket2
    version: 0.3.11
    sha: e8b74de517221a2cb01a53349cf54182acdc31a074727d3079068448c0676d85
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
    version: 1.0.0
    sha: 2d67a5a62ba6e01cb2192ff309324cb4875d0c451d55fe2319433abe7a05a8ee
  - name: syn
    version: 0.11.11
    sha: d3b891b9015c88c576343b9b3e41c2c11a51c219ef067b264bd9c8aa9b441dad
  - name: syn
    version: 1.0.11
    sha: dff0acdb207ae2fe6d5976617f887eb1e35a2ba52c13c7234c790960cdad9238
  - name: synom
    version: 0.11.3
    sha: a393066ed9010ebaed60b9eafa373d4b1baac186dd7e008555b0f702b51945b6
  - name: synstructure
    version: 0.12.3
    sha: 67656ea1dc1b41b1451851562ea232ec2e5a80242139f7e679ceccfb5d61f545
  - name: syslog
    version: 4.0.1
    sha: a0641142b4081d3d44beffa4eefd7346a228cdf91ed70186db2ca2cef762d327
  - name: tar
    version: 0.4.26
    sha: b3196bfbffbba3e57481b6ea32249fbaf590396a52505a2615adbb79d9d826d3
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
    version: 1.0.5
    sha: 96d6098003bde162e4277c70665bd87c326f5a0c3f3fbfb285787fa482d54e6e
  - name: textwrap
    version: 0.11.0
    sha: d326610f408c7a4eb6f51c37c330e496b08506c9457c9d34287ecc38809fb060
  - name: thread_local
    version: 0.3.6
    sha: c6b53e329000edc2b34dbe8545fd20e55a333362d0a321909685a19bd28c3f1b
  - name: threadpool
    version: 1.7.1
    sha: e2f0c90a5f3459330ac8bc0d2f879c693bb7a2f59689c1083fc4ef83834da865
  - name: time
    version: 0.1.42
    sha: db8dcfca086c1143c9270ac42a2bbd8a7ee477b78ac8e45b19abfb0cbede4b6f
  - name: tiny_http
    version: 0.6.2
    sha: 1661fa0a44c95d01604bd05c66732a446c657efb62b5164a7a083a3b552b4951
  - name: tokio
    version: 0.1.22
    sha: 5a09c0b5bb588872ab2f09afa13ee6e9dac11e10a0ec9e8e3ba39a5a5d530af6
  - name: tokio-buf
    version: 0.1.1
    sha: 8fb220f46c53859a4b7ec083e41dec9778ff0b1851c0942b211edb89e0ccdc46
  - name: tokio-codec
    version: 0.1.1
    sha: 5c501eceaf96f0e1793cf26beb63da3d11c738c4a943fdf3746d81d64684c39f
  - name: tokio-current-thread
    version: 0.1.6
    sha: d16217cad7f1b840c5a97dfb3c43b0c871fef423a6e8d2118c604e843662a443
  - name: tokio-executor
    version: 0.1.9
    sha: ca6df436c42b0c3330a82d855d2ef017cd793090ad550a6bc2184f4b933532ab
  - name: tokio-fs
    version: 0.1.6
    sha: 3fe6dc22b08d6993916647d108a1a7d15b9cd29c4f4496c62b92c45b5041b7af
  - name: tokio-io
    version: 0.1.12
    sha: 5090db468dad16e1a7a54c8c67280c5e4b544f3d3e018f0b913b400261f85926
  - name: tokio-named-pipes
    version: 0.1.0
    sha: 9d282d483052288b2308ba5ee795f5673b159c9bdf63c385a05609da782a5eae
  - name: tokio-process
    version: 0.2.4
    sha: afbd6ef1b8cc2bd2c2b580d882774d443ebb1c6ceefe35ba9ea4ab586c89dbe8
  - name: tokio-reactor
    version: 0.1.11
    sha: 6732fe6b53c8d11178dcb77ac6d9682af27fc6d4cb87789449152e5377377146
  - name: tokio-serde
    version: 0.1.0
    sha: 894168193c4f80862a2244ff953b69145a9961a9efba39500e0970b083d0649c
  - name: tokio-serde-bincode
    version: 0.1.1
    sha: 02e35c8d60a5e87cfb30dd562a309e56f8a6d36617b0a76c87f04d5466607ca8
  - name: tokio-signal
    version: 0.2.7
    sha: dd6dc5276ea05ce379a16de90083ec80836440d5ef8a6a39545a3207373b8296
  - name: tokio-sync
    version: 0.1.7
    sha: d06554cce1ae4a50f42fba8023918afa931413aded705b560e29600ccf7c6d76
  - name: tokio-tcp
    version: 0.1.3
    sha: 1d14b10654be682ac43efee27401d792507e30fd8d26389e1da3b185de2e4119
  - name: tokio-threadpool
    version: 0.1.17
    sha: f0c32ffea4827978e9aa392d2f743d973c1dfa3730a2ed3f22ce1e6984da848c
  - name: tokio-timer
    version: 0.2.12
    sha: 1739638e364e558128461fc1ad84d997702c8e31c2e6b18fb99842268199e827
  - name: tokio-udp
    version: 0.1.5
    sha: f02298505547f73e60f568359ef0d016d5acd6e830ab9bc7c4a5b3403440121b
  - name: tokio-uds
    version: 0.2.5
    sha: 037ffc3ba0e12a0ab4aca92e5234e0dedeb48fddf6ccd260f1f150a36a9f2445
  - name: toml
    version: 0.4.10
    sha: 758664fc71a3a69038656bee8b6be6477d2a6c315a6b81f7081f591bffa4111f
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
    version: 0.1.1
    sha: 2d990c5b6c0e4e192db8cf3dacaafefe1278962d0ec45dc84421175db32d33f0
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
    version: 0.1.10
    sha: ff4e4f59e752cb3beb5b61c6d5e11191c7946231ba84faec2902c9efdd8691c5
  - name: tracing-attributes
    version: 0.1.5
    sha: a4263b12c3d3c403274493eb805966093b53214124796552d674ca1dd5d27c2b
  - name: tracing-core
    version: 0.1.7
    sha: bc913647c520c959b6d21e35ed8fa6984971deca9f0a2fcb8c51207e0c56af1d
  - name: treeline
    version: 0.1.0
    sha: a7f741b240f1a48843f9b8e0444fb55fb2a4ff67293b50a9179dfd5ea67f8d41
  - name: try-lock
    version: 0.2.2
    sha: e604eb7b43c06650e854be16a2a03155743d3752dd1c943f6829e26b7a36e382
  - name: try_from
    version: 0.3.2
    sha: 283d3b89e1368717881a9d51dad843cc435380d8109c9e47d38780a324698d8b
  - name: twoway
    version: 0.1.8
    sha: 59b11b2b5241ba34be09c3cc85a36e56e48f9888862e19cedf23336d35316ed1
  - name: typenum
    version: 1.11.2
    sha: 6d2783fe2d6b8c1101136184eb41be8b1ad379e4657050b8aaff0c79ee7575f9
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
    version: 0.1.11
    sha: b561e267b2326bb4cebfc0ef9e68355c7abe6c6f522aeac2f5bf95d56c59bdcf
  - name: unicode-width
    version: 0.1.7
    sha: caaa9d531767d1ff2150b9332433f32a24622147e5ebb1f26409d5da67afd479
  - name: unicode-xid
    version: 0.0.4
    sha: 8c1f860d7d29cf02cb2f3f359fd35991af3d30bac52c57d265a3c461074cb4dc
  - name: unicode-xid
    version: 0.2.0
    sha: 826e7639553986605ec5979c7dd957c7895e93eabed50ab2ffa7f6128a75097c
  - name: unix_socket
    version: 0.5.0
    sha: 6aa2700417c405c38f5e6902d699345241c28c0b7ade4abaad71e35a87eb1564
  - name: unreachable
    version: 1.0.0
    sha: 382810877fe448991dfc7f0dd6e3ae5d58088fd0ea5e35189655f84e6814fa56
  - name: untrusted
    version: 0.6.2
    sha: 55cd1f4b4e96b46aeb8d4855db4a7a9bd96eeeb5c6a1ab54593328761642ce2f
  - name: url
    version: 1.7.2
    sha: dd4e7c0d531266369519a4aa4f399d748bd37043b00bde1e4ff1f60a120b355a
  - name: url
    version: 2.1.0
    sha: 75b414f6c464c879d7f9babf951f23bc3743fb7313c081b2e6ca719067ea9d61
  - name: utf8parse
    version: 0.1.1
    sha: 8772a4ccbb4e89959023bc5b7cb8623a795caa7092d99f3aa9501b9484d4557d
  - name: uuid
    version: 0.7.4
    sha: 90dbc611eb48397705a6b0f6e917da23ae517e4d127123d2cf7674206627d32a
  - name: vcpkg
    version: 0.2.8
    sha: 3fc439f2794e98976c88a2a2dafce96b930fe8010b0a256b3c2199a773933168
  - name: vec_map
    version: 0.8.1
    sha: 05c78687fb1a80548ae3250346c3db86a80a7cdd77bda190189f2d0a0987c81a
  - name: version-compare
    version: 0.0.10
    sha: d63556a25bae6ea31b52e640d7c41d1ab27faba4ccb600013837a3d0b3994ca1
  - name: version_check
    version: 0.1.5
    sha: 914b1a6776c4c929a602fafd8bc742e06365d4bcbe48c30f9cca5824f70dc9dd
  - name: version_check
    version: 0.9.1
    sha: 078775d0255232fb988e6fccf26ddc9d1ac274299aaedcedce21c6f72cc533ce
  - name: void
    version: 1.0.2
    sha: 6a02e4885ed3bc0f2de90ea6dd45ebcbb66dacffe03547fadbb0eeae2770887d
  - name: vte
    version: 0.3.3
    sha: 4f42f536e22f7fcbb407639765c8fd78707a33109301f834a594758bedd6e8cf
  - name: walkdir
    version: 1.0.7
    sha: bb08f9e670fab86099470b97cd2b252d6527f0b3cc1401acdb595ffc9dd288ff
  - name: want
    version: 0.2.0
    sha: b6395efa4784b027708f7451087e647ec73cc74f5d9bc2e418404248d679a230
  - name: wasi
    version: 0.7.0
    sha: b89c3ce4ce14bdc6fb6beaf9ec7928ca331de5df7e5ea278375642a2f478570d
  - name: which
    version: 2.0.1
    sha: b57acb10231b9493c8472b20cb57317d0679a49e0bdbee44b3b803a6473af164
  - name: winapi
    version: 0.2.8
    sha: 167dc9d6949a9b857f3451275e911c3f44255842c1f7a76f33c55103a909087a
  - name: winapi
    version: 0.3.8
    sha: 8093091eeb260906a183e6ae1abdba2ef5ef2257a21801128899c3fc699229c6
  - name: winapi-build
    version: 0.1.1
    sha: 2d315eee3b34aca4797b2da6b13ed88266e6d612562a0c46390af8299fc699bc
  - name: winapi-i686-pc-windows-gnu
    version: 0.4.0
    sha: ac3b87c63620426dd9b991e5ce0329eff545bccbbb34f3be09ff6fb6ab51b7b6
  - name: winapi-util
    version: 0.1.2
    sha: 7168bab6e1daee33b4557efd0e95d5ca70a03706d39fa5f3fe7a236f584b03c9
  - name: winapi-x86_64-pc-windows-gnu
    version: 0.4.0
    sha: 712e227841d057c1ee1cd2fb22fa7e5a5461ae8e48fa2ca79ec42cfc1931183f
  - name: wincolor
    version: 1.0.2
    sha: 96f5016b18804d24db43cebf3c77269e7569b8954a8464501c216cc5e070eaa9
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
    version: 0.4.2
    sha: 36b9e08fb518a65cf7e08a1e482573eb87a2f4f8c6619316612a3c1f162fe822
