#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAGIC_NUMBER 0x12345678
#define BITMAP_START 1
#define BITMAP_BLOCKS 4
#define INODE_START 5
#define INODE_BLOCKS 16
#define DATA_START 21
#define MAX_COMMAND_LENGTH 256

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

// functions for disk interface
FILE *disk_create(char *diskfile, int disk_size);
FILE *disk_open(char *disk_file);
FILE *disk_create_or_open(char *disk_file, int disk_size);
int disk_close(FILE *disk_file_pointer);
int disk_read_block(FILE *disk_file, int block_number, int block_size, int total_blocks, char *buffer);
int disk_write_block(FILE *disk_file, int block_number, int block_size, int total_blocks, char *buffer);

// helper functions
void print_usage(char *program_name);
int initialize_superblock(superblock_t *sb, int disk_size, int block_size, int total_blocks);
int initialize_bitmap(char *bitmap, int bitmap_size, int total_blocks);
int initialize_inode_table(inode_t *inode_table, int max_inodes);

// functions for parsing workload file and executing commands
int parse_workload_file(char *workload_file, FILE *disk_file_pointer, superblock_t *sb);
int execute_command(char *command, FILE *disk_file_pointer, superblock_t *sb);

int main(int argc, char *argv[])
{
    int option;
    int disk_size = 0;
    int block_size = 0;
    int total_blocks = 0;

    char *workload_file = NULL;
    char *disk_file = NULL;

    FILE *disk_file_pointer = NULL;
    superblock_t sb;

    // parse command-line arguments
    while ((option = getopt(argc, argv, "hf:d:b:t:w:")) != -1)
    {
        switch (option)
        {
        case 'h':
            print_usage(argv[0]);
            return 0;
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
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    // validate arguments
    if (disk_file == NULL || disk_size <= 0 || block_size <= 0 || total_blocks <= 0 || workload_file == NULL)
    {
        fprintf(stderr, "Invalid arguments\n");
        print_usage(argv[0]);
        return 1;
    }

    if (disk_size < block_size * total_blocks)
    {
        fprintf(stderr, "Invalid arguments: disk_size must be at least block_size * total_blocks\n");
        return 1;
    }

    if (block_size < (int)sizeof(superblock_t))
    {
        fprintf(stderr, "Invalid arguments: block_size is too small for the superblock\n");
        return 1;
    }

    if (total_blocks < DATA_START)
    {
        fprintf(stderr, "Invalid arguments: total_blocks must be at least 21\n");
        return 1;
    }

    // this bitmap uses one byte per block
    if (total_blocks > BITMAP_BLOCKS * block_size)
    {
        fprintf(stderr, "Invalid arguments: bitmap region cannot track all blocks\n");
        return 1;
    }

    initialize_superblock(&sb, disk_size, block_size, total_blocks);

    // create disk if missing, otherwise open existing disk
    disk_file_pointer = disk_create_or_open(disk_file, disk_size);
    if (disk_file_pointer == NULL)
    {
        return 1;
    }

    // open workload file and parse commands
    if (parse_workload_file(workload_file, disk_file_pointer, &sb) == 0)
    {
        disk_close(disk_file_pointer);
        return 1;
    }

    disk_close(disk_file_pointer);
    return 0;
}

void print_usage(char *program_name)
{
    fprintf(stderr, "Usage: %s -f <disk_file> -d <disk_size> -b <block_size> -t <total_blocks> -w <workload_file>\n", program_name);
}

// functions for disk interface
FILE *disk_create(char *diskfile, int disk_size)
{
    FILE *fp = fopen(diskfile, "wb+");

    if (fp == NULL)
    {
        fprintf(stderr, "Error creating disk file: %s\n", diskfile);
        return NULL;
    }

    // allocate the virtual disk file to the requested size
    if (fseek(fp, disk_size - 1, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error allocating disk file\n");
        fclose(fp);
        return NULL;
    }

    if (fputc('\0', fp) == EOF)
    {
        fprintf(stderr, "Error writing last byte of disk file\n");
        fclose(fp);
        return NULL;
    }

    rewind(fp);
    return fp;
}

FILE *disk_open(char *disk_file)
{
    FILE *fp = fopen(disk_file, "rb+");

    if (fp == NULL)
    {
        return NULL;
    }

    return fp;
}

FILE *disk_create_or_open(char *disk_file, int disk_size)
{
    FILE *fp = disk_open(disk_file);

    if (fp == NULL)
    {
        fp = disk_create(disk_file, disk_size);
    }

    return fp;
}

int disk_close(FILE *disk_file_pointer)
{
    if (disk_file_pointer == NULL)
    {
        return 0;
    }

    fclose(disk_file_pointer);
    return 1;
}

int disk_read_block(FILE *disk_file, int block_number, int block_size, int total_blocks, char *buffer)
{
    if (block_number < 0 || block_number >= total_blocks)
    {
        fprintf(stderr, "Error: invalid block read %d\n", block_number);
        return 0;
    }

    // move to the requested block
    if (fseek(disk_file, block_number * block_size, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error seeking to block %d\n", block_number);
        return 0;
    }

    // read one full block
    if (fread(buffer, 1, block_size, disk_file) != (size_t)block_size)
    {
        fprintf(stderr, "Error reading block %d\n", block_number);
        return 0;
    }

    return 1;
}

int disk_write_block(FILE *disk_file, int block_number, int block_size, int total_blocks, char *buffer)
{
    if (block_number < 0 || block_number >= total_blocks)
    {
        fprintf(stderr, "Error: invalid block write %d\n", block_number);
        return 0;
    }

    // move to the requested block
    if (fseek(disk_file, block_number * block_size, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error seeking to block %d\n", block_number);
        return 0;
    }

    // write one full block
    if (fwrite(buffer, 1, block_size, disk_file) != (size_t)block_size)
    {
        fprintf(stderr, "Error writing block %d\n", block_number);
        return 0;
    }

    fflush(disk_file);
    return 1;
}

// helper functions
int initialize_superblock(superblock_t *sb, int disk_size, int block_size, int total_blocks)
{
    sb->magic_number = MAGIC_NUMBER;
    sb->disk_size = disk_size;
    sb->block_size = block_size;
    sb->total_blocks = total_blocks;

    // fixed layout from the assignment
    sb->bitmap_start = BITMAP_START;
    sb->bitmap_blocks = BITMAP_BLOCKS;
    sb->inode_start = INODE_START;
    sb->inode_blocks = INODE_BLOCKS;
    sb->data_start = DATA_START;

    return 1;
}

int initialize_bitmap(char *bitmap, int bitmap_size, int total_blocks)
{
    memset(bitmap, 0, bitmap_size);

    // blocks 0-20 are metadata blocks
    for (int i = 0; i < DATA_START && i < total_blocks; i++)
    {
        bitmap[i] = 1;
    }

    return 1;
}

int initialize_inode_table(inode_t *inode_table, int max_inodes)
{
    // clear all inode entries
    memset(inode_table, 0, sizeof(inode_t) * max_inodes);
    return 1;
}

// functions for parsing workload file and executing commands
int parse_workload_file(char *workload_file, FILE *disk_file_pointer, superblock_t *sb)
{
    FILE *fp = fopen(workload_file, "r");
    char command[MAX_COMMAND_LENGTH];

    if (fp == NULL)
    {
        fprintf(stderr, "Error opening workload file: %s\n", workload_file);
        return 0;
    }

    while (fgets(command, sizeof(command), fp) != NULL)
    {
        // remove newline character
        command[strcspn(command, "\n")] = '\0';

        if (command[0] != '\0')
        {
            if (execute_command(command, disk_file_pointer, sb) == 0)
            {
                fclose(fp);
                return 0;
            }
        }
    }

    fclose(fp);
    return 1;
}

int execute_command(char *command, FILE *disk_file_pointer, superblock_t *sb)
{
    // these will be used when commands are implemented
    (void)disk_file_pointer;
    (void)sb;

    printf("Executing command: %s\n", command);
    return 1;
}