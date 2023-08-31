# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from os.path import abspath, dirname, join

try:
    from urllib.parse import urljoin
except ImportError:
    from urlparse import urljoin

try:
    from urllib.request import pathname2url
except ImportError:
    from urllib import pathname2url

from setuptools import setup


def _get_adjacent_package_requirement(name):
    d = abspath(dirname(__file__))
    package_path = join(d, "..", name)
    file_url = urljoin("file://localhost", pathname2url(package_path))
    return '{} @ {}'.format(name, file_url)


def readme():
    with open('README.md') as f:
        return f.read()


setup(
    name='webkitscmpy',
    version='6.6.10',
    description='Library designed to interact with git and svn repositories.',
    long_description=readme(),
    classifiers=[
        'Development Status :: 1 - Planning',
        'Intended Audience :: Developers',
        'License :: Other/Proprietary License',
        'Operating System :: MacOS',
        'Natural Language :: English',
        'Programming Language :: Python :: 3',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    keywords='git svn',
    url='https://github.com/WebKit/WebKit/tree/main/Tools/Scripts/libraries/webkitscmpy',
    author='Jonathan Bedard',
    author_email='jbedard@apple.com',
    license='Modified BSD',
    packages=[
        'webkitscmpy',
        'webkitscmpy.local',
        'webkitscmpy.mocks',
        'webkitscmpy.mocks.local',
        'webkitscmpy.mocks.remote',
        'webkitscmpy.program',
        'webkitscmpy.program.canonicalize',
        'webkitscmpy.remote',
        'webkitscmpy.test',
    ],
    scripts=['git-webkit'],
    install_requires=[
        'fasteners',
        'jinja2',
        'monotonic',
        _get_adjacent_package_requirement('webkitcorepy'),
        _get_adjacent_package_requirement('webkitbugspy'),
        'xmltodict',
        'rapidfuzz',
    ],
    include_package_data=True,
    zip_safe=False,
)
