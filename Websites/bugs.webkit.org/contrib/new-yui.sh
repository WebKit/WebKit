#!/bin/sh
#
# Updates the version of YUI used by Bugzilla. Just pass the path to 
# an unzipped yui release directory, like:
#
#  contrib/new-yui.sh /path/to/yui-2.8.1/
#
rsync -av --delete $1/build/ js/yui/
cd js/yui
rm -rf editor/ yuiloader-dom-event/
find -name '*.js' -not -name '*-min.js' -not -name '*-dom-event.js' \
     -exec rm -f {} \;
find -name '*-skin.css' -exec rm -f {} \;
find -depth -path '*/assets' -not -path './assets' -exec rm -rf {} \;
rm assets/skins/sam/sprite.psd
rm assets/skins/sam/skin.css
rmdir utilities
