#!/bin/sh
#
# This script scans Source for certain types of handwritten serialization logic and provides a count
#

cd "$(dirname "$0")/../../Source/"

exclusions_array=(
"Scripts/webkit/tests"
"generate-serializers.py"
"WTF/wtf/ArgumentCoder.h"
"JavaScriptCore/runtime/CachedTypes.cpp"
"gtk"
"glib"
"IPC/win"
"IPC/unix"
"IPC/DaemonCoders.cpp"
"win/WebCoreArgumentCodersWin.cpp"
"Cocoa/ArgumentCodersCocoa.h"
"Cocoa/CoreIPCNSCFObject.h"
"WebCore/platform/graphics/gbm"
);

exclusions_string="";

for ((i = 0; i < ${#exclusions_array[@]}; i++))
do
    exclusions_string+="grep -vi ";
    exclusions_string+=${exclusions_array[$i]};
    exclusions_string+=" | ";
done

exclusions_and_count_string="$exclusions_string wc -l | xargs";

optionalsString="grep -riE \"> decode\((Decoder|IPC::Decoder)\" . | $exclusions_and_count_string";
boolsString="grep -riE \"bool decode\((Decoder|IPC::Decoder)\" . | $exclusions_and_count_string";

enumtraits=$(grep -ri "struct EnumTraits<" | wc -l | xargs);
optionals=$(eval "$optionalsString");
bools=$(eval "$boolsString");

echo "EnumTraits remaining: $enumtraits"
echo "Decoders that return std::optional<>: $optionals"
echo "Legacy decoders that return bool: $bools"
sum=$(($optionals+$bools))
echo "Total decoders remaining: $sum"
