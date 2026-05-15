#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
typedef struct
{
    int magic_number;
    int disk_size;
    int block_size;
    int total_blocks;
    int bitmap_start;
    int bitmap_blocks;
    int inode_start;
    int inode_blocks;
    int data_start;
} superblock_t;

typedef struct
{
    char filename[32];
    int size;
    int start_block;
    int block_count;
    int used;
} inode_t;

// Functions for disk interface
FILE *disk_create(char *diskfile);
int disk_open(char *disk_file);
int disk_close(FILE *disk_file_pointer);
int disk_read_block(FILE *disk_file, int block_number, int block_size, char *buffer);
int disk_write_block(FILE *disk_file, int block_number, int block_size, char *buffer);

// Helper functions 
int initialize_superblock(superblock_t *sb, int disk_size, int block_size, int total_blocks);
int initialize_bitmap(int *bitmap, int bitmap_blocks);
int initialize_inode_table(inode_t *inode_table, int inode_blocks);

// Functions for parsing workload file and executing commands
int parse_workload_file(char *workload_file, superblock_t *sb, int block_size, int total_blocks, char *disk_file);
int execute_command(char *command, superblock_t *sb, int block_size, int total_blocks, char *disk_file);

int main(int argc, char *argv[])
{
    int option;
    int disk_size;
    int block_size;
    int total_blocks;
    char *workload_file;
    char *disk_file;
    int *buffer;

    // Define long options
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"diskFile", required_argument, 0, 'f'},
        {"diskSize", required_argument, 0, 'd'},
        {"blockSize", required_argument, 0, 'b'},
        {"totalBlocks", required_argument, 0, 't'},
        {"workloadFile", required_argument, 0, 'w'},
        {0, 0, 0, 0}};

    while ((option = getopt_long(argc, argv, "hvf:d:b:t:w:", long_options, NULL)) != -1)
    {
        switch (option)
        {
        case 'h':
            printf("Usage: %s [--help] [--version] [-f <disk_file> -d <disk_size> -b "
                   "<block_size> -t <total_blocks> -w <workload file>]\n",
                   argv[0]);
            break;
        case 'v':
            printf("Version 0.6.7\n");
            break;
        case 'f':
            disk_file = optarg;
            break;
        case 'd':
            disk_size = atoi(optarg);
            break;
        case 'b':
            block_size = atoi(optarg);
            break;
        case 't':
            total_blocks = atoi(optarg);
            break;
        case 'w':
            workload_file = optarg;
            break;
        case '?':
            // getopt_long already printed an error message
            break;
        default:
            abort();
        }
    }

    #ifdef DEBUG
        printf("Debug Mode Enabled\n");
        printf("Parsed Arguments:\n");
        printf("Disk File: %s\n", disk_file);
        printf("Disk Size: %d\n", disk_size);
        printf("Block Size: %d\n", block_size);
        printf("Total Blocks: %d\n", total_blocks);
        printf("Workload File: %s\n", workload_file);
    #endif

    // Validate command-line arguments
    // Assumptions:
    // - Disk_file must not be NULL
    // - Disk_size, block_size, and total_blocks must be positive integers
    // - Workload_file must not be NULL
    // - Disk_size should be at least block_size * total_blocks
    // - Block_size should be at least 9 integers to store superblock information 
    // - total_blocks should be large enough to accommodate superblock, bitmap, and inode table (at least 21 blocks)
    
    

    if (disk_file == NULL || disk_size <= 0 || block_size <= 0 || total_blocks <= 0 || workload_file == NULL)
    {
        fprintf(stderr, "Invalid arguments\n");
        fprintf(stderr, "Usage: %s [--help] [--version] [-f <disk_file> -d <disk_size> -b <block_size> -t <total_blocks> -w <workload file>]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (disk_size < block_size * total_blocks)
    {
        fprintf(stderr, "Invalid arguments: disk_size must be at least block_size * total_blocks\n");
        return EXIT_FAILURE;
    }

    if(block_size < sizeof(int) * 9){
        fprintf(stderr, "Invalid arguments: block_size must be at least %lu bytes to store superblock information\n", sizeof(int) * 9);
        return EXIT_FAILURE;
    }
    
    if (total_blocks < 21)
    {
        fprintf(stderr, "Invalid arguments: total_blocks must be at least 21 to accommodate superblock, bitmap, and inode table\n");
        return EXIT_FAILURE;
    }

    // Create the buffer to store temporary data read from the disk
    buffer = (int *)malloc(block_size);
    if (buffer == NULL)
    {
        fprintf(stderr, "Error allocating memory for buffer\n");
        #ifdef DEBUG
            fprintf(stderr, "Failed to allocate buffer of size %d bytes\n", block_size);
        #endif
        return EXIT_FAILURE;
    }
    #ifdef DEBUG
        printf("Buffer allocated successfully with size %d bytes\n", block_size);
    #endif

    // Open the workload file and parse commands








    free(buffer);
    return 0;
}

// Functions for disk interface
FILE *disk_create(char *diskfile){
    FILE *fp = fopen(diskfile, "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "Error creating disk file: %s\n", diskfile);
        return NULL;
    }
    return fp;
}
int disk_open(char *disk_file){
    FILE *fp = fopen(disk_file, "rb+");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening disk file: %s\n", disk_file);
        return -1;
    }
    return 0;
}
int disk_close(FILE *disk_file_pointer){
    fclose(disk_file_pointer);
    return 0;
}
int disk_read_block(FILE *disk_file, int block_number, int block_size, char *buffer)
{
    fseek(disk_file, block_number * block_size, SEEK_SET);
    fread(buffer, block_size, 1, disk_file);
    return 0;
}
int disk_write_block(FILE *disk_file, int block_number, int block_size, char *buffer)
{
    fseek(disk_file, block_number * block_size, SEEK_SET);
    fwrite(buffer, block_size, 1, disk_file);
    return 0;
}

