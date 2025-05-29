# zygisk-hook-android_dlopen_ext
official templete forked from https://github.com/topjohnwu/zygisk-module-sample/

hook android_dlopen_ext in libnativeloader.so for reverse-engineering porpuse

## Build & install 
ndk-build & push to phone(via adb) & magisk --install-module all in [build.fish](https://github.com/DX39061/zygisk-hook-android_dlopen_ext/blob/master/build.fish)([fish](https://fishshell.com/) script)
``` shell
./build.fish
```

## Usage
set target app name in /data/local/tmp/dlsymHook.txt
``` shell
su
echo "com.example.app" > /data/local/tmp/dlsymHook.txt
```
then grep `AndroidDlopenExtHook` in logcat
