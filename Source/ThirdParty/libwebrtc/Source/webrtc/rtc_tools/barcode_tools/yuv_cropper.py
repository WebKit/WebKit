#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import optparse
import os
import sys


def _CropOneFrame(yuv_file, output_file, component_sizes):
  """Crops one frame.

  This function crops one frame going through all the YUV planes and cropping
  respective amount of rows.

  Args:
    yuv_file(file): The opened (for binary reading) YUV file.
    output_file(file): The opened (for binary writing) file.
    component_sizes(list of 3 3-ples): The list contains the sizes for all the
      planes (Y, U, V) of the YUV file plus the crop_height scaled for every
      plane. The sizes equal width, height and crop_height for the Y plane,
      and are equal to width/2, height/2 and crop_height/2 for the U and V
      planes.
  Return:
    (bool): True if there are more frames to crop, False otherwise.
  """
  for comp_width, comp_height, comp_crop_height in component_sizes:
    for row in range(comp_height):
      # Read the plane data for this row.
      yuv_plane = yuv_file.read(comp_width)

      # If the plane is empty, we have reached the end of the file.
      if yuv_plane == "":
        return False

      # Only write the plane data for the rows bigger than crop_height.
      if row >= comp_crop_height:
        output_file.write(yuv_plane)
  return True


def CropFrames(yuv_file_name, output_file_name, width, height, crop_height):
  """Crops rows of pixels from the top of the YUV frames.

  This function goes through all the frames in a video and crops the crop_height
  top pixel rows of every frame.

  Args:
    yuv_file_name(string): The name of the YUV file to be cropped.
    output_file_name(string): The name of the output file where the result will
      be written.
    width(int): The width of the original YUV file.
    height(int): The height of the original YUV file.
    crop_height(int): The height (the number of pixel rows) to be cropped from
      the frames.
  """
  # Component sizes = [Y_sizes, U_sizes, V_sizes].
  component_sizes = [(width, height, crop_height),
                     (width/2, height/2, crop_height/2),
                     (width/2, height/2, crop_height/2)]

  yuv_file = open(yuv_file_name, 'rb')
  output_file = open(output_file_name, 'wb')

  data_left = True
  while data_left:
    data_left = _CropOneFrame(yuv_file, output_file, component_sizes)

  yuv_file.close()
  output_file.close()


def _ParseArgs():
  """Registers the command-line options."""
  usage = "usage: %prog [options]"
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('--width', type='int',
                    default=352,
                    help=('Width of the YUV file\'s frames. '
                          'Default: %default'))
  parser.add_option('--height', type='int', default=288,
                    help=('Height of the YUV file\'s frames. '
                          'Default: %default'))
  parser.add_option('--crop_height', type='int', default=32,
                    help=('How much of the top of the YUV file to crop. '
                          'Has to be module of 2. Default: %default'))
  parser.add_option('--yuv_file', type='string',
                    help=('The YUV file to be cropped.'))
  parser.add_option('--output_file', type='string', default='output.yuv',
                    help=('The output YUV file containing the cropped YUV. '
                          'Default: %default'))
  options = parser.parse_args()[0]
  if not options.yuv_file:
    parser.error('yuv_file argument missing. Please specify input YUV file!')
  return options


def main():
  """A tool to crop rows of pixels from the top part of a YUV file.

  A simple invocation will be:
  ./yuv_cropper.py --width=640 --height=480 --crop_height=32
  --yuv_file=<path_and_name_of_yuv_file>
  --output_yuv=<path and name_of_output_file>
  """
  options = _ParseArgs()

  if os.path.getsize(options.yuv_file) == 0:
    sys.stderr.write('Error: The YUV file you have passed has size 0. The '
                     'produced output will also have size 0.\n')
    return -1

  CropFrames(options.yuv_file, options.output_file, options.width,
             options.height, options.crop_height)
  return 0


if __name__ == '__main__':
  sys.exit(main())
