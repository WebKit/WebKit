# Copyright (C) 2019-2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from resultsdbpy import version
from setuptools import setup


def readme():
    with open('README.md') as f:
        return f.read()


setup(
    name='resultsdbpy',
    version=str(version),
    description='Library for visualizing, processing and storing test results.',
    long_description=readme(),
    classifiers=[
        'Development Status :: 4 - Beta',
        'Framework :: Flask',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Operating System :: MacOS',
        'Natural Language :: English',
        'Programming Language :: Python :: 3 :: Only',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'Topic :: Software Development :: Testing',
    ],
    keywords='database history results testing webkit',
    url='https://svn.webkit.org/repository/webkit/trunk/Tools/resultsdbpy',
    author='Jonathan Bedard',
    author_email='jbedard@apple.com',
    license='Modified BSD',
    packages=['resultsdbpy'],
    install_requires=[
        'cassandra-driver',
        'fakeredis',
        'Flask',
        'Flask-Cors',
        'gunicorn',
        'lupa',
        'redis',
        'xmltodict',
        'selenium',
        'webkitcorepy',
    ],
    include_package_data=True,
    zip_safe=False,
)
