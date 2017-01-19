# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'primitives',
      'type': 'static_library',
      'sources': [
        'desktop_capture_types.h',
        'desktop_frame.cc',
        'desktop_frame.h',
        'desktop_geometry.cc',
        'desktop_geometry.h',
        'desktop_region.cc',
        'desktop_region.h',
      ],
    },
    {
      'target_name': 'desktop_capture',
      'type': 'static_library',
      'dependencies': [
        ':primitives',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/base/base.gyp:rtc_base',
      ],
      'sources': [
        'cropped_desktop_frame.cc',
        'cropped_desktop_frame.h',
        'cropping_window_capturer.cc',
        'cropping_window_capturer.h',
        'cropping_window_capturer_win.cc',
        'desktop_and_cursor_composer.cc',
        'desktop_and_cursor_composer.h',
        'desktop_capture_options.h',
        'desktop_capture_options.cc',
        'desktop_capturer.cc',
        'desktop_capturer.h',
        'desktop_frame_win.cc',
        'desktop_frame_win.h',
        'mac/desktop_configuration.h',
        'mac/desktop_configuration.mm',
        'mac/desktop_configuration_monitor.h',
        'mac/desktop_configuration_monitor.cc',
        'mac/full_screen_chrome_window_detector.cc',
        'mac/full_screen_chrome_window_detector.h',
        'mac/scoped_pixel_buffer_object.cc',
        'mac/scoped_pixel_buffer_object.h',
        'mac/window_list_utils.cc',
        'mac/window_list_utils.h',
        'mouse_cursor.cc',
        'mouse_cursor.h',
        'mouse_cursor_monitor.h',
        'mouse_cursor_monitor_mac.mm',
        'mouse_cursor_monitor_win.cc',
        'screen_capture_frame_queue.h',
        'screen_capturer.h',
        'screen_capturer_helper.cc',
        'screen_capturer_helper.h',
        'screen_capturer_mac.mm',
        'screen_capturer_win.cc',
        'shared_desktop_frame.cc',
        'shared_desktop_frame.h',
        'shared_memory.cc',
        'shared_memory.h',
        'win/cursor.cc',
        'win/cursor.h',
        'win/d3d_device.cc',
        'win/d3d_device.h',
        'win/desktop.cc',
        'win/desktop.h',
        'win/dxgi_adapter_duplicator.cc',
        'win/dxgi_adapter_duplicator.h',
        'win/dxgi_duplicator_controller.cc',
        'win/dxgi_duplicator_controller.h',
        'win/dxgi_output_duplicator.cc',
        'win/dxgi_output_duplicator.h',
        'win/dxgi_texture.cc',
        'win/dxgi_texture.h',
        'win/dxgi_texture_mapping.cc',
        'win/dxgi_texture_mapping.h',
        'win/dxgi_texture_staging.cc',
        'win/dxgi_texture_staging.h',
        'win/scoped_gdi_object.h',
        'win/scoped_thread_desktop.cc',
        'win/scoped_thread_desktop.h',
        'win/screen_capture_utils.cc',
        'win/screen_capture_utils.h',
        'win/screen_capturer_win_directx.cc',
        'win/screen_capturer_win_directx.h',
        'win/screen_capturer_win_gdi.cc',
        'win/screen_capturer_win_gdi.h',
        'win/screen_capturer_win_magnifier.cc',
        'win/screen_capturer_win_magnifier.h',
        'win/window_capture_utils.cc',
        'win/window_capture_utils.h',
        'window_capturer.h',
        'window_capturer_mac.mm',
        'window_capturer_win.cc',
      ],
      'conditions': [
        ['OS!="ios" and (target_arch=="ia32" or target_arch=="x64")', {
          'dependencies': [
            'desktop_capture_differ_sse2',
          ],
        }],
        ['use_x11==1', {
          'sources': [
            'mouse_cursor_monitor_x11.cc',
            'screen_capturer_x11.cc',
            'window_capturer_x11.cc',
            'x11/shared_x_display.h',
            'x11/shared_x_display.cc',
            'x11/x_error_trap.cc',
            'x11/x_error_trap.h',
            'x11/x_server_pixel_buffer.cc',
            'x11/x_server_pixel_buffer.h',
          ],
          'link_settings': {
            'libraries': [
              '-lX11',
              '-lXcomposite',
              '-lXdamage',
              '-lXext',
              '-lXfixes',
              '-lXrender',
            ],
          },
        }],
        ['OS!="win" and OS!="mac" and use_x11==0', {
          'sources': [
            'mouse_cursor_monitor_null.cc',
            'screen_capturer_null.cc',
            'window_capturer_null.cc',
          ],
        }],
        ['OS!="ios" ', {
          'sources': [
            'differ_block.cc',
            'differ_block.h',
            'screen_capturer_differ_wrapper.cc',
            'screen_capturer_differ_wrapper.h',
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
      ],
      'all_dependent_settings': {
        'conditions': [
          ['OS=="win"', {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'd3d11.lib',
                  'dxgi.lib',
                ],
              },
            },
          }],
        ],
      },
    },
  ],  # targets
  'conditions': [
    ['OS!="ios" and (target_arch=="ia32" or target_arch=="x64")', {
      'targets': [
        {
          # Have to be compiled as a separate target because it needs to be
          # compiled with SSE2 enabled.
          'target_name': 'desktop_capture_differ_sse2',
          'type': 'static_library',
          'sources': [
            'differ_vector_sse2.cc',
            'differ_vector_sse2.h',
          ],
          'conditions': [
            ['os_posix==1', {
              'cflags': [ '-msse2', ],
              'xcode_settings': {
                'OTHER_CFLAGS': [ '-msse2', ],
              },
            }],
          ],
        },
      ],  # targets
    }],
  ],
}
