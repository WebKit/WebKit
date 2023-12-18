#!/bin/sh
#
# This script scans Source/WebCore and Source/WebKit/WebProcess for places where we are likely skipping code on out-of-process frames and provides a count.
#

count_occurrences() {
    local pattern=$1
    local flag=$2
    grep "$flag" "$pattern" "$(dirname "$0")/../../Source/WebCore" "$(dirname "$0")/../../Source/WebKit/WebProcess" | wc -l | xargs
}

downcastLocalFrame=$(count_occurrences "[dD]owncast<\(WebCore::\)\?LocalFrame>" "-r")
downcastLocalFrameView=$(count_occurrences "[dD]owncast<\(WebCore::\)\?LocalFrameView>" "-r")
topDocument=$(count_occurrences "topDocument()" "-r")
parentDocument=$(count_occurrences "parentDocument()" "-r")

echo "Downcast LocalFrame remaining: $downcastLocalFrame"
echo "Downcast LocalFrameView remaining: $downcastLocalFrameView"
echo "topDocument() remaining: $topDocument"
echo "parentDocument() remaining: $parentDocument"

sum=$((downcastLocalFrame + downcastLocalFrameView + topDocument + parentDocument))
echo "Total skipped out-of-process frames remaining: $sum"
