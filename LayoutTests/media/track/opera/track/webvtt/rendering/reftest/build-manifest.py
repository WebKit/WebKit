#!/usr/bin/python

import os
import re

manifest = open('reftest.list', 'w')
paths = [
    './',
    'evil/',
    'selectors/cue/',
    'selectors/cue_function/',
    'selectors/cue_function/bold_object/',
    'selectors/cue_function/italic_object/',
    'selectors/cue_function/underline_object/',
    'selectors/cue_function/class_object/',
    'selectors/cue_function/voice_object/',
    'selectors/default_styles/',
    'bidi/'
]

for path in paths:
    listing = os.listdir(path)
    for infile in listing:
        if re.search(r'-ref\.html$', infile):
            continue
        if not re.search(r'\.html$', infile):
            continue
        manifest.write('== '+path+infile+' '+path+infile[:-5] + '-ref.html\n')

manifest.close()
