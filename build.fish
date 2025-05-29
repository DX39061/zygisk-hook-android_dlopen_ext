#!/usr/bin/env fish

set MODULE_ID "androidDlopenExtHook"
set ROOT $(pwd)

function echo_red    ; echo -e "\033[31m$argv\033[0m" ; end
function echo_green  ; echo -e "\033[32m$argv\033[0m" ; end
function echo_yellow   ; echo -e "\033[33m$argv\033[0m" ; end

echo_green "=======> create directory"
echo cd $ROOT
cd $ROOT
if not test -d $ROOT/out
    echo mkdir $ROOT/out
    mkdir $ROOT/out
end
if not test -d $ROOT/out/$MODULE_ID
    echo mkdir $ROOT/out/$MODULE_ID
    mkdir $ROOT/out/$MODULE_ID
end
if test -e "module/module.prop"
    echo cp module/module.prop $ROOT/out/$MODULE_ID/module.prop
    cp module/module.prop $ROOT/out/$MODULE_ID/module.prop
else
    echo_red error: plz manually create module/module.prop first
    return
end
if not test -d $ROOT/out/$MODULE_ID/zygisk
    echo mkdir $ROOT/out/$MODULE_ID/zygisk
    mkdir $ROOT/out/$MODULE_ID/zygisk
end

echo_green "=======> ndk-build"
cd $ROOT/module
echo ndk-build
ndk-build
if test $status -eq 0
    echo build success
else
    return
end

echo_green "=======> find *.so file"
echo cd $ROOT/module/libs
cd $ROOT/module/libs
for file in (find . -name "*.so")
    if string match -qr "armeabi-v7a" $file
        echo cp $file $ROOT/out/$MODULE_ID/zygisk/armeabi-v7a.so
        cp $file $ROOT/out/$MODULE_ID/zygisk/armeabi-v7a.so
    else if string match -qr "arm64-v8a" $file
        echo cp $file $ROOT/out/$MODULE_ID/zygisk/arm64-v8a.so
        cp $file $ROOT/out/$MODULE_ID/zygisk/arm64-v8a.so
    else if string match -qr "x86_64" $file
        echo cp $file $ROOT/out/$MODULE_ID/zygisk/x86_64.so
        cp $file $ROOT/out/$MODULE_ID/zygisk/x86_64.so
    else if string match -qr "x86" $file
        echo cp $file $ROOT/out/$MODULE_ID/zygisk/x86.so
        cp $file $ROOT/out/$MODULE_ID/zygisk/x86.so
    end
end

echo_green "=======> zip package"
echo cd $ROOT/out/$MODULE_ID
cd $ROOT/out/$MODULE_ID
echo zip -r ../$MODULE_ID.zip *
zip -r ../$MODULE_ID.zip *

echo_green "=======> push to download dir"
echo cd $ROOT
cd $ROOT
echo adb push ./out/$MODULE_ID.zip /storage/emulated/0/Download/
adb push ./out/$MODULE_ID.zip /storage/emulated/0/Download/

echo_green "=======> install and reboot"
echo adb shell su -c magisk --install-module /storage/emulated/0/Download/$MODULE_ID.zip
adb shell su -c magisk --install-module /storage/emulated/0/Download/$MODULE_ID.zip

read -P "adb reboot? (Y/n): " input
if test "$input" = "n"
    return
else
    adb reboot
end