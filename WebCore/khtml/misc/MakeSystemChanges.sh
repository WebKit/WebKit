#!/bin/sh

FILES='Makefile.am'
for file in $FILES; do
        if (test -f "$file"); then
                A=`grep APPLE $file`
                if (test -z "$A"); then
                        echo "Moving KDE build file $file..."
                        mv $file $file.kde
                fi
        fi
done

if (! test -L "Makefile.in"); then
        echo "Making link to Apple Makefile.in..."
        ln -s Makefile.in.apple Makefile.in
fi
