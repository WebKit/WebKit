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
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "sdk/android/native_api/jni/application_context_provider.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace test {
static const uint32_t kTestIpv4Address = 0xC0A80011;  // 192.168.0.17
// The following two ipv6 addresses only diff by the last 64 bits.
static const char kTestIpv6Address1[] = "2a00:8a00:a000:1190:0000:0001:000:252";
static const char kTestIpv6Address2[] = "2a00:8a00:a000:1190:0000:0002:000:253";

static const char kTestIfName1[] = "testlan0";
static const char kTestIfName1V4[] = "v4-testlan0";
static const char kTestIfName2[] = "testnet0";

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
    ScopedJavaLocalRef<jobject> context = GetAppContext(env);
    network_monitor_ = std::make_unique<jni::AndroidNetworkMonitor>(
        env, context, field_trials_);
  }

  void SetUp() override {
    // Reset network monitor states.
    network_monitor_->Stop();
  }

  void TearDown() override {
    // The network monitor must be stopped, before it is destructed.
    network_monitor_->Stop();
  }

  void Disconnect(jni::NetworkHandle handle) {
    network_monitor_->OnNetworkDisconnected_n(handle);
  }

 protected:
  test::ScopedKeyValueConfig field_trials_;
  rtc::AutoThread main_thread_;
  std::unique_ptr<jni::AndroidNetworkMonitor> network_monitor_;
};

TEST_F(AndroidNetworkMonitorTest, TestFindNetworkHandleUsingIpv4Address) {
  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info =
      CreateNetworkInformation(kTestIfName1, ipv4_handle, ipv4_address);
  std::vector<jni::NetworkInformation> net_infos(1, net_info);
  network_monitor_->SetNetworkInfos(net_infos);

  auto network_handle =
      network_monitor_->FindNetworkHandleFromAddressOrName(ipv4_address, "");

  ASSERT_TRUE(network_handle.has_value());
  EXPECT_EQ(ipv4_handle, *network_handle);
}

TEST_F(AndroidNetworkMonitorTest, TestFindNetworkHandleUsingFullIpv6Address) {
  jni::NetworkHandle ipv6_handle = 200;
  rtc::IPAddress ipv6_address1 = GetIpAddressFromIpv6String(kTestIpv6Address1);
  rtc::IPAddress ipv6_address2 = GetIpAddressFromIpv6String(kTestIpv6Address2);
  // Set up an IPv6 network.
  jni::NetworkInformation net_info =
      CreateNetworkInformation(kTestIfName1, ipv6_handle, ipv6_address1);
  std::vector<jni::NetworkInformation> net_infos(1, net_info);
  network_monitor_->OnNetworkConnected_n(net_info);

  auto network_handle1 =
      network_monitor_->FindNetworkHandleFromAddressOrName(ipv6_address1, "");
  auto network_handle2 =
      network_monitor_->FindNetworkHandleFromAddressOrName(ipv6_address2, "");

  ASSERT_TRUE(network_handle1.has_value());
  EXPECT_EQ(ipv6_handle, *network_handle1);
  EXPECT_TRUE(!network_handle2);
}

TEST_F(AndroidNetworkMonitorTest,
       TestFindNetworkHandleIgnoringIpv6TemporaryPart) {
  ScopedKeyValueConfig field_trials(
      field_trials_,
      "WebRTC-FindNetworkHandleWithoutIpv6TemporaryPart/Enabled/");
  // Start() updates the states introduced by the field trial.
  network_monitor_->Start();
  jni::NetworkHandle ipv6_handle = 200;
  rtc::IPAddress ipv6_address1 = GetIpAddressFromIpv6String(kTestIpv6Address1);
  rtc::IPAddress ipv6_address2 = GetIpAddressFromIpv6String(kTestIpv6Address2);
  // Set up an IPv6 network.
  jni::NetworkInformation net_info =
      CreateNetworkInformation(kTestIfName1, ipv6_handle, ipv6_address1);
  std::vector<jni::NetworkInformation> net_infos(1, net_info);
  network_monitor_->OnNetworkConnected_n(net_info);

  auto network_handle1 =
      network_monitor_->FindNetworkHandleFromAddressOrName(ipv6_address1, "");
  auto network_handle2 =
      network_monitor_->FindNetworkHandleFromAddressOrName(ipv6_address2, "");

  ASSERT_TRUE(network_handle1.has_value());
  EXPECT_EQ(ipv6_handle, *network_handle1);
  ASSERT_TRUE(network_handle2.has_value());
  EXPECT_EQ(ipv6_handle, *network_handle2);
}

