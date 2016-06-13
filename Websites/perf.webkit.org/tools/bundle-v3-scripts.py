#!/usr/bin/python

import os
import re
import subprocess
import sys
import xml.dom.minidom


def main(argv):
    tools_dir = os.path.dirname(__file__)
    public_v3_dir = os.path.abspath(os.path.join(tools_dir, '..', 'public', 'v3'))

    bundled_script = ''

    index_html = xml.dom.minidom.parse(os.path.join(public_v3_dir, 'index.html'))
    for template in index_html.getElementsByTagName('template'):
        if template.getAttribute('id') != 'unbundled-scripts':
            continue
        unbundled_scripts = template.getElementsByTagName('script')
        for script in unbundled_scripts:
            src = script.getAttribute('src')
            with open(os.path.join(public_v3_dir, src)) as script_file:
                script_content = script_file.read()
                script_content = re.sub(r'([\"\'])use strict\1;', '', script_content)
                bundled_script += script_content

    jsmin = subprocess.Popen(['python', os.path.join(tools_dir, 'jsmin.py')], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    minified_script = jsmin.communicate(input=bundled_script)[0]

    new_size = float(len(minified_script))
    old_size = float(len(bundled_script))
    print '%d -> %d (%.1f%%)' % (old_size, new_size, new_size / old_size * 100)

    with open(os.path.join(public_v3_dir, 'bundled-scripts.js'), 'w') as bundled_file:
        bundled_file.write(minified_script)

if __name__ == "__main__":
    main(sys.argv)
