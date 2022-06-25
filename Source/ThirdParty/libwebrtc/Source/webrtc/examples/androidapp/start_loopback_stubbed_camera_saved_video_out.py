# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

from optparse import OptionParser
import random
import string
import subprocess
import sys
import time

from com.android.monkeyrunner import MonkeyRunner, MonkeyDevice


def main():
    parser = OptionParser()

    parser.add_option('--devname', dest='devname', help='The device id')

    parser.add_option(
        '--videooutsave',
        dest='videooutsave',
        help='The path where to save the video out file on local computer')

    parser.add_option('--videoout',
                      dest='videoout',
                      help='The path where to put the video out file')

    parser.add_option('--videoout_width',
                      dest='videoout_width',
                      type='int',
                      help='The width for the video out file')

    parser.add_option('--videoout_height',
                      dest='videoout_height',
                      type='int',
                      help='The height for the video out file')

    parser.add_option(
        '--videoin',
        dest='videoin',
        help='The path where to read input file instead of camera')

    parser.add_option('--call_length',
                      dest='call_length',
                      type='int',
                      help='The length of the call')

    (options, args) = parser.parse_args()

    print(options, args)

    devname = options.devname

    videoin = options.videoin

    videoout = options.videoout
    videoout_width = options.videoout_width
    videoout_height = options.videoout_height

    videooutsave = options.videooutsave

    call_length = options.call_length or 10

    room = ''.join(
        random.choice(string.ascii_letters + string.digits) for _ in range(8))

    # Delete output video file.
    if videoout:
        subprocess.check_call(
            ['adb', '-s', devname, 'shell', 'rm', '-f', videoout])

    device = MonkeyRunner.waitForConnection(2, devname)

    extras = {
        'org.appspot.apprtc.USE_VALUES_FROM_INTENT': True,
        'org.appspot.apprtc.AUDIOCODEC': 'OPUS',
        'org.appspot.apprtc.LOOPBACK': True,
        'org.appspot.apprtc.VIDEOCODEC': 'VP8',
        'org.appspot.apprtc.CAPTURETOTEXTURE': False,
        'org.appspot.apprtc.CAMERA2': False,
        'org.appspot.apprtc.ROOMID': room
    }

    if videoin:
        extras.update({'org.appspot.apprtc.VIDEO_FILE_AS_CAMERA': videoin})

    if videoout:
        extras.update({
            'org.appspot.apprtc.SAVE_REMOTE_VIDEO_TO_FILE':
            videoout,
            'org.appspot.apprtc.SAVE_REMOTE_VIDEO_TO_FILE_WIDTH':
            videoout_width,
            'org.appspot.apprtc.SAVE_REMOTE_VIDEO_TO_FILE_HEIGHT':
            videoout_height
        })

    print extras

    device.startActivity(data='https://appr.tc',
                         action='android.intent.action.VIEW',
                         component='org.appspot.apprtc/.ConnectActivity',
                         extras=extras)

    print 'Running a call for %d seconds' % call_length
    for _ in xrange(call_length):
        sys.stdout.write('.')
        sys.stdout.flush()
        time.sleep(1)
    print '\nEnding call.'

    # Press back to end the call. Will end on both sides.
    device.press('KEYCODE_BACK', MonkeyDevice.DOWN_AND_UP)

    if videooutsave:
        time.sleep(2)

        subprocess.check_call(
            ['adb', '-s', devname, 'pull', videoout, videooutsave])


if __name__ == '__main__':
    main()