TEST_F(AndroidNetworkMonitorTest, TestFindNetworkHandleUsingIfName) {
  // Start() updates the states introduced by the field trial.
  network_monitor_->Start();
  jni::NetworkHandle ipv6_handle = 200;
  rtc::IPAddress ipv6_address1 = GetIpAddressFromIpv6String(kTestIpv6Address1);

  // Set up an IPv6 network.
  jni::NetworkInformation net_info =
      CreateNetworkInformation(kTestIfName1, ipv6_handle, ipv6_address1);
  std::vector<jni::NetworkInformation> net_infos(1, net_info);
  network_monitor_->OnNetworkConnected_n(net_info);

  rtc::IPAddress ipv4_address(kTestIpv4Address);

  // Search using ip address only...
  auto network_handle1 =
      network_monitor_->FindNetworkHandleFromAddressOrName(ipv4_address, "");

  // Search using ip address AND if_name (for typical ipv4 over ipv6 tunnel).
  auto network_handle2 = network_monitor_->FindNetworkHandleFromAddressOrName(
      ipv4_address, kTestIfName1V4);

  ASSERT_FALSE(network_handle1.has_value());
  ASSERT_TRUE(network_handle2.has_value());
  EXPECT_EQ(ipv6_handle, *network_handle2);
}

TEST_F(AndroidNetworkMonitorTest, TestUnderlyingVpnType) {
  ScopedKeyValueConfig field_trials(field_trials_,
                                    "WebRTC-BindUsingInterfaceName/Enabled/");
  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info =
      CreateNetworkInformation(kTestIfName1, ipv4_handle, ipv4_address);
  net_info.type = jni::NETWORK_VPN;
  net_info.underlying_type_for_vpn = jni::NETWORK_WIFI;
  network_monitor_->OnNetworkConnected_n(net_info);

  EXPECT_EQ(rtc::ADAPTER_TYPE_WIFI,
            network_monitor_->GetInterfaceInfo(kTestIfName1V4)
                .underlying_type_for_vpn);
}

// Verify that Disconnect makes interface unavailable.
TEST_F(AndroidNetworkMonitorTest, Disconnect) {
  network_monitor_->Start();

  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info =
      CreateNetworkInformation(kTestIfName1, ipv4_handle, ipv4_address);
  net_info.type = jni::NETWORK_WIFI;
  network_monitor_->OnNetworkConnected_n(net_info);

  EXPECT_TRUE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_TRUE(
      network_monitor_
          ->FindNetworkHandleFromAddressOrName(ipv4_address, kTestIfName1V4)
          .has_value());
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1V4).adapter_type,
            rtc::ADAPTER_TYPE_WIFI);

  // Check that values are reset on disconnect().
  Disconnect(ipv4_handle);
  EXPECT_FALSE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_FALSE(
      network_monitor_
          ->FindNetworkHandleFromAddressOrName(ipv4_address, kTestIfName1V4)
          .has_value());
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1V4).adapter_type,
            rtc::ADAPTER_TYPE_UNKNOWN);
}

// Verify that Stop() resets all caches.
TEST_F(AndroidNetworkMonitorTest, Reset) {
  network_monitor_->Start();

  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info =
      CreateNetworkInformation(kTestIfName1, ipv4_handle, ipv4_address);
  net_info.type = jni::NETWORK_WIFI;
  network_monitor_->OnNetworkConnected_n(net_info);

  EXPECT_TRUE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_TRUE(
      network_monitor_
          ->FindNetworkHandleFromAddressOrName(ipv4_address, kTestIfName1V4)
          .has_value());
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1V4).adapter_type,
            rtc::ADAPTER_TYPE_WIFI);

  // Check that values are reset on Stop().
  network_monitor_->Stop();
  EXPECT_FALSE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_FALSE(
      network_monitor_
          ->FindNetworkHandleFromAddressOrName(ipv4_address, kTestIfName1V4)
          .has_value());
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1V4).adapter_type,
            rtc::ADAPTER_TYPE_UNKNOWN);
}

TEST_F(AndroidNetworkMonitorTest, DuplicateIfname) {
  network_monitor_->Start();

  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info1 =
      CreateNetworkInformation(kTestIfName1, ipv4_handle, ipv4_address);
  net_info1.type = jni::NETWORK_WIFI;

  jni::NetworkHandle ipv6_handle = 101;
  rtc::IPAddress ipv6_address = GetIpAddressFromIpv6String(kTestIpv6Address1);
  jni::NetworkInformation net_info2 =
      CreateNetworkInformation(kTestIfName1, ipv6_handle, ipv6_address);
  net_info2.type = jni::NETWORK_UNKNOWN_CELLULAR;

  network_monitor_->OnNetworkConnected_n(net_info1);
  network_monitor_->OnNetworkConnected_n(net_info2);

  // The last added.
  EXPECT_TRUE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1V4).adapter_type,
            rtc::ADAPTER_TYPE_CELLULAR);

  // But both IP addresses are still searchable.
  EXPECT_EQ(
      *network_monitor_->FindNetworkHandleFromAddressOrName(ipv4_address, ""),
      ipv4_handle);
  EXPECT_EQ(
      *network_monitor_->FindNetworkHandleFromAddressOrName(ipv6_address, ""),
      ipv6_handle);
}

