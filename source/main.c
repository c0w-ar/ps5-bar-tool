#include "bar_file.h"
#include "bar_srv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAIN_DIR "/mnt/usb0/PS5/EXPORT/BACKUP/"
#define DUMP_DIR MAIN_DIR "DUMP"
#define DUMP_MAIN_SEGMENTS 0
#define DUMP_FILES 1

char *segment_name[] = {
        "1_tmpBackupInfo.dat",
        "2_tmpBackupDir.dat",
        "3_tmpBackupFile.dat",
        "4_tmpBackupReg.dat",
        "5_tmpBackupRebootData.dat"
};

typedef struct _bar_session {
    int         context;
    int         mode;
    int         version;
    uint8_t     key[16];
    uint8_t     iv[12];
    uint64_t    n_segments;
    bar_file_segment_metadata* segment_metadata;
    bar_file_segment_hash* segment_hash;
    uint8_t     complete_hash[16];
} bar_session;

// Return pointer to decrypted data
int decrypt_segment(char* file_name, bar_session* session, void* buffer, int segment_id);

int main() {

    bar_session session;
    bar_file_header* f_header;
    int ret;
    char* file = MAIN_DIR "archive.dat";
    void* buffer = NULL;
    int decrypted_size;

    int file_fd = open(file, O_RDONLY);

    if(file_fd == -1) {
        printf("File %s not found", file);
        return -1;
    }

    f_header = malloc(sizeof(bar_file_header));
    read(file_fd, f_header, sizeof(bar_file_header));

    if(*(uint64_t*) &f_header->magic != 0x464143454953) {
        printf("Header Magic is wrong in file: %s\n", file);
        free(f_header);
        close(file_fd);
        return -1;
    }

    printf("Information in header:\n");
    printf("Magic: %s\n", f_header->magic);
    printf("Mode: %d\n", f_header->mode);
    printf("Version: %d\n", f_header->version);
    printf("Key: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X%c", f_header->key[i], (i == 15) ? '\n' : ':');
    }
    printf("IV: ");
    for (int i = 0; i < 12; i++) {
        printf("%02X%c", f_header->iv[i], (i == 11) ? '\n' : ':');
    }
    printf("Segments: 0x%x\n", (int) f_header->n_segments);
    printf("Data Offset: 0x%p\n", (void*) f_header->data_offset);
    printf("Data Size: 0x%x\n", (int) f_header->data_size);

    session.mode = f_header->mode;
    session.version = f_header->version;
    session.n_segments = f_header->n_segments;
    memcpy(session.key, f_header->key, 16);
    memcpy(session.iv, f_header->iv, 12);

    // Read Segments Metadata Table (N*0x40)
    session.segment_metadata = malloc(sizeof(bar_file_segment_metadata) * session.n_segments);
    read(file_fd, session.segment_metadata, sizeof(bar_file_segment_metadata) * session.n_segments);
    printf("First Metadata: ");
    for (int i = 0; i < 0x40; i++) {
        printf("%02X%c", ((uint8_t*)session.segment_metadata)[i], (i == 0x3F) ? '\n' : ':');
    }

    // Read Segments Hash Table (N*0x30)
    session.segment_hash = malloc(sizeof(bar_file_segment_hash) * session.n_segments);
    read(file_fd, session.segment_hash, sizeof(bar_file_segment_hash) * session.n_segments);
    printf("First Hash: ");
    for (int i = 0; i < 0x30; i++) {
        printf("%02X%c", ((uint8_t*)session.segment_hash)[i], (i == 0x2F) ? '\n' : ':');
    }

    // Read Complete Header Hash (0x10 but PS5 reads 0x20)
    read(file_fd, session.complete_hash, 0x20);
    printf("First Complete_Hash: ");
    for (int i = 0; i < 0x20; i++) {
        printf("%02X%c", session.complete_hash[i], (i == 0x1F) ? '\n' : ':');
    }

    // Create context
    ret = bar_context_create(&session.context);
    if (ret!=0) {
        printf("Something went wrong while creating context\n");
        goto cleanup;
    }
    // Init context
    ret = bar_context_init (session.context, session.version, session.mode, session.key, session.iv);
    if (ret!=0) {
        printf("Something went wrong while init context\n");
        goto cleanup;
    }
    // Update Additional Authentication Data
    ret = bar_update_aad (session.context, f_header, sizeof(bar_file_header));
    if (ret!=0) {
        printf("Something went wrong while updating AAD\n");
        goto cleanup;
    }
    ret = bar_update_aad (session.context, session.segment_metadata, sizeof(bar_file_segment_metadata) * session.n_segments);
    if (ret!=0) {
        printf("Something went wrong while updating AAD\n");
        goto cleanup;
    }
    ret = bar_update_aad (session.context, session.segment_hash, sizeof(bar_file_segment_hash) * session.n_segments);
    if (ret!=0) {
        printf("Something went wrong while updating AAD\n");
        goto cleanup;
    }
    // Check if file Hash is valid
    ret = bar_finish_decrypt (session.context, session.complete_hash);
    if (ret!=0) {
        printf("Something went wrong while checking BAR File Hash\n");
        goto cleanup;
    }
    // Destroy context
    ret = bar_context_destroy(session.context);
    if (ret!=0) {
        printf("Something went wrong while destroying context\n");
        goto cleanup;
    }


    buffer = malloc(0x1000000);
    // This header is 0x400
    decrypted_size = decrypt_segment(file, &session, buffer, 1);

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

    printf("\n\n\n");
    printf("|-- Backup and Restore File Information --|\n\n");
    printf("File version: %d\n", bar_file_version);
    printf("Software Version: ");
        for (int i = 3; i >= 0; i--) {
        printf("%02x%c", ((uint8_t*)&bar_sw_version)[i], (i == 0) ? '\n' : '.');
    }
    printf("Required HDD Size: %lu bytes\n", bar_required_hdd_size);
    printf("Target: %d\n", bar_target);
    printf("Hardware Number: %d\n", bar_hw_number);
    printf("Console Name: %s\n", bar_console_name);
    printf("File Description: %s\n", bar_description);
    printf("Total files in Backup: %d", bar_total_files);
    printf("\n\n");
    printf("*** Users in BAR file ***\n\n");
    for(int i=0; i<bar_total_users; i++) {
        printf("ID: %08X  Online: %s  Name: %s\n", user_list->user_id, user_list->flag_online==1?"Yes":"No ", user_list->user_name);
        user_list++;
    }
    printf("\n\n\n");


    free(bar_console_name);
    free(bar_description);


    // Dump Main Segments to File
