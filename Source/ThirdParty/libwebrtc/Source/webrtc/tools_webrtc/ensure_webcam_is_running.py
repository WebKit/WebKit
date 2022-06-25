#!/usr/bin/env python
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Checks if a virtual webcam is running and starts it if not.

Returns a non-zero return code if the webcam could not be started.

Prerequisites:
* The Python interpreter must have the psutil package installed.
* Windows: a scheduled task named 'ManyCam' must exist and be configured to
  launch ManyCam preconfigured to auto-play the test clip.
* Mac: ManyCam must be installed in the default location and be preconfigured
  to auto-play the test clip.
* Linux: Not implemented

NOTICE: When running this script as a buildbot step, make sure to set
usePTY=False for the build step when adding it, or the subprocess will die as
soon the step has executed.

If any command line arguments are passed to the script, it is executed as a
command in a subprocess.
"""

# psutil is not installed on non-Linux machines by default.
import psutil  # pylint: disable=F0401
import subprocess
import sys

WEBCAM_WIN = ('schtasks', '/run', '/tn', 'ManyCam')
WEBCAM_MAC = ('open', '/Applications/ManyCam/ManyCam.app')


def IsWebCamRunning():
    if sys.platform == 'win32':
        process_name = 'ManyCam.exe'
    elif sys.platform.startswith('darwin'):
        process_name = 'ManyCam'
    elif sys.platform.startswith('linux'):
        # TODO(bugs.webrtc.org/9636): Currently a no-op on Linux: sw webcams no
        # longer in use.
        print 'Virtual webcam: no-op on Linux'
        return True
    else:
        raise Exception('Unsupported platform: %s' % sys.platform)
    for p in psutil.process_iter():
        try:
            if process_name == p.name:
                print 'Found a running virtual webcam (%s with PID %s)' % (
                    p.name, p.pid)
                return True
        except psutil.AccessDenied:
            pass  # This is normal if we query sys processes, etc.
    return False


def StartWebCam():
    try:
        if sys.platform == 'win32':
            subprocess.check_call(WEBCAM_WIN)
            print 'Successfully launched virtual webcam.'
        elif sys.platform.startswith('darwin'):
            subprocess.check_call(WEBCAM_MAC)
            print 'Successfully launched virtual webcam.'
        elif sys.platform.startswith('linux'):
            # TODO(bugs.webrtc.org/9636): Currently a no-op on Linux: sw webcams no
            # longer in use.
            print 'Not implemented on Linux'

    except Exception as e:
        print 'Failed to launch virtual webcam: %s' % e
        return False

    return True


def _ForcePythonInterpreter(cmd):
    """Returns the fixed command line to call the right python executable."""
    out = cmd[:]
    if out[0] == 'python':
        out[0] = sys.executable
    elif out[0].endswith('.py'):
        out.insert(0, sys.executable)
    return out


def Main(argv):
    if not IsWebCamRunning():
        if not StartWebCam():
            return 1

    if argv:
        return subprocess.call(_ForcePythonInterpreter(argv))
    else:
        return 0


if __name__ == '__main__':
    sys.exit(Main(sys.argv[1:]))
