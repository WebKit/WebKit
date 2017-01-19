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

import helper_functions

_DEFAULT_BARCODE_WIDTH = 352
_DEFAULT_BARCODES_FILE = 'barcodes.yuv'


def generate_upca_barcodes(number_of_barcodes, barcode_width, barcode_height,
                           output_directory='.',
                           path_to_zxing='zxing-read-only'):
  """Generates UPC-A barcodes.

  This function generates a number_of_barcodes UPC-A barcodes. The function
  calls an example Java encoder from the Zxing library. The barcodes are
  generated as PNG images. The width of the barcodes shouldn't be less than 102
  pixels because otherwise Zxing can't properly generate the barcodes.

  Args:
    number_of_barcodes(int): The number of barcodes to generate.
    barcode_width(int): Width of barcode in pixels.
    barcode_height(int): Height of barcode in pixels.
    output_directory(string): Output directory where to store generated
      barcodes.
    path_to_zxing(string): The path to Zxing.

  Return:
    (bool): True if the conversion is successful.
  """
  base_file_name = os.path.join(output_directory, "barcode_")
  jars = _form_jars_string(path_to_zxing)
  command_line_encoder = 'com.google.zxing.client.j2se.CommandLineEncoder'
  barcode_width = str(barcode_width)
  barcode_height = str(barcode_height)

  errors = False
  for i in range(number_of_barcodes):
    suffix = helper_functions.zero_pad(i)
    # Barcodes starting from 0
    content = helper_functions.zero_pad(i, 11)
    output_file_name = base_file_name + suffix + ".png"

    command = ["java", "-cp", jars, command_line_encoder,
               "--barcode_format=UPC_A", "--height=%s" % barcode_height,
               "--width=%s" % barcode_width,
               "--output=%s" % (output_file_name), "%s" % (content)]
    try:
      helper_functions.run_shell_command(
          command, fail_msg=('Error during barcode %s generation' % content))
    except helper_functions.HelperError as err:
      print >> sys.stderr, err
      errors = True
  return not errors


def convert_png_to_yuv_barcodes(input_directory='.', output_directory='.'):
  """Converts PNG barcodes to YUV barcode images.

  This function reads all the PNG files from the input directory which are in
  the format frame_xxxx.png, where xxxx is the number of the frame, starting
  from 0000. The frames should be consecutive numbers. The output YUV file is
  named frame_xxxx.yuv. The function uses ffmpeg to do the conversion.

  Args:
    input_directory(string): The input direcotry to read the PNG barcodes from.
    output_directory(string): The putput directory to write the YUV files to.
  Return:
    (bool): True if the conversion was without errors.
  """
  return helper_functions.perform_action_on_all_files(
      input_directory, 'barcode_', 'png', 0, _convert_to_yuv_and_delete,
      output_directory=output_directory, pattern='barcode_')


def _convert_to_yuv_and_delete(output_directory, file_name, pattern):
  """Converts a PNG file to a YUV file and deletes the PNG file.

  Args:
    output_directory(string): The output directory for the YUV file.
    file_name(string): The PNG file name.
    pattern(string): The file pattern of the PNG/YUV file. The PNG/YUV files are
      named patternxx..x.png/yuv, where xx..x are digits starting from 00..0.
  Return:
    (bool): True upon successful conversion, false otherwise.
  """
  # Pattern should be in file name
  if not pattern in file_name:
    return False
  pattern_position = file_name.rfind(pattern)

  # Strip the path to the PNG file and replace the png extension with yuv
  yuv_file_name = file_name[pattern_position:-3] + 'yuv'
  yuv_file_name = os.path.join(output_directory, yuv_file_name)

  command = ['ffmpeg', '-i', '%s' % (file_name), '-pix_fmt', 'yuv420p',
             '%s' % (yuv_file_name)]
  try:
    helper_functions.run_shell_command(
        command, fail_msg=('Error during PNG to YUV conversion of %s' %
                           file_name))
    os.remove(file_name)
  except helper_functions.HelperError as err:
    print >> sys.stderr, err
    return False
  return True


def combine_yuv_frames_into_one_file(output_file_name, input_directory='.'):
  """Combines several YUV frames into one YUV video file.

  The function combines the YUV frames from input_directory into one YUV video
  file. The frames should be named in the format frame_xxxx.yuv where xxxx
  stands for the frame number. The numbers have to be consecutive and start from
  0000. The YUV frames are removed after they have been added to the video.

  Args:
    output_file_name(string): The name of the file to produce.
    input_directory(string): The directory from which the YUV frames are read.
  Return:
    (bool): True if the frame stitching went OK.
  """
  output_file = open(output_file_name, "wb")
  success = helper_functions.perform_action_on_all_files(
      input_directory, 'barcode_', 'yuv', 0, _add_to_file_and_delete,
      output_file=output_file)
  output_file.close()
  return success

