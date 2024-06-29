# Fuzzing in WebRTC

## Intro
WebRTC currently uses libfuzzer for fuzz testing however FuzzTest is a new approach which we have not yet looked into but we will in the future.

Before continuing, read the [libfuzzer][libfuzzer-getting-started] and [FuzzTest][fuzztest-getting-started] getting started docs to get familar.

## Compiling locally
To build the fuzzers residing in the [test/fuzzers][fuzzers] directory, use
```
$ gn gen out/fuzzers --args='use_libfuzzer=true optimize_for_fuzzing=true'
```
Depending on the fuzzer additional arguments like `is_asan`, `is_msan` or `is_ubsan_security` might be required.

See the [GN][gn-doc] documentation for all available options. There are also more
platform specific tips on the [Android][webrtc-android-development] and
[iOS][webrtc-ios-development] instructions.

## Add new fuzzers
Create a new `.cc` file in the [test/fuzzers][fuzzers] directory, use existing files as a guide.

Add a new `webrtc_fuzzers_test` build rule in the [test/fuzzers/BUILD.gn][BUILD.gn], use existing rules as a guide.

Ensure it compiles and executes locally then add it to a gerrit CL and upload it for review, e.g.

```
$ autoninja -C out/fuzzers test/fuzzers:h264_depacketizer_fuzzer
```

It can then be executed like so:
```
$ out/fuzzers/bin/run_h264_depacketizer_fuzzer
```

## Running fuzzers automatically
All fuzzer tests in the [test/fuzzers/BUILD.gn][BUILD.gn] file are compiled per CL on the [libfuzzer bot][libfuzzer-bot].
This is only to verify that it compiles, this bot does not do any fuzz testing.

When WebRTC is [rolled][webrtc-autoroller] into to Chromium, the libfuzz bots in the [chromium.fuzz][chromium-fuzz] will compile it, zip it and then upload to https://clusterfuzz.com for execution.

You can verify that the fuzz test is being executed by:
 - Navigate to a bot in the [chromium.fuzz][chromium-fuzz] libfuzzer waterfall, e.g. [ Libfuzzer Upload Linux ASan bot/linux bot][linux-bot].
 - Click on the latest `build#` link.
 - Search for `//third_party/webrtc/test/fuzzers` in the `raw_io.output_text_refs_` file in the `calculate_all_fuzzers` step.
 - Verify that the new fuzzer (as it's named in the `webrtc_fuzzers_test` build rule) is present.
 - Also verify that it's _NOT_ in the `no_clusterfuzz` file in the `calculate_no_clusterfuzz` step. If it is, file a bug at https://bugs.webrtc.org.

Bugs are filed automatically in https://crbug.com in the blink > WebRTC component and assigned based on [test/fuzzers/OWNERS][OWNERS] file or the commit history.

If you are a non-googler, you can only view data from https://clusterfuzz.com if your account is CC'ed on the reported bug.

## Additional reading

[Libfuzzer in Chromium][libfuzzer-chromium]


[libfuzzer-chromium]: https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/README.md
[libfuzzer-bot]: https://ci.chromium.org/ui/p/webrtc/builders/luci.webrtc.ci/Linux64%20Release%20%28Libfuzzer%29
[fuzzers]: https://webrtc.googlesource.com/src/+/main/test/fuzzers/
[OWNERS]: https://webrtc.googlesource.com/src/+/main/test/fuzzers/OWNERS
[BUILD.gn]: https://webrtc.googlesource.com/src/+/main/test/fuzzers/BUILD.gn
[gn]: https://gn.googlesource.com/gn/+/main/README.md
[gn-doc]: https://gn.googlesource.com/gn/+/main/docs/reference.md#IDE-options
[webrtc-android-development]: https://webrtc.googlesource.com/src/+/main/docs/native-code/android/
[webrtc-ios-development]: https://webrtc.googlesource.com/src/+/main/docs/native-code/ios/
[chromium-fuzz]: https://ci.chromium.org/p/chromium/g/chromium.fuzz/console
[linux-bot]: https://ci.chromium.org/ui/p/chromium/builders/ci/Libfuzzer%20Upload%20Linux%20ASan/
[libfuzzer-getting-started]: https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/getting_started_with_libfuzzer.md
[fuzztest-getting-started]: https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/getting_started.md
[webrtc-autoroller]: https://autoroll.skia.org/r/webrtc-chromium-autoroll
