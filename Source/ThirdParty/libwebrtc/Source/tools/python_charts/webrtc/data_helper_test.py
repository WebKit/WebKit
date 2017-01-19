#!/usr/bin/env python
#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import unittest
import webrtc.data_helper

class Test(unittest.TestCase):

  def setUp(self):
    # Simulate frame data from two different test runs, with 2 frames each.
    self.frame_data_0 = [{'frame_number': 0, 'ssim': 0.5, 'psnr': 30.5},
                         {'frame_number': 1, 'ssim': 0.55, 'psnr': 30.55}]
    self.frame_data_1 = [{'frame_number': 0, 'ssim': 0.6, 'psnr': 30.6},
                         {'frame_number': 0, 'ssim': 0.66, 'psnr': 30.66}]
    self.all_data = [self.frame_data_0, self.frame_data_1]

    # Test with frame_number column in a non-first position sice we need to
    # support reordering that to be able to use the gviz_api as we want.
    self.type_description = {
                             'ssim': ('number', 'SSIM'),
                             'frame_number': ('number', 'Frame number'),
                             'psnr': ('number', 'PSRN'),
    }
    self.names = ["Test 0", "Test 1"]
    self.configurations = [
     [{'name': 'name', 'value': 'Test 0'},
      {'name': 'test_number', 'value': '13'},
      {'name': 'input_filename', 'value': 'foreman_cif.yuv'},
     ],
     [{'name': 'name', 'value': 'Test 1'},
      {'name': 'test_number', 'value': '5'},
      {'name': 'input_filename', 'value': 'foreman_cif.yuv'},
     ],
    ]

  def testCreateData(self):
    messages = []
    helper = webrtc.data_helper.DataHelper(self.all_data, self.type_description,
                                           self.names, messages)
    description, data_table = helper.CreateData('ssim')
    self.assertEqual(3, len(description))
    self.assertTrue('frame_number' in description)
    self.assertTrue('ssim_0' in description)
    self.assertTrue('number' in description['ssim_0'][0])
    self.assertTrue('Test 0' in description['ssim_0'][1])
    self.assertTrue('ssim_1' in description)
    self.assertTrue('number' in description['ssim_1'][0])
    self.assertTrue('Test 1' in description['ssim_1'][1])

    self.assertEqual(0, len(messages))

    self.assertEquals(2, len(data_table))
    row = data_table[0]
    self.assertEquals(0, row['frame_number'])
    self.assertEquals(0.5, row['ssim_0'])
    self.assertEquals(0.6, row['ssim_1'])
    row = data_table[1]
    self.assertEquals(1, row['frame_number'])
    self.assertEquals(0.55, row['ssim_0'])
    self.assertEquals(0.66, row['ssim_1'])

    description, data_table = helper.CreateData('psnr')
    self.assertEqual(3, len(description))
    self.assertTrue('frame_number' in description)
    self.assertTrue('psnr_0' in description)
    self.assertTrue('psnr_1' in description)
    self.assertEqual(0, len(messages))

    self.assertEquals(2, len(data_table))
    row = data_table[0]
    self.assertEquals(0, row['frame_number'])
    self.assertEquals(30.5, row['psnr_0'])
    self.assertEquals(30.6, row['psnr_1'])
    row = data_table[1]
    self.assertEquals(1, row['frame_number'])
    self.assertEquals(30.55, row['psnr_0'])
    self.assertEquals(30.66, row['psnr_1'])

  def testGetOrdering(self):
    """ Tests that the ordering help method returns a list with frame_number
       first and the rest sorted alphabetically """
    messages = []
    helper = webrtc.data_helper.DataHelper(self.all_data, self.type_description,
                                           self.names, messages)
    description, _ = helper.CreateData('ssim')
    columns = helper.GetOrdering(description)
    self.assertEqual(3, len(columns))
    self.assertEqual(0, len(messages))
    self.assertEqual('frame_number', columns[0])
    self.assertEqual('ssim_0', columns[1])
    self.assertEqual('ssim_1', columns[2])

  def testCreateConfigurationTable(self):
    messages = []
    helper = webrtc.data_helper.DataHelper(self.all_data, self.type_description,
                                           self.names, messages)
    description, data = helper.CreateConfigurationTable(self.configurations)
    self.assertEqual(3, len(description))  # 3 columns
    self.assertEqual(2, len(data))  # 2 data sets
    self.assertTrue(description.has_key('name'))
    self.assertTrue(description.has_key('test_number'))
    self.assertTrue(description.has_key('input_filename'))
    self.assertEquals('Test 0', data[0]['name'])
    self.assertEquals('Test 1', data[1]['name'])

if __name__ == "__main__":
  unittest.main()
