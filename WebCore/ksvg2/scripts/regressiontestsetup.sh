#!/bin/sh

# Automatically setup the dom level 1/2/3 testing environment
# -----------------------------------------------------------
# Step 1) Call "cd /path/to/your/ksvg2/cvs/sources/"
# Step 2) Call "scripts/regressiontestsetup.sh"
# Step 3) Adjust 'OUTPUTDIR' of the scripts/regressiontest.sh
# Step 4) Call "cd TestSuite"
# Step 5) Call "../scripts/regressiontest.sh"
# Step 6) Be patient :)

WGET=`which wget`
if [ $? -ne 0 ]; then
   WGET="`which curl` -O -C -"
   if [ $? -ne 0 ]; then
      echo "wget or curl is required for this script!"; exit 1;
   fi
fi

function getTestSuite()
{
	LEVEL="$1"
	MODULE="$2"
	DIR="$3"
	
	cd TestSuite
	
	$WGET http://homepage.mac.com/curt.arnold/.Public/dom$LEVEL-$MODULE.tar.gz
	tar xfzv dom$LEVEL-$MODULE.tar.gz self-hosted/svg/level$LEVEL/$MODULE
	mkdir $DIR
	mv self-hosted/svg/level$LEVEL/$MODULE/* $DIR
	rm -Rf self-hosted

	cd ..
}

mkdir TestSuite &>/dev/null

getTestSuite "1" "core" "Level1Core"
getTestSuite "2" "core" "Level2Core"
getTestSuite "2" "events" "Level2Events"
getTestSuite "3" "core" "Level3Core"
getTestSuite "3" "ls" "Level3LS"
getTestSuite "3" "xpath" "Level3XPath"
