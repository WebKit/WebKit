#!/bin/sh

# DOM Level 1/2/3 - automated regression testing
# Adjust OUTPUTDIR

if [ ! -n "$OUTPUTDIR" ]; then
   echo "OUTPUTDIR not set, defaulting to /tmp"
   OUTPUTDIR=/tmp
elif [ ! -d "$OUTPUTDIR" ]; then
   echo "OUTPUTDIR ($OUTPUTDIR) does not exist, defaulting to /tmp"
   OUTPUTDIR=/tmp
fi

echo "Running ksvg regression test suite.  Outputing results to $OUTPUTDIR"

function runSuite
{
	DIR="$1"
	NAME="$2"

	cd "$DIR" || exit 1
	echo "Testing $DIR...";
	../../scripts/ksvgparsertest.sh || exit 1
	xsltproc ../../scripts/ksvgstatus.xsl "status-`date +%y-%m-%d`.xml" > "status-`date +%y-%m-%d`.html" || exit 1
	touch "$OUTPUTDIR/result-$NAME.html"
	mv "$OUTPUTDIR/result-$NAME.html" "$OUTPUTDIR/result-$NAME-last.html"
	cp "status-`date +%y-%m-%d`.html" "$OUTPUTDIR/result-$NAME.html"
	diff -u "$OUTPUTDIR/result-$NAME-last.html" "$OUTPUTDIR/result-$NAME.html" > "$OUTPUTDIR/result-$NAME.diff"
	cd ..
}

runSuite "Level1Core" "dom1-core"
runSuite "Level2Core" "dom2-core"
runSuite "Level2Events" "dom2-events"
runSuite "Level3Core" "dom3-core"
runSuite "Level3LS" "dom3-ls"
runSuite "Level3XPath" "dom3-xpath"

# This uploads the new results to http://ktown.kde.org/~wildfox/ktown
#cd /home/nikoz
#ktown/update