#if DUMP_MAIN_SEGMENTS == 1
    for (int i=1; i<=5; i++) {
        char* dump_path;
        dump_path = malloc(0x100);
        sprintf(dump_path, "/mnt/usb0/PS5/EXPORT/BACKUP/%s", segment_name[i-1]);
        printf("Dumping segment %d to path: %s\n", i, dump_path);
        int fd_dump = open(dump_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd_dump == -1) {
            printf("Error opening file for write\n");
        }
        decrypted_size = decrypt_segment(file, &session, buffer, i);
        printf("Decrypted Size: %d\n", decrypted_size);
        write(fd_dump, buffer, decrypted_size);
        close(fd_dump);
        free(dump_path);
    }
#endif

#if DUMP_FILES == 1
    printf("*** Listing and Dumping Embedded Files in BAR File - This may take several minutes ***\n");
    mkdir(DUMP_DIR, 0777);
    mkdir(DUMP_DIR "/system_data", 0777);
    mkdir(DUMP_DIR "/user", 0777);
    // Let's create directory structure from Segment 2
    bar_dir_file* dir_list = malloc(bar_total_dirs * sizeof(bar_dir_file));
    decrypt_segment(file, &session, dir_list, 2);
    for (int i=0; i<bar_total_dirs; i++) {
        char* dir_path;
        dir_path = malloc(0x500);
        sprintf(dir_path, DUMP_DIR "%s", dir_list[i].path);
        int ret = mkdir(dir_path, 0777);
        if (ret!=0) 
            printf("Could not create dir: %s\n", dir_path);
    }

#else
    printf("*** Listing Embedded Files in BAR File ***\n");
#endif

    bar_dir_file* file_list = malloc(bar_total_files * sizeof(bar_dir_file));
    // Get File List from Segment 3
    decrypt_segment(file, &session, file_list, 3);
    for (int i=0; i<bar_total_files; i++) {
        printf("%04X : %s", i+0x2710, file_list[i].path);
#if DUMP_FILES == 1
        char* dump_path;
        dump_path = malloc(0x500);
        sprintf(dump_path, DUMP_DIR "%s", file_list[i].path);
        int fd_dump = open(dump_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd_dump == -1) {
            printf("\nError opening file for write: %s\n", dump_path);
            continue;
        }
        // Embedded Files start at index 0x2710
        decrypted_size = decrypt_segment(file, &session, buffer, i+0x2710);
        write(fd_dump, buffer, decrypted_size);
        close(fd_dump);
        free(dump_path);
#endif
        printf("\n");
    }
    printf("\n\n\n");

cleanup:
    // Free Pointers
    if(file_list) free(file_list);
    close(file_fd);
    close_bar_fd();
    if(f_header) free(f_header);
    if(session.segment_metadata) free(session.segment_metadata);
    if(session.segment_hash) free(session.segment_hash);
    if (buffer) free(buffer);
}

int decrypt_segment(char* file_name, bar_session* session, void* buffer, int segment_id) {
    //int meta_index = -1;
    //int hash_index = -1;
    bar_file_segment_metadata* metadata;
    //bar_file_segment_hash* hash;
    void* encrypted_data;
    int file_fd;

    for (int i=0; i<=session->n_segments; i++) {
        metadata = &session->segment_metadata[i];   
        if (metadata->segment_id == segment_id) {
            //meta_index = i;
            break;
        }
    }

    // TODO: Implement logic to validate decryption if requested (flag)
/*
    for (int i=0; i<=session->n_segments; i++) {
        hash = &session->segment_hash[i];
        if (hash->segment_id == segment_id) {
            hash_index = i;
            break;
        }
    }

    if (meta_index == -1 || hash_index == -1) {
        printf("Could not find segment with ID: %d\n", segment_id);
        return -1;
    }
*/
    //printf("The segment %d has size: %lx and start at offset: %p\n", segment_id, metadata->uncompressed_size, (void*) metadata->data_offset);

    encrypted_data = malloc(metadata->compressed_size);

    file_fd = open(file_name, O_RDONLY);

    if(file_fd == -1) {
        printf("File %s not found", file_name);
        return -1;
    }

    pread(file_fd, encrypted_data, metadata->compressed_size, metadata->data_offset);

    bar_context_create(&session->context);
    bar_context_init(session->context, session->version, session->mode, session->key, metadata->iv);
    bar_update_decrypt(session->context, buffer, encrypted_data, metadata->uncompressed_size);
    bar_context_destroy(session->context);
    
    close(file_fd);
    free(encrypted_data);
    
    return (int) metadata->uncompressed_size;

}