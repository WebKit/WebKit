# Copyright 2022 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import importlib
import logging


class LogFormatter(logging.Formatter):

    def __init__(self):
        logging.Formatter.__init__(self, fmt='%(levelname).1s%(asctime)s %(message)s')

    def formatTime(self, record, datefmt=None):
        # Drop date as these scripts are short lived
        return datetime.datetime.fromtimestamp(record.created).strftime('%H:%M:%S.%fZ')


def setupLogging(level):
    # Reload to reset if it was already setup by a library
    importlib.reload(logging)

    logger = logging.getLogger()
    logger.setLevel(level)

    handler = logging.StreamHandler()
    handler.setFormatter(LogFormatter())
    logger.addHandler(handler)
