# Copyright (C) 2020 Apple Inc. All rights reserved.

import os
import glob

from webkitscmpy.mocks import local
from webkitscmpy.mocks import remote


def add_datafiles_to_pyfakefs(fs):
    for path in glob.iglob(os.path.join(os.path.dirname(__file__), "*.json")):
        fs.add_real_file(path)
