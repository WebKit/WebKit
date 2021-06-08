#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import network_tester_config_pb2


def AddConfig(all_configs,
              packet_send_interval_ms,
              packet_size,
              execution_time_ms):
  config = all_configs.configs.add()
  config.packet_send_interval_ms = packet_send_interval_ms
  config.packet_size = packet_size
  config.execution_time_ms = execution_time_ms

def main():
  all_configs = network_tester_config_pb2.NetworkTesterAllConfigs()
  AddConfig(all_configs, 10, 50, 200)
  AddConfig(all_configs, 10, 100, 200)
  with open("network_tester_config.dat", 'wb') as f:
    f.write(all_configs.SerializeToString())

if __name__ == "__main__":
  main()