// Helper functions
int initialize_superblock(superblock_t *sb, int disk_size, int block_size, int total_blocks)
{
    /*   
    -------------------------------------
    Blocks        Purpose
    ------------- -----------------------
    Block 0       Superblock

    Blocks 1–4    Free-space bitmap

    Blocks 5–20   Inode table (file
                    metadata table)

    Remaining     File data blocks
    blocks        
    -------------------------------------
        */
    sb->magic_number = 0x12345678; 
    sb->disk_size = disk_size;
    sb->block_size = block_size;
    sb->total_blocks = total_blocks;
    sb->bitmap_start = 1; // Bitmap starts at block 1   
    sb->bitmap_blocks = (total_blocks + block_size * 8 - 1) / (block_size * 8); // Calculate bitmap blocks needed
    sb->inode_start = sb->bitmap_start + sb->bitmap_blocks; // Inode table starts after bitmap
    sb->inode_blocks = 20; // Fixed number of blocks for inode table (can be adjusted based on requirements)
    sb->data_start = sb->inode_start + sb->inode_blocks; // Data blocks start after inode table
    return 0;
}
int initialize_bitmap(int *bitmap, int bitmap_blocks)
{
    memset(bitmap, 0, bitmap_blocks * sizeof(int)); // Initialize all bits to 0 (free)
    return 0;
}
int initialize_inode_table(inode_t *inode_table, int inode_blocks)
{
    memset(inode_table, 0, inode_blocks * sizeof(inode_t)); // Initialize all inodes to 0 (unused)
    return 0;   
}

// Functions for parsing workload file and executing commands
int parse_workload_file(char *workload_file, superblock_t *sb, int block_size, int total_blocks, char *disk_file)
{    FILE *fp = fopen(workload_file, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening workload file: %s\n", workload_file);
        return -1;
    }   
    char command[256];
    while (fgets(command, sizeof(command), fp) != NULL)
    {        // Remove newline character from command
        command[strcspn(command, "\n")] = 0;
        execute_command(command, sb, block_size, total_blocks, disk_file);
    }
    fclose(fp);
    return 0;
}
int execute_command(char *command, superblock_t *sb, int block_size, int total_blocks, char *disk_file)
{    
    printf("Executing command: %s\n", command);
    return 0;
}