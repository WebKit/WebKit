#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import unittest
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.join(SCRIPT_DIR, os.pardir)
sys.path.append(PARENT_DIR)
import low_bandwidth_audio_test


class TestExtractTestRuns(unittest.TestCase):
  def _TestLog(self, log, *expected):
    self.assertEqual(
        tuple(low_bandwidth_audio_test.ExtractTestRuns(log.splitlines(True))),
        expected)

  def testLinux(self):
    self._TestLog(LINUX_LOG,
        (None, 'GoodNetworkHighBitrate',
         '/webrtc/src/resources/voice_engine/audio_tiny16.wav',
         '/webrtc/src/out/LowBandwidth_GoodNetworkHighBitrate.wav', None),
        (None, 'Mobile2GNetwork',
         '/webrtc/src/resources/voice_engine/audio_tiny16.wav',
         '/webrtc/src/out/LowBandwidth_Mobile2GNetwork.wav', None),
        (None, 'PCGoodNetworkHighBitrate',
         '/webrtc/src/resources/voice_engine/audio_tiny16.wav',
         '/webrtc/src/out/PCLowBandwidth_PCGoodNetworkHighBitrate.wav',
         '/webrtc/src/out/PCLowBandwidth_perf_48.json'),
        (None, 'PCMobile2GNetwork',
         '/webrtc/src/resources/voice_engine/audio_tiny16.wav',
         '/webrtc/src/out/PCLowBandwidth_PCMobile2GNetwork.wav',
         '/webrtc/src/out/PCLowBandwidth_perf_48.json'))

  def testAndroid(self):
    self._TestLog(ANDROID_LOG,
        ('ddfa6149', 'Mobile2GNetwork',
         '/sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav',
         '/sdcard/chromium_tests_root/LowBandwidth_Mobile2GNetwork.wav', None),
        ('TA99205CNO', 'GoodNetworkHighBitrate',
         '/sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav',
         '/sdcard/chromium_tests_root/LowBandwidth_GoodNetworkHighBitrate.wav',
         None),
        ('ddfa6149', 'PCMobile2GNetwork',
         '/sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav',
         '/sdcard/chromium_tests_root/PCLowBandwidth_PCMobile2GNetwork.wav',
         '/sdcard/chromium_tests_root/PCLowBandwidth_perf_48.json'),
        ('TA99205CNO', 'PCGoodNetworkHighBitrate',
         '/sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav',
         ('/sdcard/chromium_tests_root/'
         'PCLowBandwidth_PCGoodNetworkHighBitrate.wav'),
         '/sdcard/chromium_tests_root/PCLowBandwidth_perf_48.json'))


LINUX_LOG = r'''\
[==========] Running 2 tests from 1 test case.
[----------] Global test environment set-up.
[----------] 2 tests from LowBandwidthAudioTest
[ RUN      ] LowBandwidthAudioTest.GoodNetworkHighBitrate
TEST GoodNetworkHighBitrate /webrtc/src/resources/voice_engine/audio_tiny16.wav /webrtc/src/out/LowBandwidth_GoodNetworkHighBitrate.wav
[       OK ] LowBandwidthAudioTest.GoodNetworkHighBitrate (5932 ms)
[ RUN      ] LowBandwidthAudioTest.Mobile2GNetwork
TEST Mobile2GNetwork /webrtc/src/resources/voice_engine/audio_tiny16.wav /webrtc/src/out/LowBandwidth_Mobile2GNetwork.wav
[       OK ] LowBandwidthAudioTest.Mobile2GNetwork (6333 ms)
[----------] 2 tests from LowBandwidthAudioTest (12265 ms total)
[----------] 2 tests from PCLowBandwidthAudioTest
[ RUN      ] PCLowBandwidthAudioTest.PCGoodNetworkHighBitrate
TEST PCGoodNetworkHighBitrate /webrtc/src/resources/voice_engine/audio_tiny16.wav /webrtc/src/out/PCLowBandwidth_PCGoodNetworkHighBitrate.wav /webrtc/src/out/PCLowBandwidth_perf_48.json
[       OK ] PCLowBandwidthAudioTest.PCGoodNetworkHighBitrate (5932 ms)
[ RUN      ] PCLowBandwidthAudioTest.PCMobile2GNetwork
TEST PCMobile2GNetwork /webrtc/src/resources/voice_engine/audio_tiny16.wav /webrtc/src/out/PCLowBandwidth_PCMobile2GNetwork.wav /webrtc/src/out/PCLowBandwidth_perf_48.json
[       OK ] PCLowBandwidthAudioTest.PCMobile2GNetwork (6333 ms)
[----------] 2 tests from PCLowBandwidthAudioTest (12265 ms total)

[----------] Global test environment tear-down
[==========] 2 tests from 1 test case ran. (12266 ms total)
[  PASSED  ] 2 tests.
'''

