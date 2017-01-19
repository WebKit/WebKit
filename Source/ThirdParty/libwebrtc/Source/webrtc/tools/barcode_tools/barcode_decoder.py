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

if __name__ == '__main__':
  # Make sure we always can import helper_functions.
  sys.path.append(os.path.dirname(__file__))

import helper_functions

# Chrome browsertests will throw away stderr; avoid that output gets lost.
sys.stderr = sys.stdout


def convert_yuv_to_png_files(yuv_file_name, yuv_frame_width, yuv_frame_height,
                             output_directory, ffmpeg_path):
  """Converts a YUV video file into PNG frames.

  The function uses ffmpeg to convert the YUV file. The output of ffmpeg is in
  the form frame_xxxx.png, where xxxx is the frame number, starting from 0001.

  Args:
    yuv_file_name(string): The name of the YUV file.
    yuv_frame_width(int): The width of one YUV frame.
    yuv_frame_height(int): The height of one YUV frame.
    output_directory(string): The output directory where the PNG frames will be
      stored.
    ffmpeg_path(string): The path to the ffmpeg executable. If None, the PATH
      will be searched for it.

  Return:
    (bool): True if the conversion was OK.
  """
  size_string = str(yuv_frame_width) + 'x' + str(yuv_frame_height)
  output_files_pattern = os.path.join(output_directory, 'frame_%04d.png')
  if not ffmpeg_path:
    ffmpeg_path = 'ffmpeg.exe' if sys.platform == 'win32' else 'ffmpeg'
  command = [ffmpeg_path, '-s', '%s' % size_string, '-i', '%s'
             % yuv_file_name, '-f', 'image2', '-vcodec', 'png',
             '%s' % output_files_pattern]
  try:
    print 'Converting YUV file to PNG images (may take a while)...'
    print ' '.join(command)
    helper_functions.run_shell_command(
        command, fail_msg='Error during YUV to PNG conversion')
  except helper_functions.HelperError, err:
    print 'Error executing command: %s. Error: %s' % (command, err)
    return False
  except OSError:
    print 'Did not find %s. Have you installed it?' % ffmpeg_path
    return False
  return True


def decode_frames(input_directory, zxing_path):
  """Decodes the barcodes overlaid in each frame.

  The function uses the Zxing command-line tool from the Zxing C++ distribution
  to decode the barcode in every PNG frame from the input directory. The frames
  should be named frame_xxxx.png, where xxxx is the frame number. The frame
  numbers should be consecutive and should start from 0001.
  The decoding results in a frame_xxxx.txt file for every successfully decoded
  barcode. This file contains the decoded barcode as 12-digit string (UPC-A
  format: 11 digits content + one check digit).

  Args:
    input_directory(string): The input directory from where the PNG frames are
      read.
    zxing_path(string): The path to the zxing binary. If specified as None,
      the PATH will be searched for it.
  Return:
    (bool): True if the decoding succeeded.
  """
  if not zxing_path:
    zxing_path = 'zxing.exe' if sys.platform == 'win32' else 'zxing'
  print 'Decoding barcodes from PNG files with %s...' % zxing_path
  return helper_functions.perform_action_on_all_files(
      directory=input_directory, file_pattern='frame_',
      file_extension='png', start_number=1, action=_decode_barcode_in_file,
      command_line_decoder=zxing_path)


def _decode_barcode_in_file(file_name, command_line_decoder):
  """Decodes the barcode in the upper left corner of a PNG file.

  Args:
    file_name(string): File name of the PNG file.
    command_line_decoder(string): The ZXing command-line decoding tool.

  Return:
    (bool): True upon success, False otherwise.
  """
  command = [command_line_decoder, '--try-harder', '--dump-raw', file_name]
  try:
    out = helper_functions.run_shell_command(
        command, fail_msg='Error during decoding of %s' % file_name)
    text_file = open('%s.txt' % file_name[:-4], 'w')
    text_file.write(out)
    text_file.close()
  except helper_functions.HelperError, err:
    print 'Barcode in %s cannot be decoded.' % file_name
    print err
    return False
  except OSError:
    print 'Did not find %s. Have you installed it?' % command_line_decoder
    return False
  return True


def _generate_stats_file(stats_file_name, input_directory='.'):
  """Generate statistics file.

  The function generates a statistics file. The contents of the file are in the
  format <frame_name> <barcode>, where frame name is the name of every frame
  (effectively the frame number) and barcode is the decoded barcode. The frames
  and the helper .txt files are removed after they have been used.
  """
  file_prefix = os.path.join(input_directory, 'frame_')
  stats_file = open(stats_file_name, 'w')

  print 'Generating stats file: %s' % stats_file_name
  for i in range(1, _count_frames_in(input_directory=input_directory) + 1):
    frame_number = helper_functions.zero_pad(i)
    barcode_file_name = file_prefix + frame_number + '.txt'
    png_frame = file_prefix + frame_number + '.png'
    entry_frame_number = helper_functions.zero_pad(i-1)
    entry = 'frame_' + entry_frame_number + ' '

    if os.path.isfile(barcode_file_name):
      barcode = _read_barcode_from_text_file(barcode_file_name)
      os.remove(barcode_file_name)

      if _check_barcode(barcode):
        entry += (helper_functions.zero_pad(int(barcode[0:11])) + '\n')
      else:
        entry += 'Barcode error\n'  # Barcode is wrongly detected.
    else:  # Barcode file doesn't exist.
      entry += 'Barcode error\n'

    stats_file.write(entry)
    os.remove(png_frame)

  stats_file.close()


