#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>
#include <android/dlext.h>
#include <sys/stat.h>
#include <cstring>
#include <link.h>
#include <dlfcn.h>

#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "AndroidDlopenExtHook", __VA_ARGS__)
static char target_process_name[100];

struct DevIno {
    dev_t dev;
    ino_t ino;
};

int get_dev_ino(const char* lib, DevIno* out_result) {
    if (!lib || !out_result) return -1;

    struct State {
        const char* needle{};
        DevIno* result{};
        int found = 0;
    } state = {lib, out_result, 0};

    dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) -> int {
        auto* state = static_cast<State*>(data);
        if (state->found) return 1;

        if (info->dlpi_name && strstr(info->dlpi_name, state->needle)) {
            struct stat st{};
            if (stat(info->dlpi_name, &st) == 0) {
                state->result->dev = st.st_dev;
                state->result->ino = st.st_ino;
                state->found = 1;
                return 1;
            }
        }
        return 0;
    }, &state);

    return state.found;
}

void read_target_file() {
    int fd = open("/data/local/tmp/dlsymHook.txt", O_RDONLY);
    int bytes_read = read(fd, target_process_name, 100);
    if (bytes_read <= 0) {
        LOGD("bytes_read: %d", bytes_read);
        return;
    }

    // trim tailing space
    char *p_t = target_process_name + strlen(target_process_name) - 1;
    while (target_process_name < p_t) {
        if (*(p_t) == ' ' || *(p_t) == '\r' || *(p_t) == '\n' || *(p_t) == '\t') {
            *(p_t) = 0;
            p_t --;
        } else {
            break;
        }
    }
}

void* _Nullable (*android_dlopen_ext_old)(const char* _Nullable, int, const android_dlextinfo* _Nullable) = nullptr;
static void* _Nullable android_dlopen_ext_new(const char* _Nullable _filename, int _flags, const android_dlextinfo* _Nullable _info) {
    void *handle = android_dlopen_ext_old(_filename, _flags, _info);
    LOGD("android_dlopen_ext_new(filename=%s, _flags=%d, _info=%p)", _filename, _flags, _info);

    void *JNI_OnLoad_address = dlsym(handle, "JNI_OnLoad");
    if (JNI_OnLoad_address) {
        Dl_info dl_info;
        if (dladdr(JNI_OnLoad_address, &dl_info)) {
            LOGD("filename=%s, JNI_OnLoad=%p, base_address=%p",
                 dl_info.dli_fname,
                 JNI_OnLoad_address,
                 dl_info.dli_fbase);
        }
    }

    return handle;
}

class AndroidDlopenExtHook : public zygisk::ModuleBase {
public:
    void onLoad(Api *_api, JNIEnv *_env) override {
        this->api = _api;
        this->env = _env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        // Use JNI to fetch our process name
        const char *processName = this->env->GetStringUTFChars(args->nice_name, nullptr);

        int companionFd = this->api->connectCompanion();
        int bytes_read = read(companionFd, target_process_name, 100);
        if (bytes_read <= 0 || strcmp(processName, target_process_name) != 0) {
//            LOGD("%s, %s", processName, target_process_name);
            this->api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        LOGD("start hook %s, %d", processName, getpid());
        auto devIno = DevIno{};
        if(!get_dev_ino("libnativeloader.so", &devIno)) {
            LOGD("error: cannot find libnativeloader.so");
            this->api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        LOGD("libnativeloader.so dev:%u, ino: %lu", devIno.dev, devIno.ino);
        this->api->pltHookRegister(
                devIno.dev,
                devIno.ino,
                "android_dlopen_ext",
                (void *) android_dlopen_ext_new,
                (void **)&android_dlopen_ext_old
        );

        if (this->api->pltHookCommit()) {
            LOGD("plt hook commit success");
        } else {
            LOGD("error: plt hook commit");
        }
    }

private:
    Api *api{};
    JNIEnv *env{};
};

static void companion_handler(int mainFd) {
    read_target_file();
    int bytes_write = write(mainFd, target_process_name, 100);
    if (bytes_write <= 0) {
        LOGD("error write target process name");
        return;
    }
}

// Register our module class and the companion handler function
REGISTER_ZYGISK_MODULE(AndroidDlopenExtHook)
REGISTER_ZYGISK_COMPANION(companion_handler)
