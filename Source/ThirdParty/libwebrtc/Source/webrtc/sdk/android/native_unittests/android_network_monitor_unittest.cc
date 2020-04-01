/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/android_network_monitor.h"

#include "rtc_base/ip_address.h"
#include "sdk/android/native_unittests/application_context_provider.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
static const uint32_t kTestIpv4Address = 0xC0A80011;  // 192.168.0.17
// The following two ipv6 addresses only diff by the last 64 bits.
static const char kTestIpv6Address1[] = "2a00:8a00:a000:1190:0000:0001:000:252";
static const char kTestIpv6Address2[] = "2a00:8a00:a000:1190:0000:0002:000:253";

jni::NetworkInformation CreateNetworkInformation(
    const std::string& interface_name,
    jni::NetworkHandle network_handle,
    const rtc::IPAddress& ip_address) {
  jni::NetworkInformation net_info;
  net_info.interface_name = interface_name;
  net_info.handle = network_handle;
  net_info.type = jni::NETWORK_WIFI;
  net_info.ip_addresses.push_back(ip_address);
  return net_info;
}

rtc::IPAddress GetIpAddressFromIpv6String(const std::string& str) {
  rtc::IPAddress ipv6;
  RTC_CHECK(rtc::IPFromString(str, &ipv6));
  return ipv6;
}

class AndroidNetworkMonitorTest : public ::testing::Test {
 public:
  AndroidNetworkMonitorTest() {
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    ScopedJavaLocalRef<jobject> context = test::GetAppContextForTest(env);
    network_monitor_ =
        std::make_unique<jni::AndroidNetworkMonitor>(env, context);
  }

  void SetUp() {
    // Reset network monitor states.
    network_monitor_->Stop();
  }

 protected:
  std::unique_ptr<jni::AndroidNetworkMonitor> network_monitor_;
};

TEST_F(AndroidNetworkMonitorTest, TestFindNetworkHandleUsingIpv4Address) {
  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info =
      CreateNetworkInformation("wlan0", ipv4_handle, ipv4_address);
  std::vector<jni::NetworkInformation> net_infos(1, net_info);
  network_monitor_->SetNetworkInfos(net_infos);

  auto network_handle =
      network_monitor_->FindNetworkHandleFromAddress(ipv4_address);

  ASSERT_TRUE(network_handle.has_value());
  EXPECT_EQ(ipv4_handle, *network_handle);
}

TEST_F(AndroidNetworkMonitorTest, TestFindNetworkHandleUsingFullIpv6Address) {
  jni::NetworkHandle ipv6_handle = 200;
  rtc::IPAddress ipv6_address1 = GetIpAddressFromIpv6String(kTestIpv6Address1);
  rtc::IPAddress ipv6_address2 = GetIpAddressFromIpv6String(kTestIpv6Address2);
  // Set up an IPv6 network.
  jni::NetworkInformation net_info =
      CreateNetworkInformation("wlan0", ipv6_handle, ipv6_address1);
  std::vector<jni::NetworkInformation> net_infos(1, net_info);
  network_monitor_->SetNetworkInfos(net_infos);

  auto network_handle1 =
      network_monitor_->FindNetworkHandleFromAddress(ipv6_address1);
  auto network_handle2 =
      network_monitor_->FindNetworkHandleFromAddress(ipv6_address2);

  ASSERT_TRUE(network_handle1.has_value());
  EXPECT_EQ(ipv6_handle, *network_handle1);
  EXPECT_TRUE(!network_handle2);
}

TEST_F(AndroidNetworkMonitorTest,
       TestFindNetworkHandleIgnoringIpv6TemporaryPart) {
  ScopedFieldTrials field_trials(
      "WebRTC-FindNetworkHandleWithoutIpv6TemporaryPart/Enabled/");
  // Start() updates the states introduced by the field trial.
  network_monitor_->Start();
  jni::NetworkHandle ipv6_handle = 200;
  rtc::IPAddress ipv6_address1 = GetIpAddressFromIpv6String(kTestIpv6Address1);
  rtc::IPAddress ipv6_address2 = GetIpAddressFromIpv6String(kTestIpv6Address2);
  // Set up an IPv6 network.
  jni::NetworkInformation net_info =
      CreateNetworkInformation("wlan0", ipv6_handle, ipv6_address1);
  std::vector<jni::NetworkInformation> net_infos(1, net_info);
  network_monitor_->SetNetworkInfos(net_infos);

  auto network_handle1 =
      network_monitor_->FindNetworkHandleFromAddress(ipv6_address1);
  auto network_handle2 =
      network_monitor_->FindNetworkHandleFromAddress(ipv6_address2);

  ASSERT_TRUE(network_handle1.has_value());
  EXPECT_EQ(ipv6_handle, *network_handle1);
  ASSERT_TRUE(network_handle2.has_value());
  EXPECT_EQ(ipv6_handle, *network_handle2);
}

}  // namespace test
}  // namespace webrtc