ANDROID_LOG = r'''\
I    0.000s Main  command: /webrtc/src/build/android/test_runner.py gtest --suite low_bandwidth_audio_test --output-directory /webrtc/src/out/debug-android --runtime-deps-path /webrtc/src/out/debug-android/gen.runtime/webrtc/audio/low_bandwidth_audio_test__test_runner_script.runtime_deps -v
I    0.007s Main  [host]> /webrtc/src/third_party/android_sdk/public/build-tools/24.0.2/aapt dump xmltree /webrtc/src/out/debug-android/low_bandwidth_audio_test_apk/low_bandwidth_audio_test-debug.apk AndroidManifest.xml
I    0.028s TimeoutThread-1-for-MainThread  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb devices
I    0.062s TimeoutThread-1-for-prepare_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO wait-for-device
I    0.063s TimeoutThread-1-for-prepare_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 wait-for-device
I    0.102s TimeoutThread-1-for-prepare_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( ( c=/data/local/tmp/cache_token;echo $EXTERNAL_STORAGE;cat $c 2>/dev/null||echo;echo "77611072-160c-11d7-9362-705b0f464195">$c &&getprop )>/data/local/tmp/temp_file-5ea34389e3f92 );echo %$?'
I    0.105s TimeoutThread-1-for-prepare_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( ( c=/data/local/tmp/cache_token;echo $EXTERNAL_STORAGE;cat $c 2>/dev/null||echo;echo "77618afc-160c-11d7-bda4-705b0f464195">$c &&getprop )>/data/local/tmp/temp_file-b995cef6e0e3d );echo %$?'
I    0.204s TimeoutThread-1-for-prepare_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 pull /data/local/tmp/temp_file-b995cef6e0e3d /tmp/tmpieAgDj/tmp_ReadFileWithPull
I    0.285s TimeoutThread-1-for-prepare_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( test -d /storage/emulated/legacy );echo %$?'
I    0.285s TimeoutThread-1-for-delete_temporary_file(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell 'rm -f /data/local/tmp/temp_file-b995cef6e0e3d'
I    0.302s TimeoutThread-1-for-prepare_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO pull /data/local/tmp/temp_file-5ea34389e3f92 /tmp/tmpvlyG3I/tmp_ReadFileWithPull
I    0.352s TimeoutThread-1-for-prepare_device(ddfa6149)  condition 'sd_card_ready' met (0.3s)
I    0.353s TimeoutThread-1-for-prepare_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( pm path android );echo %$?'
I    0.369s TimeoutThread-1-for-prepare_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( test -d /sdcard );echo %$?'
I    0.370s TimeoutThread-1-for-delete_temporary_file(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell 'rm -f /data/local/tmp/temp_file-5ea34389e3f92'
I    0.434s TimeoutThread-1-for-prepare_device(TA99205CNO)  condition 'sd_card_ready' met (0.4s)
I    0.434s TimeoutThread-1-for-prepare_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( pm path android );echo %$?'
I    1.067s TimeoutThread-1-for-prepare_device(ddfa6149)  condition 'pm_ready' met (1.0s)
I    1.067s TimeoutThread-1-for-prepare_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( getprop sys.boot_completed );echo %$?'
I    1.115s TimeoutThread-1-for-prepare_device(ddfa6149)  condition 'boot_completed' met (1.1s)
I    1.181s TimeoutThread-1-for-prepare_device(TA99205CNO)  condition 'pm_ready' met (1.1s)
I    1.181s TimeoutThread-1-for-prepare_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( getprop sys.boot_completed );echo %$?'
I    1.242s TimeoutThread-1-for-prepare_device(TA99205CNO)  condition 'boot_completed' met (1.2s)
I    1.268s TimeoutThread-1-for-individual_device_set_up(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( pm path org.chromium.native_test );echo %$?'
I    1.269s TimeoutThread-1-for-individual_device_set_up(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( pm path org.chromium.native_test );echo %$?'
I    2.008s calculate_device_checksums  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( a=/data/local/tmp/md5sum/md5sum_bin;! [[ $(ls -l $a) = *1225256* ]]&&exit 2;export LD_LIBRARY_PATH=/data/local/tmp/md5sum;$a /data/app/org.chromium.native_test-2/base.apk;: );echo %$?'
I    2.008s calculate_host_checksums  [host]> /webrtc/src/out/debug-android/md5sum_bin_host /webrtc/src/out/debug-android/low_bandwidth_audio_test_apk/low_bandwidth_audio_test-debug.apk
I    2.019s calculate_device_checksums  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( a=/data/local/tmp/md5sum/md5sum_bin;! [[ $(ls -l $a) = *1225256* ]]&&exit 2;export LD_LIBRARY_PATH=/data/local/tmp/md5sum;$a /data/app/org.chromium.native_test-1/base.apk;: );echo %$?'
I    2.020s calculate_host_checksums  [host]> /webrtc/src/out/debug-android/md5sum_bin_host /webrtc/src/out/debug-android/low_bandwidth_audio_test_apk/low_bandwidth_audio_test-debug.apk
I    2.172s TimeoutThread-1-for-individual_device_set_up(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( p=org.chromium.native_test;if [[ "$(ps)" = *$p* ]]; then am force-stop $p; fi );echo %$?'
I    2.183s TimeoutThread-1-for-individual_device_set_up(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( p=org.chromium.native_test;if [[ "$(ps)" = *$p* ]]; then am force-stop $p; fi );echo %$?'
I    2.290s calculate_device_checksums  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( a=/data/local/tmp/md5sum/md5sum_bin;! [[ $(ls -l $a) = *1225256* ]]&&exit 2;export LD_LIBRARY_PATH=/data/local/tmp/md5sum;$a /sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav;: );echo %$?'
I    2.291s calculate_host_checksums  [host]> /webrtc/src/out/debug-android/md5sum_bin_host /webrtc/src/resources/voice_engine/audio_tiny16.wav
I    2.373s calculate_device_checksums  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( a=/data/local/tmp/md5sum/md5sum_bin;! [[ $(ls -l $a) = *1225256* ]]&&exit 2;export LD_LIBRARY_PATH=/data/local/tmp/md5sum;$a /storage/emulated/legacy/chromium_tests_root/resources/voice_engine/audio_tiny16.wav;: );echo %$?'
I    2.374s calculate_host_checksums  [host]> /webrtc/src/out/debug-android/md5sum_bin_host /webrtc/src/resources/voice_engine/audio_tiny16.wav
I    2.390s calculate_device_checksums  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( a=/data/local/tmp/md5sum/md5sum_bin;! [[ $(ls -l $a) = *1225256* ]]&&exit 2;export LD_LIBRARY_PATH=/data/local/tmp/md5sum;$a /sdcard/chromium_tests_root/icudtl.dat;: );echo %$?'
I    2.390s calculate_host_checksums  [host]> /webrtc/src/out/debug-android/md5sum_bin_host /webrtc/src/out/debug-android/icudtl.dat
I    2.472s calculate_device_checksums  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( a=/data/local/tmp/md5sum/md5sum_bin;! [[ $(ls -l $a) = *1225256* ]]&&exit 2;export LD_LIBRARY_PATH=/data/local/tmp/md5sum;$a /storage/emulated/legacy/chromium_tests_root/icudtl.dat;: );echo %$?'
I    2.472s calculate_host_checksums  [host]> /webrtc/src/out/debug-android/md5sum_bin_host /webrtc/src/out/debug-android/icudtl.dat
I    2.675s TimeoutThread-1-for-list_tests(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( ( p=org.chromium.native_test;am instrument -w -e "$p".NativeTestInstrumentationTestRunner.ShardNanoTimeout 30000000000 -e "$p".NativeTestInstrumentationTestRunner.NativeTestActivity "$p".NativeUnitTestActivity -e "$p".NativeTestInstrumentationTestRunner.StdoutFile /sdcard/temp_file-6407c967884af.gtest_out -e "$p".NativeTest.CommandLineFlags --gtest_list_tests "$p"/"$p".NativeTestInstrumentationTestRunner )>/data/local/tmp/temp_file-d21ebcd0977d9 );echo %$?'
I    2.675s TimeoutThread-1-for-list_tests(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( ( p=org.chromium.native_test;am instrument -w -e "$p".NativeTestInstrumentationTestRunner.ShardNanoTimeout 30000000000 -e "$p".NativeTestInstrumentationTestRunner.NativeTestActivity "$p".NativeUnitTestActivity -e "$p".NativeTestInstrumentationTestRunner.StdoutFile /storage/emulated/legacy/temp_file-fa09560c3259.gtest_out -e "$p".NativeTest.CommandLineFlags --gtest_list_tests "$p"/"$p".NativeTestInstrumentationTestRunner )>/data/local/tmp/temp_file-95ad995999939 );echo %$?'
I    3.739s TimeoutThread-1-for-list_tests(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 pull /data/local/tmp/temp_file-95ad995999939 /tmp/tmpSnnF6Y/tmp_ReadFileWithPull
I    3.807s TimeoutThread-1-for-delete_temporary_file(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell 'rm -f /data/local/tmp/temp_file-95ad995999939'
I    3.812s TimeoutThread-1-for-list_tests(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( TZ=utc ls -a -l /storage/emulated/legacy/ );echo %$?'
I    3.866s TimeoutThread-1-for-list_tests(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( cat /storage/emulated/legacy/temp_file-fa09560c3259.gtest_out );echo %$?'
I    3.912s TimeoutThread-1-for-delete_temporary_file(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell 'rm -f /storage/emulated/legacy/temp_file-fa09560c3259.gtest_out'
I    4.256s TimeoutThread-1-for-list_tests(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO pull /data/local/tmp/temp_file-d21ebcd0977d9 /tmp/tmpokPF5b/tmp_ReadFileWithPull
I    4.324s TimeoutThread-1-for-delete_temporary_file(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell 'rm -f /data/local/tmp/temp_file-d21ebcd0977d9'
I    4.342s TimeoutThread-1-for-list_tests(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( TZ=utc ls -a -l /sdcard/ );echo %$?'
I    4.432s TimeoutThread-1-for-list_tests(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( cat /sdcard/temp_file-6407c967884af.gtest_out );echo %$?'
I    4.476s TimeoutThread-1-for-delete_temporary_file(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell 'rm -f /sdcard/temp_file-6407c967884af.gtest_out'
I    4.483s Main  Using external sharding settings. This is shard 0/1
I    4.483s Main  STARTING TRY #1/3
I    4.484s Main  Will run 2 tests on 2 devices: TA99205CNO, ddfa6149
I    4.486s TimeoutThread-1-for-run_tests_on_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( pm dump org.chromium.native_test | grep dataDir=; echo "PIPESTATUS: ${PIPESTATUS[@]}" );echo %$?'
I    4.486s TimeoutThread-1-for-run_tests_on_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( pm dump org.chromium.native_test | grep dataDir=; echo "PIPESTATUS: ${PIPESTATUS[@]}" );echo %$?'
I    5.551s run_tests_on_device(TA99205CNO)  flags:
I    5.552s run_tests_on_device(ddfa6149)  flags:
I    5.554s TimeoutThread-1-for-run_tests_on_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( ( p=org.chromium.native_test;am instrument -w -e "$p".NativeTestInstrumentationTestRunner.ShardNanoTimeout 120000000000 -e "$p".NativeTestInstrumentationTestRunner.NativeTestActivity "$p".NativeUnitTestActivity -e "$p".NativeTestInstrumentationTestRunner.Test LowBandwidthAudioTest.GoodNetworkHighBitrate -e "$p".NativeTestInstrumentationTestRunner.StdoutFile /sdcard/temp_file-ffe7b76691cb7.gtest_out "$p"/"$p".NativeTestInstrumentationTestRunner )>/data/local/tmp/temp_file-c9d83b3078ab1 );echo %$?'
I    5.556s TimeoutThread-1-for-run_tests_on_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( ( p=org.chromium.native_test;am instrument -w -e "$p".NativeTestInstrumentationTestRunner.ShardNanoTimeout 120000000000 -e "$p".NativeTestInstrumentationTestRunner.NativeTestActivity "$p".NativeUnitTestActivity -e "$p".NativeTestInstrumentationTestRunner.Test LowBandwidthAudioTest.Mobile2GNetwork -e "$p".NativeTestInstrumentationTestRunner.StdoutFile /storage/emulated/legacy/temp_file-f0ceb1a05ea8.gtest_out "$p"/"$p".NativeTestInstrumentationTestRunner )>/data/local/tmp/temp_file-245ef307a5b32 );echo %$?'
I   12.956s TimeoutThread-1-for-run_tests_on_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO pull /data/local/tmp/temp_file-c9d83b3078ab1 /tmp/tmpRQhTcM/tmp_ReadFileWithPull
I   13.024s TimeoutThread-1-for-delete_temporary_file(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell 'rm -f /data/local/tmp/temp_file-c9d83b3078ab1'
I   13.032s TimeoutThread-1-for-run_tests_on_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( TZ=utc ls -a -l /sdcard/ );echo %$?'
I   13.114s TimeoutThread-1-for-run_tests_on_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( cat /sdcard/temp_file-ffe7b76691cb7.gtest_out );echo %$?'
I   13.154s TimeoutThread-1-for-run_tests_on_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 pull /data/local/tmp/temp_file-245ef307a5b32 /tmp/tmpfQ4J96/tmp_ReadFileWithPull
I   13.167s TimeoutThread-1-for-delete_temporary_file(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell 'rm -f /sdcard/temp_file-ffe7b76691cb7.gtest_out'
I   13.169s TimeoutThread-1-for-delete_temporary_file(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell 'rm -f /data/user/0/org.chromium.native_test/temp_file-f07c4808dbf8f.xml'
I   13.170s TimeoutThread-1-for-run_tests_on_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( pm clear org.chromium.native_test );echo %$?'
I   13.234s TimeoutThread-1-for-delete_temporary_file(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell 'rm -f /data/local/tmp/temp_file-245ef307a5b32'
I   13.239s TimeoutThread-1-for-run_tests_on_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( TZ=utc ls -a -l /storage/emulated/legacy/ );echo %$?'
I   13.291s TimeoutThread-1-for-run_tests_on_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( cat /storage/emulated/legacy/temp_file-f0ceb1a05ea8.gtest_out );echo %$?'
I   13.341s TimeoutThread-1-for-delete_temporary_file(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell 'rm -f /storage/emulated/legacy/temp_file-f0ceb1a05ea8.gtest_out'
I   13.343s TimeoutThread-1-for-delete_temporary_file(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell 'rm -f /data/data/org.chromium.native_test/temp_file-5649bb01682da.xml'
I   13.346s TimeoutThread-1-for-run_tests_on_device(ddfa6149)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s ddfa6149 shell '( pm clear org.chromium.native_test );echo %$?'
I   13.971s TimeoutThread-1-for-run_tests_on_device(TA99205CNO)  Setting permissions for org.chromium.native_test.
I   13.971s TimeoutThread-1-for-run_tests_on_device(TA99205CNO)  [host]> /webrtc/src/third_party/android_sdk/public/platform-tools/adb -s TA99205CNO shell '( pm grant org.chromium.native_test android.permission.CAMERA&&pm grant org.chromium.native_test android.permission.RECORD_AUDIO&&pm grant org.chromium.native_test android.permission.WRITE_EXTERNAL_STORAGE&&pm grant org.chromium.native_test android.permission.READ_EXTERNAL_STORAGE );echo %$?'
I   14.078s run_tests_on_device(ddfa6149)  >>ScopedMainEntryLogger
I   14.078s run_tests_on_device(ddfa6149)  Note: Google Test filter = LowBandwidthAudioTest.Mobile2GNetwork
I   14.078s run_tests_on_device(ddfa6149)  [==========] Running 1 test from 1 test case.
I   14.078s run_tests_on_device(ddfa6149)  [----------] Global test environment set-up.
I   14.078s run_tests_on_device(ddfa6149)  [----------] 1 test from LowBandwidthAudioTest
I   14.078s run_tests_on_device(ddfa6149)  [ RUN      ] LowBandwidthAudioTest.Mobile2GNetwork
I   14.078s run_tests_on_device(ddfa6149)  TEST Mobile2GNetwork /sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav /sdcard/chromium_tests_root/LowBandwidth_Mobile2GNetwork.wav
I   14.078s run_tests_on_device(ddfa6149)  [       OK ] LowBandwidthAudioTest.Mobile2GNetwork (6438 ms)
I   14.078s run_tests_on_device(ddfa6149)  [----------] 1 test from LowBandwidthAudioTest (6438 ms total)
I   14.078s run_tests_on_device(ddfa6149)
I   14.078s run_tests_on_device(ddfa6149)  [----------] Global test environment tear-down
I   14.079s run_tests_on_device(ddfa6149)  [==========] 1 test from 1 test case ran. (6438 ms total)
I   14.079s run_tests_on_device(ddfa6149)  [  PASSED  ] 1 test.
I   14.079s run_tests_on_device(ddfa6149)  <<ScopedMainEntryLogger
I   16.576s run_tests_on_device(TA99205CNO)  >>ScopedMainEntryLogger
I   16.576s run_tests_on_device(TA99205CNO)  Note: Google Test filter = LowBandwidthAudioTest.GoodNetworkHighBitrate
I   16.576s run_tests_on_device(TA99205CNO)  [==========] Running 1 test from 1 test case.
I   16.576s run_tests_on_device(TA99205CNO)  [----------] Global test environment set-up.
I   16.576s run_tests_on_device(TA99205CNO)  [----------] 1 test from LowBandwidthAudioTest
I   16.576s run_tests_on_device(TA99205CNO)  [ RUN      ] LowBandwidthAudioTest.GoodNetworkHighBitrate
I   16.576s run_tests_on_device(TA99205CNO)  TEST GoodNetworkHighBitrate /sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav /sdcard/chromium_tests_root/LowBandwidth_GoodNetworkHighBitrate.wav
I   16.576s run_tests_on_device(TA99205CNO)  [       OK ] LowBandwidthAudioTest.GoodNetworkHighBitrate (5968 ms)
I   16.576s run_tests_on_device(TA99205CNO)  [----------] 1 test from LowBandwidthAudioTest (5968 ms total)
I   16.576s run_tests_on_device(TA99205CNO)
I   16.576s run_tests_on_device(TA99205CNO)  [----------] Global test environment tear-down
I   16.576s run_tests_on_device(TA99205CNO)  [==========] 1 test from 1 test case ran. (5968 ms total)
I   16.577s run_tests_on_device(TA99205CNO)  [  PASSED  ] 1 test.
I   16.577s run_tests_on_device(TA99205CNO)  <<ScopedMainEntryLogger
I   14.078s run_tests_on_device(ddfa6149)  >>ScopedMainEntryLogger
I   14.078s run_tests_on_device(ddfa6149)  Note: Google Test filter = PCLowBandwidthAudioTest.PCMobile2GNetwork
I   14.078s run_tests_on_device(ddfa6149)  [==========] Running 1 test from 1 test case.
I   14.078s run_tests_on_device(ddfa6149)  [----------] Global test environment set-up.
I   14.078s run_tests_on_device(ddfa6149)  [----------] 1 test from PCLowBandwidthAudioTest
I   14.078s run_tests_on_device(ddfa6149)  [ RUN      ] PCLowBandwidthAudioTest.PCMobile2GNetwork
I   14.078s run_tests_on_device(ddfa6149)  TEST PCMobile2GNetwork /sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav /sdcard/chromium_tests_root/PCLowBandwidth_PCMobile2GNetwork.wav /sdcard/chromium_tests_root/PCLowBandwidth_perf_48.json
I   14.078s run_tests_on_device(ddfa6149)  [       OK ] PCLowBandwidthAudioTest.PCMobile2GNetwork (6438 ms)
I   14.078s run_tests_on_device(ddfa6149)  [----------] 1 test from PCLowBandwidthAudioTest (6438 ms total)
I   14.078s run_tests_on_device(ddfa6149)
I   14.078s run_tests_on_device(ddfa6149)  [----------] Global test environment tear-down
I   14.079s run_tests_on_device(ddfa6149)  [==========] 1 test from 1 test case ran. (6438 ms total)
I   14.079s run_tests_on_device(ddfa6149)  [  PASSED  ] 1 test.
I   14.079s run_tests_on_device(ddfa6149)  <<ScopedMainEntryLogger
I   16.576s run_tests_on_device(TA99205CNO)  >>ScopedMainEntryLogger
I   16.576s run_tests_on_device(TA99205CNO)  Note: Google Test filter = PCLowBandwidthAudioTest.PCGoodNetworkHighBitrate
I   16.576s run_tests_on_device(TA99205CNO)  [==========] Running 1 test from 1 test case.
I   16.576s run_tests_on_device(TA99205CNO)  [----------] Global test environment set-up.
I   16.576s run_tests_on_device(TA99205CNO)  [----------] 1 test from PCLowBandwidthAudioTest
I   16.576s run_tests_on_device(TA99205CNO)  [ RUN      ] PCLowBandwidthAudioTest.PCGoodNetworkHighBitrate
I   16.576s run_tests_on_device(TA99205CNO)  TEST PCGoodNetworkHighBitrate /sdcard/chromium_tests_root/resources/voice_engine/audio_tiny16.wav /sdcard/chromium_tests_root/PCLowBandwidth_PCGoodNetworkHighBitrate.wav /sdcard/chromium_tests_root/PCLowBandwidth_perf_48.json
I   16.576s run_tests_on_device(TA99205CNO)  [       OK ] PCLowBandwidthAudioTest.PCGoodNetworkHighBitrate (5968 ms)
I   16.576s run_tests_on_device(TA99205CNO)  [----------] 1 test from PCLowBandwidthAudioTest (5968 ms total)
I   16.576s run_tests_on_device(TA99205CNO)
I   16.576s run_tests_on_device(TA99205CNO)  [----------] Global test environment tear-down
I   16.576s run_tests_on_device(TA99205CNO)  [==========] 1 test from 1 test case ran. (5968 ms total)
I   16.577s run_tests_on_device(TA99205CNO)  [  PASSED  ] 1 test.
I   16.577s run_tests_on_device(TA99205CNO)  <<ScopedMainEntryLogger
I   16.577s run_tests_on_device(TA99205CNO)  Finished running tests on this device.
I   16.577s run_tests_on_device(ddfa6149)  Finished running tests on this device.
I   16.604s Main  FINISHED TRY #1/3
I   16.604s Main  All tests completed.
C   16.604s Main  ********************************************************************************
C   16.604s Main  Summary
C   16.604s Main  ********************************************************************************
C   16.605s Main  [==========] 2 tests ran.
C   16.605s Main  [  PASSED  ] 2 tests.
C   16.605s Main  ********************************************************************************
I   16.608s tear_down_device(ddfa6149)  Wrote device cache: /webrtc/src/out/debug-android/device_cache_ddea6549.json
I   16.608s tear_down_device(TA99205CNO)  Wrote device cache: /webrtc/src/out/debug-android/device_cache_TA99305CMO.json
'''


if __name__ == "__main__":
  unittest.main()
