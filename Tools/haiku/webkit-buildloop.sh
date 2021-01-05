#!/bin/sh

cd /path/to/working/dir

function build_webkit {
	rm -r JavaScriptCore/DerivedSources
	rm -r WebCore/DerivedSources
	rm -r WebKitBuild
	/bin/sh WebKitTools/haiku/make-generated-sources.sh > logs/buildlog_$1.log 2>&1
	NDEBUG=1 jam -qj3 WebPositive >> logs/buildlog_$1.log 2>&1
	if [ $? == 0 ]
	then
		cp WebKitBuild/Release/JavaScriptCore/libjavascriptcore.so package/WebPositive/lib
		cp WebKitBuild/Release/WebCore/libwebcore.so package/WebPositive/lib
		cp WebKitBuild/Release/WebKit/libwebkit.so package/WebPositive/lib
		cp WebKitBuild/Release/WebKit/WebPositive package/WebPositive
		cd package
		zip -r WebPositive.zip WebPositive
		cd ..
		cp package/WebPositive.zip /Source/trac/webkit/htdocs/nightlies/WebPositive-r$1.zip
		generate-index.sh /target/dir/nightlies http://base.url/
	fi
	cp logs/buildlog_$1.log /target/dir/buildlogs/buildlog_$1.txt
	generate-index.sh /target/dir/buildlogs http://base.url/
}

while true
do
	oldRevision=$(svn info | grep Revision | cut -d" " -f 2)
	svn update > /dev/null 2>&1
	newRevision=$(svn info | grep Revision | cut -d" " -f 2)
	if [ $oldRevision != $newRevision ]
	then
		build_webkit $newRevision
	fi

	sleep 60
done

