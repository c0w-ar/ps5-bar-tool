#ifndef __BAR_SRV_H__
#define __BAR_SRV_H__

#include <stdint.h>

int get_bar_fd(void);
int close_bar_fd (void);

typedef struct _bar_args
{
    int         context;
    int         version;        // 5
    int         mode;           // 1
    uint8_t     key[16];
    uint8_t     iv[12];
    uint8_t     hash[16];       // used by finish encrypt / decrypt
    void*       aad_source;     // used as source by add
    int         aad_size;
    uint8_t     padd_2[4];
    void*       enc_dec_source; // source for encrypt / decrypt data
    int         enc_dec_size;
    uint8_t     padd_3[4];
    void*       enc_dec_dest;   // destination for encrypt / decrypt data
    uint64_t    status;
} bar_args;

typedef struct _bar_update_aad_args
{
    int         context;
    uint8_t     padd_1[52];
    void *      source;
    int         size;
    uint8_t     padd_2[36];
    uint64_t    status;
} bar_update_aad_args;

int bar_context_create (int* context);

int bar_context_destroy (int context);

int bar_context_init (int context, int version, int mode, uint8_t key[16], uint8_t iv[12]);

int bar_update_aad (int context, void* buf, int size);

int bar_update_encrypt (int context, void* dest, void* source, int size);

int bar_update_decrypt (int context, void* dest, void* source, int size);

int bar_finish_encrypt (int context, uint8_t auth_tag[16]);

int bar_finish_decrypt (int context, uint8_t auth_tag[16]);

#endif