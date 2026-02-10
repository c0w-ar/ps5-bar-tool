#ifndef __BAR_FILE_H__
#define __BAR_FILE_H__

#include <stdint.h>

typedef struct _bar_file_header {
    uint8_t     magic[8];
    uint64_t    unknown;
    int         mode;
    uint32_t    padd_1;
    int         version;
    uint32_t    padd_2;
    uint8_t     key[16];
    uint8_t     iv[12];
    uint32_t    padd_3;
    uint64_t    n_segments;
    uint64_t    data_offset;
    uint64_t    data_size;
} bar_file_header;

typedef struct _bar_file_segment_metadata {
    int         segment_id;
    uint16_t    padd_1;
    uint16_t    part_number;       // always 0 for Normal File and 0, 1 , 2 for Special File
    uint64_t    data_offset;
    uint64_t    compressed_size;
    uint64_t    algorithm_type;     // always 3 for archive.dat
    uint64_t    algorithm_version;  // always 1 for archive.dat
    uint8_t     iv[12];
    uint32_t    padd_2;
    uint64_t    uncompressed_size;
} bar_file_segment_metadata;

typedef struct _bar_file_segment_hash {
    int         segment_id;
    int         segment_type;       // always 0 for archive.dat
    uint8_t     hash[16];
    uint8_t     padd[24];           // all 0

} bar_file_segment_hash;

typedef struct _bar_dir_file {
    /* 0x000 */ uint64_t atime_sec;      // Access timestamp
    /* 0x008 */ uint64_t atime_nsec;
    /* 0x010 */ uint64_t mtime_sec;      // Modified timestamp
    /* 0x018 */ uint64_t mtime_nsec;
    /* 0x020 */ uint64_t ctime_sec;      // Change timestamp
    /* 0x028 */ uint64_t ctime_nsec;
    /* 0x030 */ uint64_t size;
    /* 0x038 */ uint64_t btime_sec;       // Birth timestamp
    /* 0x040 */ uint64_t btime_nsec;
    /* 0x048 */ uint32_t uid;
    /* 0x04C */ uint32_t gid;
    /* 0x050 */ uint16_t mode;
    /* 0x052 */ uint16_t special;          // 0 = Normal - 1 = Special
    /* 0x054 */ char     path[0x400];
    /* 0x454 */ uint32_t flags;            // Internal status flags
} bar_dir_file; // Total Size: 0x458

typedef struct _bar_user {
    int user_id;
    int flag_online;
    char user_name[0x18];
} bar_user;

#endif