def _read_barcode_from_text_file(barcode_file_name):
  """Reads the decoded barcode for a .txt file.

  Args:
    barcode_file_name(string): The name of the .txt file.
  Return:
    (string): The decoded barcode.
  """
  barcode_file = open(barcode_file_name, 'r')
  barcode = barcode_file.read()
  barcode_file.close()
  return barcode


def _check_barcode(barcode):
  """Check weather the UPC-A barcode was decoded correctly.

  This function calculates the check digit of the provided barcode and compares
  it to the check digit that was decoded.

  Args:
    barcode(string): The barcode (12-digit).
  Return:
    (bool): True if the barcode was decoded correctly.
  """
  if len(barcode) != 12:
    return False

  r1 = range(0, 11, 2)  # Odd digits
  r2 = range(1, 10, 2)  # Even digits except last
  dsum = 0
  # Sum all the even digits
  for i in r1:
    dsum += int(barcode[i])
  # Multiply the sum by 3
  dsum *= 3
  # Add all the even digits except the check digit (12th digit)
  for i in r2:
    dsum += int(barcode[i])
  # Get the modulo 10
  dsum = dsum % 10
  # If not 0 substract from 10
  if dsum != 0:
    dsum = 10 - dsum
  # Compare result and check digit
  return dsum == int(barcode[11])


def _count_frames_in(input_directory='.'):
  """Calculates the number of frames in the input directory.

  The function calculates the number of frames in the input directory. The
  frames should be named frame_xxxx.png, where xxxx is the number of the frame.
  The numbers should start from 1 and should be consecutive.

  Args:
    input_directory(string): The input directory.
  Return:
    (int): The number of frames.
  """
  file_prefix = os.path.join(input_directory, 'frame_')
  file_exists = True
  num = 1

  while file_exists:
    file_name = (file_prefix + helper_functions.zero_pad(num) + '.png')
    if os.path.isfile(file_name):
      num += 1
    else:
      file_exists = False
  return num - 1


def _parse_args():
  """Registers the command-line options."""
  usage = "usage: %prog [options]"
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('--zxing_path', type='string',
                    help=('The path to where the zxing executable is located. '
                          'If omitted, it will be assumed to be present in the '
                          'PATH with the name zxing[.exe].'))
  parser.add_option('--ffmpeg_path', type='string',
                    help=('The path to where the ffmpeg executable is located. '
                          'If omitted, it will be assumed to be present in the '
                          'PATH with the name ffmpeg[.exe].'))
  parser.add_option('--yuv_frame_width', type='int', default=640,
                    help='Width of the YUV file\'s frames. Default: %default')
  parser.add_option('--yuv_frame_height', type='int', default=480,
                    help='Height of the YUV file\'s frames. Default: %default')
  parser.add_option('--yuv_file', type='string', default='output.yuv',
                    help='The YUV file to be decoded. Default: %default')
  parser.add_option('--stats_file', type='string', default='stats.txt',
                    help='The output stats file. Default: %default')
  parser.add_option('--png_working_dir', type='string', default='.',
                    help=('The directory for temporary PNG images to be stored '
                          'in when decoding from YUV before they\'re barcode '
                          'decoded. If using Windows and a Cygwin-compiled '
                          'zxing.exe, you should keep the default value to '
                          'avoid problems. Default: %default'))
  options, _ = parser.parse_args()
  return options


def _main():
  """The main function.

  A simple invocation is:
  ./webrtc/tools/barcode_tools/barcode_decoder.py
  --yuv_file=<path_and_name_of_overlaid_yuv_video>
  --yuv_frame_width=640 --yuv_frame_height=480
  --stats_file=<path_and_name_to_stats_file>
  """
  options = _parse_args()

  # Convert the overlaid YUV video into a set of PNG frames.
  if not convert_yuv_to_png_files(options.yuv_file, options.yuv_frame_width,
                                  options.yuv_frame_height,
                                  output_directory=options.png_working_dir,
                                  ffmpeg_path=options.ffmpeg_path):
    print 'An error occurred converting from YUV to PNG frames.'
    return -1

  # Decode the barcodes from the PNG frames.
  if not decode_frames(input_directory=options.png_working_dir,
                       zxing_path=options.zxing_path):
    print 'An error occurred decoding barcodes from PNG frames.'
    return -2

  # Generate statistics file.
  _generate_stats_file(options.stats_file,
                       input_directory=options.png_working_dir)
  print 'Completed barcode decoding.'
  return 0

if __name__ == '__main__':
  sys.exit(_main())
