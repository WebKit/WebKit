#!/bin/sh
# Copyright (c) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if [[ $# -ne 4 ]];then
echo "Usage: cold-boot.sh QUEUE_TYPE BOT_ID BUGZILLA_USERNAME BUGZILLA_PASSWORD"
exit 1
fi

# Format the disk
cat <<EOF | sudo fdisk /dev/vdb
n
p
1
8

w
EOF

sudo mkfs.ext4 /dev/vdb1
sudo mount /dev/vdb1 /mnt

echo ttf-mscorefonts-installer msttcorefonts/accepted-mscorefonts-eula select true | sudo debconf-set-selections

curl http://src.chromium.org/svn/trunk/src/build/install-build-deps.sh > install-build-deps.sh
bash install-build-deps.sh --no-prompt
sudo apt-get install xvfb screen git-svn zip -y

# install-build-deps.sh will install flashplugin-installer, which causes some plug-in tests to crash.
sudo apt-get remove flashplugin-installer -y

cd /mnt
sudo mkdir -p git
sudo chown $USER git
sudo chgrp $USER git
cd git

echo "Cloning WebKit git repository, process takes ~30m."
echo "Note: No status output will be shown via remote pipe."
git clone http://git.chromium.org/external/Webkit.git
mv Webkit webkit-$1
cd webkit-$1

cat >> .git/config <<EOF
[bugzilla]
	username = $3
	password = $4
EOF

if [[ $1 == "commit-queue" ]];then
cat >> .git/config <<EOF
[svn-remote "svn"]
	url = http://svn.webkit.org/repository/webkit
	fetch = trunk:refs/remotes/origin/master
[user]
	email = commit-queue@webkit.org
	name = Commit Queue
EOF
fi

cd ~/tools
echo "screen -t kr ./start-queue.sh" $1 $2 > screen-config
bash boot.sh