TEST_F(AndroidNetworkMonitorTest, DuplicateIfnameDisconnectOwner) {
  network_monitor_->Start();

  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info1 =
      CreateNetworkInformation(kTestIfName1, ipv4_handle, ipv4_address);
  net_info1.type = jni::NETWORK_WIFI;

  jni::NetworkHandle ipv6_handle = 101;
  rtc::IPAddress ipv6_address = GetIpAddressFromIpv6String(kTestIpv6Address1);
  jni::NetworkInformation net_info2 =
      CreateNetworkInformation(kTestIfName1, ipv6_handle, ipv6_address);
  net_info2.type = jni::NETWORK_UNKNOWN_CELLULAR;

  network_monitor_->OnNetworkConnected_n(net_info1);
  network_monitor_->OnNetworkConnected_n(net_info2);

  // The last added.
  EXPECT_TRUE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1V4).adapter_type,
            rtc::ADAPTER_TYPE_CELLULAR);

  Disconnect(ipv6_handle);

  // We should now find ipv4_handle.
  EXPECT_TRUE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1V4).adapter_type,
            rtc::ADAPTER_TYPE_WIFI);
}

TEST_F(AndroidNetworkMonitorTest, DuplicateIfnameDisconnectNonOwner) {
  network_monitor_->Start();

  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info1 =
      CreateNetworkInformation(kTestIfName1, ipv4_handle, ipv4_address);
  net_info1.type = jni::NETWORK_WIFI;

  jni::NetworkHandle ipv6_handle = 101;
  rtc::IPAddress ipv6_address = GetIpAddressFromIpv6String(kTestIpv6Address1);
  jni::NetworkInformation net_info2 =
      CreateNetworkInformation(kTestIfName1, ipv6_handle, ipv6_address);
  net_info2.type = jni::NETWORK_UNKNOWN_CELLULAR;

  network_monitor_->OnNetworkConnected_n(net_info1);
  network_monitor_->OnNetworkConnected_n(net_info2);

  // The last added.
  EXPECT_TRUE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1).adapter_type,
            rtc::ADAPTER_TYPE_CELLULAR);

  Disconnect(ipv4_handle);

  // We should still find ipv6 network.
  EXPECT_TRUE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1V4).adapter_type,
            rtc::ADAPTER_TYPE_CELLULAR);
}

TEST_F(AndroidNetworkMonitorTest, ReconnectWithoutDisconnect) {
  network_monitor_->Start();

  jni::NetworkHandle ipv4_handle = 100;
  rtc::IPAddress ipv4_address(kTestIpv4Address);
  jni::NetworkInformation net_info1 =
      CreateNetworkInformation(kTestIfName1, ipv4_handle, ipv4_address);
  net_info1.type = jni::NETWORK_WIFI;

  rtc::IPAddress ipv6_address = GetIpAddressFromIpv6String(kTestIpv6Address1);
  jni::NetworkInformation net_info2 =
      CreateNetworkInformation(kTestIfName2, ipv4_handle, ipv6_address);
  net_info2.type = jni::NETWORK_UNKNOWN_CELLULAR;

  network_monitor_->OnNetworkConnected_n(net_info1);
  network_monitor_->OnNetworkConnected_n(net_info2);

  // Only last one should still be there!
  EXPECT_TRUE(network_monitor_->GetInterfaceInfo(kTestIfName2).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName2).adapter_type,
            rtc::ADAPTER_TYPE_CELLULAR);

  EXPECT_FALSE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1).adapter_type,
            rtc::ADAPTER_TYPE_UNKNOWN);

  Disconnect(ipv4_handle);

  // Should be empty!
  EXPECT_FALSE(network_monitor_->GetInterfaceInfo(kTestIfName2).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName2).adapter_type,
            rtc::ADAPTER_TYPE_UNKNOWN);
  EXPECT_FALSE(network_monitor_->GetInterfaceInfo(kTestIfName1).available);
  EXPECT_EQ(network_monitor_->GetInterfaceInfo(kTestIfName1).adapter_type,
            rtc::ADAPTER_TYPE_UNKNOWN);
}

}  // namespace test
}  // namespace webrtc