def _add_to_file_and_delete(output_file, file_name):
  """Adds the contents of a file to a previously opened file.

  Args:
    output_file(file): The ouput file, previously opened.
    file_name(string): The file name of the file to add to the output file.

  Return:
    (bool): True if successful, False otherwise.
  """
  input_file = open(file_name, "rb")
  input_file_contents = input_file.read()
  output_file.write(input_file_contents)
  input_file.close()
  try:
    os.remove(file_name)
  except OSError as e:
    print >> sys.stderr, 'Error deleting file %s.\nError: %s' % (file_name, e)
    return False
  return True


def _overlay_barcode_and_base_frames(barcodes_file, base_file, output_file,
                                     barcodes_component_sizes,
                                     base_component_sizes):
  """Overlays the next YUV frame from a file with a barcode.

  Args:
    barcodes_file(FileObject): The YUV file containing the barcodes (opened).
    base_file(FileObject): The base YUV file (opened).
    output_file(FileObject): The output overlaid file (opened).
    barcodes_component_sizes(list of tuples): The width and height of each Y, U
      and V plane of the barcodes YUV file.
    base_component_sizes(list of tuples): The width and height of each Y, U and
      V plane of the base YUV file.
  Return:
    (bool): True if there are more planes (i.e. frames) in the base file, false
      otherwise.
  """
  # We will loop three times - once for the Y, U and V planes
  for ((barcode_comp_width, barcode_comp_height),
      (base_comp_width, base_comp_height)) in zip(barcodes_component_sizes,
                                                  base_component_sizes):
    for base_row in range(base_comp_height):
      barcode_plane_traversed = False
      if (base_row < barcode_comp_height) and not barcode_plane_traversed:
        barcode_plane = barcodes_file.read(barcode_comp_width)
        if barcode_plane == "":
          barcode_plane_traversed = True
      else:
        barcode_plane_traversed = True
      base_plane = base_file.read(base_comp_width)

      if base_plane == "":
        return False

      if not barcode_plane_traversed:
        # Substitute part of the base component with the top component
        output_file.write(barcode_plane)
        base_plane = base_plane[barcode_comp_width:]
      output_file.write(base_plane)
  return True


def overlay_yuv_files(barcode_width, barcode_height, base_width, base_height,
                      barcodes_file_name, base_file_name, output_file_name):
  """Overlays two YUV files starting from the upper left corner of both.

  Args:
    barcode_width(int): The width of the barcode (to be overlaid).
    barcode_height(int): The height of the barcode (to be overlaid).
    base_width(int): The width of a frame of the base file.
    base_height(int): The height of a frame of the base file.
    barcodes_file_name(string): The name of the YUV file containing the YUV
      barcodes.
    base_file_name(string): The name of the base YUV file.
    output_file_name(string): The name of the output file where the overlaid
      video will be written.
  """
  # Component sizes = [Y_sizes, U_sizes, V_sizes]
  barcodes_component_sizes = [(barcode_width, barcode_height),
                              (barcode_width/2, barcode_height/2),
                              (barcode_width/2, barcode_height/2)]
  base_component_sizes = [(base_width, base_height),
                          (base_width/2, base_height/2),
                          (base_width/2, base_height/2)]

  barcodes_file = open(barcodes_file_name, 'rb')
  base_file = open(base_file_name, 'rb')
  output_file = open(output_file_name, 'wb')

  data_left = True
  while data_left:
    data_left = _overlay_barcode_and_base_frames(barcodes_file, base_file,
                                                 output_file,
                                                 barcodes_component_sizes,
                                                 base_component_sizes)

  barcodes_file.close()
  base_file.close()
  output_file.close()


def calculate_frames_number_from_yuv(yuv_width, yuv_height, file_name):
  """Calculates the number of frames of a YUV video.

  Args:
    yuv_width(int): Width of a frame of the yuv file.
    yuv_height(int): Height of a frame of the YUV file.
    file_name(string): The name of the YUV file.
  Return:
    (int): The number of frames in the YUV file.
  """
  file_size = os.path.getsize(file_name)

  y_plane_size = yuv_width * yuv_height
  u_plane_size = (yuv_width/2) * (yuv_height/2)  # Equals to V plane size too
  frame_size = y_plane_size + (2 * u_plane_size)
  return int(file_size/frame_size)  # Should be int anyway


