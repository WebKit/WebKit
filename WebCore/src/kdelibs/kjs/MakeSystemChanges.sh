#!/bin/sh

FILES='configure.in.in Makefile.am Makefile.in'
for file in $FILES; do
	if (test -f "$file"); then
		A=`grep APPLE $file`                                  
		if (test -z "$A"); then
			echo "Moving KDE build file $file..."
			mv $file $file.kde
		fi                    
	fi                    
done

FILE=Makefile.in
if (! test -L "$FILE"); then
	if (test -f "$FILE"); then
		echo "Removing spurious $FILE..."
		rm -f $file
	fi
	echo "Making link to Apple $FILE..."
	ln -s $FILE.apple $FILE
fi

