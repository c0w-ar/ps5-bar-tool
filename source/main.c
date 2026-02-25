#include "main.h"
#include "bar_file.h"
#include "bar_srv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

char *segment_name[] = {
        "1_tmpBackupInfo.dat",
        "2_tmpBackupDir.dat",
        "3_tmpBackupFile.dat",
        "4_tmpBackupReg.dat",
        "5_tmpBackupRebootData.dat"
};

int main() {

    bar_session session;
    bar_file_header* f_header;
    int ret;
    void* buffer = NULL;
    uint64_t decrypted_size;

    // Read header size 0x58
    read_header(&session);

    // This header is 0x400
    decrypted_size = decrypt_segment(&session, &buffer, 1, 0, 0);

    int bar_file_version = *(int*)(buffer + 0x4);
    int bar_sw_version = *(int*)(buffer + 0x8);
    int bar_total_users = *(int*)(buffer + 0xC);
    int bar_total_dirs = *(int*)(buffer + 0x10);
    int bar_total_files = *(int*)(buffer + 0x14);
    //int bar_number_users = *(int*)(segment_1_header + 0xC);
    uint64_t bar_required_hdd_size = *(uint64_t*)(buffer + 0x28);
    int bar_target = *(int*)(buffer + 0x30);
    int bar_hw_number = *(int*)(buffer + 0x78);
    char* bar_console_name = malloc(0x40);
    memcpy(bar_console_name, buffer + 0x34, 0x40);
    char* bar_description = malloc(0x78);
    memcpy(bar_description, buffer + 0x100, 0x78);
    bar_user* user_list = buffer + 0x200;

    log_printf("\n\n\n");
    log_printf("|-- Backup and Restore File Information --|\n\n");
    log_printf("File version: %d\n", bar_file_version);
    log_printf("Software Version: ");
        for (int i = 3; i >= 0; i--) {
        log_printf("%02x%c", ((uint8_t*)&bar_sw_version)[i], (i == 0) ? '\n' : '.');
    }
    log_printf("Required HDD Size: %lu bytes\n", bar_required_hdd_size);
    log_printf("Target: %d\n", bar_target);
    log_printf("Hardware Number: %d\n", bar_hw_number);
    log_printf("Console Name: %s\n", bar_console_name);
    log_printf("File Description: %s\n", bar_description);
    log_printf("Total files in Backup: %d\n", bar_total_files);
    log_printf("Total folders in Backup: %d\n", bar_total_dirs);
    log_printf("\n\n");
    log_printf("*** Users in BAR file ***\n\n");
    for(int i=0; i<bar_total_users; i++) {
        log_printf("ID: %08X  Online: %s  Name: %s\n", user_list->user_id, user_list->flag_online==1?"Yes":"No ", user_list->user_name);
        user_list++;
    }
    log_printf("\n\n\n");

    free(bar_console_name);
    free(bar_description);

    // Dump Main Segments to File
#if DUMP_MAIN_SEGMENTS == 1
    for (int i=1; i<=5; i++) {
        char* dump_path;
        dump_path = malloc(0x100);
        sprintf(dump_path, MAIN_DIR "%s", segment_name[i-1]);
        log_printf("Dumping segment %d to path: %s\n", i, dump_path);
        int fd_dump = open(dump_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd_dump == -1) {
            log_printf("Error opening file for write\n");
        }
        decrypted_size = decrypt_segment(&session, &buffer, i, 0, 0);
        log_printf("Decrypted Size: %lu\n", decrypted_size);
        log_printf("\n\n");
        write(fd_dump, buffer, decrypted_size);
        close(fd_dump);
        free(dump_path);
    }
#endif

    print_files_message();

#if DUMP_FILES == 1 || DUMP_SAVES == 1
    char *main_dirs[] = {
        DUMP_DIR,
        DUMP_DIR "/system_data",
        DUMP_DIR "/user"
    };

    for (int i=0; i<3; i++) {
        struct stat st;
        // Create if not exists
        if (stat(main_dirs[i], &st) == -1)
            mkdir(main_dirs[i], 0777);
    }

    // Let's create directory structure from Segment 2
    bar_dir_file* dir_list = NULL;
    struct stat st;
    char* dir_path = malloc(0x500);;

    decrypt_segment(&session, (void**) &dir_list, 2, 0, 0);
    for (int i=0; i<bar_total_dirs; i++) {
        if (should_dump(dir_list[i].path)) {
            sprintf(dir_path, DUMP_DIR "%s", dir_list[i].path);
            // Create if not exists
            if (stat(dir_path, &st) == -1)
                if(mkdir(dir_path, 0777) == -1)
                    log_printf("Could not create dir: %s\n", dir_path);
        }
    }
#endif

    bar_dir_file* file_list = NULL;
    // Get File List from Segment 3
    decrypt_segment(&session, (void**) &file_list, 3, 0, 0);

    for (int i=0; i<bar_total_files; i++) {
        if (should_print_file(file_list[i].path)) {
            log_printf("Index: %04d Segment: %04x : %s\n", i, i+0x2710, file_list[i].path);
            if (should_dump(file_list[i].path)) {
                char* dump_path;
                dump_path = malloc(0x500);
                sprintf(dump_path, DUMP_DIR "%s", file_list[i].path);
                int fd_dump = open(dump_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if (fd_dump == -1) {
                    log_printf("\nError opening file for write: %s\n", dump_path);
                    continue;
                }

                // Embedded Files start at index 0x2710
                decrypted_size = decrypt_segment(&session, &buffer, i+0x2710, (int) file_list[i].special, 0);
                if (decrypted_size>0)
                    write(fd_dump, buffer, decrypted_size);
                close(fd_dump);
                free(dump_path);
            }
            log_printf("\n");
        }
    }

    log_printf("\nps5-bar-tool finished!!\n");
    log_printf("\n\n\n");

cleanup:
    // Free Pointers
    if(file_list) free(file_list);
    close_bar_fd();
    if(session.segment_metadata) free(session.segment_metadata);
    if(session.segment_hash) free(session.segment_hash);
    if (buffer) free(buffer);
}

int read_header(bar_session* session) {
    //"/archive%04lu.dat"
    char* file = MAIN_DIR "archive.dat";
    int file_fd = -1;
    bar_file_header* f_header = NULL;
    int ret;

    file_fd = open(file, O_RDONLY);
    if (file_fd == -1) {
        log_printf("File %s not found", file);
        ret = -1;
        goto cleanup;
    }

    f_header = malloc(sizeof(bar_file_header));

    read(file_fd, f_header, sizeof(bar_file_header));

    if (*(uint64_t*) &f_header->magic != 0x464143454953) {
        log_printf("Header Magic is wrong in file: %s\n", file);
        ret = -1;
        goto cleanup;
    }

    log_printf("Information in header:\n");
    log_printf("Magic: %s\n", f_header->magic);
    log_printf("Mode: %d\n", f_header->mode);
    log_printf("Version: %d\n", f_header->version);
    log_printf("Key: ");
    for (int i = 0; i < 16; i++) {
        log_printf("%02X", f_header->key[i]);
    }
    log_printf("\n");
    log_printf("IV: ");
    for (int i = 0; i < 12; i++) {
        log_printf("%02X", f_header->iv[i]);
    }
    log_printf("Segments: 0x%x\n", (int) f_header->n_segments);
    log_printf("Data Offset: 0x%p\n", (void*) f_header->data_offset);
    log_printf("Data Size: 0x%x\n", (int) f_header->data_size);

    session->mode = f_header->mode;
    session->version = f_header->version;
    session->n_segments = f_header->n_segments;
    memcpy(session->key, f_header->key, 16);
    memcpy(session->iv, f_header->iv, 12);

    // Read Segments Metadata Table (N*0x40)
    session->segment_metadata = malloc(sizeof(bar_file_segment_metadata) * session->n_segments);
    read(file_fd, session->segment_metadata, sizeof(bar_file_segment_metadata) * session->n_segments);

    // Read Segments Hash Table (N*0x30)
    session->segment_hash = malloc(sizeof(bar_file_segment_hash) * session->n_segments);
    read(file_fd, session->segment_hash, sizeof(bar_file_segment_hash) * session->n_segments);

    // Read Complete Header Hash (0x10 but PS5 reads 0x20)
    read(file_fd, session->complete_hash, 0x20);

    // Create context
    ret = bar_context_create(&session->context);
    if (ret!=0) {
        log_printf("Something went wrong while creating context\n");
        goto cleanup;
    }
    // Init context
    ret = bar_context_init (session->context, session->version, session->mode, session->key, session->iv);
    if (ret!=0) {
        log_printf("Something went wrong while init context\n");
        goto cleanup;
    }
    // Update Additional Authentication Data
    ret = bar_update_aad (session->context, f_header, sizeof(bar_file_header));
    if (ret!=0) {
        log_printf("Something went wrong while updating AAD\n");
        goto cleanup;
    }
    ret = bar_update_aad (session->context, session->segment_metadata, sizeof(bar_file_segment_metadata) * session->n_segments);
    if (ret!=0) {
        log_printf("Something went wrong while updating AAD\n");
        goto cleanup;
    }
    ret = bar_update_aad (session->context, session->segment_hash, sizeof(bar_file_segment_hash) * session->n_segments);
    if (ret!=0) {
        log_printf("Something went wrong while updating AAD\n");
        goto cleanup;
    }
    // Check if file Hash is valid
    ret = bar_finish_decrypt (session->context, session->complete_hash);
    if (ret!=0) {
        log_printf("Something went wrong while checking BAR File Hash\n");
        goto cleanup;
    }
    // Destroy context
    ret = bar_context_destroy(session->context);
    if (ret!=0) {
        log_printf("Something went wrong while destroying context\n");
    }
    session->context = 0;

cleanup:
    if (file_fd != -1) close(file_fd);
    if (f_header) free(f_header);
    if (ret == -1) {
        if (session->context) {
            bar_context_destroy(session->context);
            session->context = 0;
        }
        if(session->segment_metadata) free(session->segment_metadata);
        if(session->segment_hash) free(session->segment_hash);
    }
    return ret;
}

// buffer pointer should not be allocated
uint64_t decrypt_segment(bar_session* session, void** buffer, int segment_id, int special_file, int flag_validate) {
    int meta_index = -1;
    int hash_index = -1;
    bar_file_segment_metadata* metadata;
    bar_file_segment_hash* hash;
    void* encrypted_data;

    if(special_file) {
        log_printf("Skipping special file\n");
        return -1;
    }

    for (int i=0; i<=session->n_segments; i++) {
        metadata = &session->segment_metadata[i];   
        if (metadata->segment_id == segment_id) {
            meta_index = i;
            break;
        }
    }

    if (flag_validate) {
        for (int i=0; i<=session->n_segments; i++) {
            hash = &session->segment_hash[i];
            if (hash->segment_id == segment_id) {
                hash_index = i;
                break;
            }
        }
    }

    if (meta_index == -1 || (hash_index == -1 && flag_validate)) {
        log_printf("Could not find segment with ID: %d\n", segment_id);
        return -1;
    }

    // Exclude files larger than 2GB
    if(metadata->uncompressed_size > 0x80000000) {
        log_printf("Skipping large file\n");
        return -1;
    }

    bar_context_create(&(session->context));
    bar_context_init(session->context, session->version, session->mode, session->key, metadata->iv);

    encrypted_data = malloc(metadata->compressed_size);

    // Reallocate to target size
    if(*buffer) free(*buffer);
    *buffer = malloc(metadata->uncompressed_size);

    read_from_archive(encrypted_data, metadata->compressed_size, metadata->data_offset);

    bar_update_decrypt(session->context, *buffer, encrypted_data, metadata->uncompressed_size);

    bar_context_destroy(session->context);
    
    free(encrypted_data);

    return metadata->uncompressed_size;

}

// buffer pointer should not be allocated
uint64_t decrypt_segment_part(bar_session* session, void** buffer, int segment_id, uint16_t part_number, int flag_validate) {
    int meta_index = -1;
    int hash_index = -1;
    bar_file_segment_metadata* metadata;
    bar_file_segment_hash* hash;
    void* encrypted_data;

    for (int i=0; i<=session->n_segments; i++) {
        metadata = &session->segment_metadata[i];   
        if (metadata->segment_id == segment_id && metadata->part_number == part_number) {
            meta_index = i;
            break;
        }
    }

    if (flag_validate) {
        for (int i=0; i<=session->n_segments; i++) {
            hash = &session->segment_hash[i];
            if (hash->segment_id == segment_id) {
                hash_index = i;
                break;
            }
        }
    }

    if (meta_index == -1 || (hash_index == -1 && flag_validate)) {
        log_printf("Could not find segment with ID: %d\n", segment_id);
        return -1;
    }

    bar_context_create(&(session->context));
    bar_context_init(session->context, session->version, session->mode, session->key, metadata->iv);

    encrypted_data = malloc(metadata->compressed_size);

    // Reallocate to target size
    if(*buffer) free(*buffer);
    *buffer = malloc(metadata->uncompressed_size);

    read_from_archive(encrypted_data, metadata->compressed_size, metadata->data_offset);

    bar_update_decrypt(session->context, *buffer, encrypted_data, metadata->uncompressed_size);

    bar_context_destroy(session->context);
    
    free(encrypted_data);
    
    return metadata->uncompressed_size;

}

int read_from_archive (void* buffer, uint64_t size, uint64_t offset) {
    
    uint64_t archive_number = 0;
    uint64_t offset_tmp = 0;
    int ret = 0;

    archive_number = offset / 0xFFFF0000;               // Each file has 4GB
    offset_tmp = offset - archive_number * 0xFFFF0000;  // Offset on target file

    // All in 1 file
    if (size + offset % 0xFFFF0000 < 0xFFFF0001) {
        return read_from_archive_number(archive_number, buffer, size, offset_tmp);
    }
    // Splitted on 2 files
    else {
        int size_tmp = 0xFFFF0000 - offset % 0xFFFF0000;
        ret = read_from_archive_number(archive_number, buffer, size_tmp, offset_tmp);
        if (ret == -1) {
            log_printf("Could not read from splitted file %d", archive_number);
            return -1;
        }
        // We start at offset 0 on the following file
        return read_from_archive_number(archive_number + 1, buffer + size_tmp, size - size_tmp, 0);
    }
    
}

int read_from_archive_number (uint64_t archive_number, void* buffer, uint64_t size, uint64_t offset) {
    int file_fd;
    char file[0x40];
    int ret=0;

    if (archive_number == 0) {
        sprintf(file, MAIN_DIR "archive.dat");
    }
    else {
        sprintf(file, MAIN_DIR "archive%04lu.dat", archive_number);
    }

    file_fd = open(file, O_RDONLY);
    if (file_fd == -1) {
        log_printf("File %s not found\n", file);
        return -1;
    }
    else {
        ret = pread(file_fd, buffer, size, offset);
        if (ret == -1) {
            log_printf("Couldn't read from file %s size %lu offset %lu\n", file, size, offset);
        }
        close(file_fd);
        return ret;
    }
}

void log_printf(const char *format, ...) {
    va_list args;

    // 1. Output to standard console
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // 2. Output to log file
    FILE *log_file = fopen(MAIN_DIR "ps5-bar-tool.log", "a");
    if (log_file != NULL) {
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fclose(log_file);
    }
}

void print_files_message(void) {
#if DUMP_FILES == 1
    log_printf("*** Listing and Dumping Embedded Files in BAR File - This may take several minutes ***\n");
#elif DUMP_SAVES == 1
    log_printf("*** Listing and Dumping Embedded Save Files in BAR File***\n");
#else
    log_printf("*** Listing Embedded Files in BAR File ***\n");
#endif
    log_printf("\n");
}

int is_savedata_path(char* path) {
    if (strstr(path, "savedata"))
        return 1;
    if (strcmp(path, "/user/home")==0) // "/user/home"
        return 1;
    if (strstr(path, "/user/home")!=0 && strlen(path)==19) // "/user/home/AABBCCDD"
        return 1;
    return 0;
}

int should_dump(char* path) {
#if DUMP_FILES == 1
    return 1;
#elif DUMP_SAVES == 1
    if (is_savedata_path(path))
        return 1;
    return 0;
#else
    return 0;
#endif
}

int should_print_file(char* path) {
#if DUMP_FILES == 1
    return 1;
#elif DUMP_SAVES == 1
    if (is_savedata_path(path))
        return 1;
    return 0;
#else
    return 1;
#endif
}