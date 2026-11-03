/* Copyright 2026 OpenDroidOS
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "bootstage.h"
#include "init_log.h"

#define EARLY_MODULE_LIMIT 512
#define EARLY_MANIFEST_LIMIT (128 * 1024)

static int ValidModuleName(const char *name, size_t length)
{
    if (length <= 3 || length > NAME_MAX || strcmp(name + length - 3, ".ko") != 0) {
        return 0;
    }
    for (size_t i = 0; i < length - 3; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return 0;
        }
    }
    return 1;
}

static int ReadModuleOrder(int directory, char **names, size_t *count)
{
    int fd = openat(directory, "modules.load.ohos", O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0 && errno == ENOENT) {
        fd = openat(directory, "modules.load", O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    }
    if (fd < 0) {
        return -errno;
    }
    struct stat status;
    if (fstat(fd, &status) != 0) {
        int error = errno;
        close(fd);
        return -error;
    }
    if (!S_ISREG(status.st_mode) || status.st_size <= 0 || status.st_size > EARLY_MANIFEST_LIMIT) {
        close(fd);
        return -EINVAL;
    }
    FILE *stream = fdopen(fd, "r");
    if (stream == NULL) {
        int error = errno;
        close(fd);
        return -error;
    }
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    int result = 0;
    while ((length = getline(&line, &capacity, stream)) >= 0) {
        if (memchr(line, '\0', (size_t)length) != NULL) {
            result = -EINVAL;
            break;
        }
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (length == 0 || line[0] == '#') {
            continue;
        }
        if (*count == EARLY_MODULE_LIMIT || !ValidModuleName(line, (size_t)length)) {
            result = -EINVAL;
            break;
        }
        for (size_t i = 0; i < *count; ++i) {
            if (strcmp(names[i], line) == 0) {
                result = -EEXIST;
                break;
            }
        }
        if (result != 0) {
            break;
        }
        names[*count] = strdup(line);
        if (names[*count] == NULL) {
            result = -ENOMEM;
            break;
        }
        ++*count;
    }
    if (ferror(stream) && result == 0) {
        result = -(errno != 0 ? errno : EIO);
    }
    free(line);
    if (fclose(stream) != 0 && result == 0) {
        result = -errno;
    }
    return result == 0 && *count == 0 ? -EINVAL : result;
}

static int LoadRamdiskModules(const char *path)
{
    int directory = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory < 0) {
        return -errno;
    }
    char **names = calloc(EARLY_MODULE_LIMIT, sizeof(*names));
    if (names == NULL) {
        close(directory);
        return -ENOMEM;
    }
    size_t count = 0;
    int result = ReadModuleOrder(directory, names, &count);
    for (size_t i = 0; result == 0 && i < count; ++i) {
        int fd = openat(directory, names[i], O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            result = -errno;
        } else {
            struct stat status;
            if (fstat(fd, &status) != 0) {
                result = -errno;
            } else if (!S_ISREG(status.st_mode)) {
                result = -EINVAL;
            } else if (syscall(__NR_finit_module, fd, "", 0) != 0) {
                int loadError = errno;
                if (loadError == EEXIST) {
                    /* An earlier init stage may already have loaded this module. */
                    INIT_LOGI("Required early module %s is already loaded", names[i]);
                } else {
                    result = -loadError;
                }
            }
            close(fd);
        }
        if (result != 0) {
            INIT_LOGE("Required early module %s failed: %d", names[i], -result);
        } else {
            INIT_LOGI("Required early module %s is available", names[i]);
        }
    }
    for (size_t i = 0; i < count; ++i) {
        free(names[i]);
    }
    free(names);
    close(directory);
    return result;
}

static int EarlyModulesHook(const HOOK_INFO *info, void *context)
{
    (void)info;
    (void)context;
    int result = LoadRamdiskModules("/lib/modules");
    if (result != 0) {
        INIT_LOGE("Required ramdisk module loading failed: %d", -result);
        /* SystemPrepare does not propagate first-stage hook return values. */
        abort();
    }
    return 0;
}

__attribute__((constructor)) static void RegisterEarlyModules(void)
{
    if (HookMgrAdd(GetBootStageHookMgr(), INIT_FIRST_STAGE, 0, EarlyModulesHook) != 0) {
        INIT_LOGE("Cannot register required early module loader");
        abort();
    }
}
