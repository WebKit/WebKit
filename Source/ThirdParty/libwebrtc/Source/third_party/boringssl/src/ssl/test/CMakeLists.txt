# Copyright 2014 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_executable(
  bssl_shim

  async_bio.cc
  bssl_shim.cc
  handshake_util.cc
  mock_quic_transport.cc
  packeted_bio.cc
  settings_writer.cc
  test_config.cc
  test_state.cc
)
target_link_libraries(bssl_shim ssl crypto)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(
    handshaker

    async_bio.cc
    handshake_util.cc
    handshaker.cc
    mock_quic_transport.cc
    packeted_bio.cc
    settings_writer.cc
    test_config.cc
    test_state.cc
  )
  target_link_libraries(handshaker ssl crypto)
else()
  # Declare a dummy target for run_tests to depend on.
  add_custom_target(handshaker)
endif()