def _form_jars_string(path_to_zxing):
  """Forms the the Zxing core and javase jars argument.

  Args:
    path_to_zxing(string): The path to the Zxing checkout folder.
  Return:
    (string): The newly formed jars argument.
  """
  javase_jar = os.path.join(path_to_zxing, "javase", "javase.jar")
  core_jar = os.path.join(path_to_zxing, "core", "core.jar")
  delimiter = ':'
  if os.name != 'posix':
    delimiter = ';'
  return javase_jar + delimiter + core_jar

def _parse_args():
  """Registers the command-line options."""
  usage = "usage: %prog [options]"
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('--barcode_width', type='int',
                    default=_DEFAULT_BARCODE_WIDTH,
                    help=('Width of the barcodes to be overlaid on top of the'
                          ' base file. Default: %default'))
  parser.add_option('--barcode_height', type='int', default=32,
                    help=('Height of the barcodes to be overlaid on top of the'
                          ' base file. Default: %default'))
  parser.add_option('--base_frame_width', type='int', default=352,
                    help=('Width of the base YUV file\'s frames. '
                          'Default: %default'))
  parser.add_option('--base_frame_height', type='int', default=288,
                    help=('Height of the top YUV file\'s frames. '
                          'Default: %default'))
  parser.add_option('--barcodes_yuv', type='string',
                    default=_DEFAULT_BARCODES_FILE,
                    help=('The YUV file with the barcodes in YUV. '
                          'Default: %default'))
  parser.add_option('--base_yuv', type='string', default='base.yuv',
                    help=('The base YUV file to be overlaid. '
                          'Default: %default'))
  parser.add_option('--output_yuv', type='string', default='output.yuv',
                    help=('The output YUV file containing the base overlaid'
                          ' with the barcodes. Default: %default'))
  parser.add_option('--png_barcodes_output_dir', type='string', default='.',
                    help=('Output directory where the PNG barcodes will be '
                          'generated. Default: %default'))
  parser.add_option('--png_barcodes_input_dir', type='string', default='.',
                    help=('Input directory from where the PNG barcodes will be '
                          'read. Default: %default'))
  parser.add_option('--yuv_barcodes_output_dir', type='string', default='.',
                    help=('Output directory where the YUV barcodes will be '
                          'generated. Default: %default'))
  parser.add_option('--yuv_frames_input_dir', type='string', default='.',
                    help=('Input directory from where the YUV will be '
                          'read before combination. Default: %default'))
  parser.add_option('--zxing_dir', type='string', default='zxing',
                    help=('Path to the Zxing barcodes library. '
                          'Default: %default'))
  options = parser.parse_args()[0]
  return options


def _main():
  """The main function.

  A simple invocation will be:
  ./webrtc/tools/barcode_tools/barcode_encoder.py --barcode_height=32
  --base_frame_width=352 --base_frame_height=288
  --base_yuv=<path_and_name_of_base_file>
  --output_yuv=<path and name_of_output_file>
  """
  options = _parse_args()
  # The barcodes with will be different than the base frame width only if
  # explicitly specified at the command line.
  if options.barcode_width == _DEFAULT_BARCODE_WIDTH:
    options.barcode_width = options.base_frame_width
  # If the user provides a value for the barcodes YUV video file, we will keep
  # it. Otherwise we create a temp file which is removed after it has been used.
  keep_barcodes_yuv_file = False
  if options.barcodes_yuv != _DEFAULT_BARCODES_FILE:
    keep_barcodes_yuv_file = True

  # Calculate the number of barcodes - it is equal to the number of frames in
  # the base file.
  number_of_barcodes = calculate_frames_number_from_yuv(
      options.base_frame_width, options.base_frame_height, options.base_yuv)

  script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
  zxing_dir = os.path.join(script_dir, 'third_party', 'zxing')
  # Generate barcodes - will generate them in PNG.
  generate_upca_barcodes(number_of_barcodes, options.barcode_width,
                         options.barcode_height,
                         output_directory=options.png_barcodes_output_dir,
                         path_to_zxing=zxing_dir)
  # Convert the PNG barcodes to to YUV format.
  convert_png_to_yuv_barcodes(options.png_barcodes_input_dir,
                              options.yuv_barcodes_output_dir)
  # Combine the YUV barcodes into one YUV file.
  combine_yuv_frames_into_one_file(options.barcodes_yuv,
                                   input_directory=options.yuv_frames_input_dir)
  # Overlay the barcodes over the base file.
  overlay_yuv_files(options.barcode_width, options.barcode_height,
                    options.base_frame_width, options.base_frame_height,
                    options.barcodes_yuv, options.base_yuv, options.output_yuv)

  if not keep_barcodes_yuv_file:
    # Remove the temporary barcodes YUV file
    os.remove(options.barcodes_yuv)


if __name__ == '__main__':
  sys.exit(_main())
