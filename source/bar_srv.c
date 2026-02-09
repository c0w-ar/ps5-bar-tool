#include "bar_srv.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

int bar_fd = -1;

int get_bar_fd (void) {
    if (bar_fd == -1) {
        bar_fd = open("/dev/bar", O_RDWR, 0);
        if (bar_fd < 0)
        {
            printf("Failed to open '/dev/bar'\n");
        }
    }
    return bar_fd;
}

int close_bar_fd (void) {
    if (bar_fd != -1) {
        return close(bar_fd);
        bar_fd = -1;
    }
    return 0;
}

int bar_context_create (int* context) {
    int ret;
    bar_args args;

    memset(&args, 0, sizeof(args));

    int fd = get_bar_fd();
    ret = ioctl(fd,0xc0684201, &args);
    if (ret == 0) {
        *context = args.context;
    }
    return ret;
}

int bar_context_destroy (int context) {
    bar_args args;

    memset(&args, 0, sizeof(args));
    args.context = context;

    int fd = get_bar_fd();
    return ioctl(fd,0xc0684202, &args);
}

int bar_context_init (int context, int version, int mode, uint8_t key[16], uint8_t iv[12]) {
    int ret;
    bar_args args;

    memset(&args, 0, sizeof(args));
    args.context = context;
    args.version = version;
    args.mode = mode;
    memcpy(&args.key, key, 16);
    memcpy(&args.iv, iv, 12);

    int fd = get_bar_fd();
    ret = ioctl(fd,0xc0684203, &args);
    if (ret == 0) {
        ret = (int) args.status;
    }
    return ret;
}

int bar_update_aad (int context, void* buf, int size) {
    int ret;
    bar_args args;

    memset(&args, 0, sizeof(args));
    args.context = context;
    args.aad_source = buf;
    args.aad_size = size;

    for (int i = 0; i < 16; i++) {
        printf("%02X%c", ((uint8_t*) buf)[i], (i == 15) ? '\n' : ':');
    }

    int fd = get_bar_fd();
    ret = ioctl(fd,0xc0684204, &args);
    if (ret == 0) {
        ret = (int) args.status;
    }
    return ret;
}

int bar_update_encrypt (int context, void* dest, void* source, int size) {
    int ret;
    bar_args args;
    int chuck_size = 0;
    void* tmp_dest = dest;
    void* tmp_source = source;

    memset(&args, 0, sizeof(args));
    args.context = context;

    do {

        if (size > 0x40000) {
            chuck_size = 0x40000;
        }
        else {
            chuck_size = size;
        }

        args.enc_dec_source = tmp_source;
        args.enc_dec_dest = tmp_dest;
        args.enc_dec_size = chuck_size;

        int fd = get_bar_fd();
        ret = ioctl(fd,0xc0684205, &args);
        if (ret == 0) {
            ret = (int) args.status;
        }

        size = size - chuck_size;
        tmp_source = tmp_source + chuck_size;
        tmp_dest = tmp_dest + chuck_size;

    } while (size);
    return ret;
}

int bar_update_decrypt (int context, void* dest, void* source, int size) {
    int ret;
    bar_args args;
    int chuck_size = 0;
    void* tmp_dest = dest;
    void* tmp_source = source;

    memset(&args, 0, sizeof(args));
    args.context = context;

    do {

        if (size > 0x40000) {
            chuck_size = 0x40000;
        }
        else {
            chuck_size = size;
        }

        args.enc_dec_source = tmp_source;
        args.enc_dec_dest = tmp_dest;
        args.enc_dec_size = chuck_size;

        int fd = get_bar_fd();
        ret = ioctl(fd,0xc0684206, &args);
        if (ret == 0) {
            ret = (int) args.status;
        }

        size = size - chuck_size;
        tmp_source = tmp_source + chuck_size;
        tmp_dest = tmp_dest + chuck_size;

    } while (size);

    return ret;
}

int bar_finish_encrypt (int context, uint8_t hash[16]) {
    int ret;
    bar_args args;

    memset(&args, 0, sizeof(args));
    args.context = context;

    int fd = get_bar_fd();
    ret = ioctl(fd,0xc0684207, &args);
    if (ret == 0) {
        memcpy(hash, &args.hash, 16);
    }
    return ret;
}

int bar_finish_decrypt (int context, uint8_t hash[16]) {
    int ret;
    bar_args args;

    memset(&args, 0, sizeof(args));
    args.context = context;
    memcpy(&args.hash, hash, 16);

    int fd = get_bar_fd();
    ret = ioctl(fd,0xc0684208, &args);

    if (ret == 0) {
        ret = (int) args.status;
    }
    return ret;
}