// Brent Matthew Ortizo and Bintie Kayode

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
int initialize_inode_table(inode_t *inode_table, int inode_bytes);
int format_disk(FILE *disk_file_pointer, superblock_t *sb);

// bitmap helper functions
int load_bitmap(FILE *disk_file_pointer, superblock_t *sb, char *bitmap);
int save_bitmap(FILE *disk_file_pointer, superblock_t *sb, char *bitmap);
int find_contiguous_blocks(char *bitmap, superblock_t *sb, int blocks_needed);

// inode helper functions
int load_inode_table(FILE *disk_file_pointer, superblock_t *sb, inode_t *inode_table);
int save_inode_table(FILE *disk_file_pointer, superblock_t *sb, inode_t *inode_table);
int create_file(FILE *disk_file_pointer, superblock_t *sb, char *filename);
int write_file(FILE *disk_file_pointer, superblock_t *sb, char *filename, char *data);
int read_file(FILE *disk_file_pointer, superblock_t *sb, char *filename);
int delete_file(FILE *disk_file_pointer, superblock_t *sb, char *filename);

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

    if (fseek(disk_file, block_number * block_size, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error seeking to block %d\n", block_number);
        return 0;
    }

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

    if (fseek(disk_file, block_number * block_size, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error seeking to block %d\n", block_number);
        return 0;
    }

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

int initialize_inode_table(inode_t *inode_table, int inode_bytes)
{
    // clear all inode table bytes
    memset(inode_table, 0, inode_bytes);
    return 1;
}

int format_disk(FILE *disk_file_pointer, superblock_t *sb)
{
    char *block_buffer;
    char *bitmap;
    inode_t *inode_table;

    int bitmap_size;
    int inode_bytes;

    block_buffer = (char *)malloc(sb->block_size);
    if (block_buffer == NULL)
    {
        fprintf(stderr, "Error allocating block buffer\n");
        return 0;
    }

    // write superblock to block 0
    memset(block_buffer, 0, sb->block_size);
    memcpy(block_buffer, sb, sizeof(superblock_t));

    if (disk_write_block(disk_file_pointer, 0, sb->block_size, sb->total_blocks, block_buffer) == 0)
    {
        free(block_buffer);
        return 0;
    }

    // create bitmap and mark metadata blocks as used
    bitmap_size = sb->bitmap_blocks * sb->block_size;
    bitmap = (char *)malloc(bitmap_size);

    if (bitmap == NULL)
    {
        fprintf(stderr, "Error allocating bitmap\n");
        free(block_buffer);
        return 0;
    }

    initialize_bitmap(bitmap, bitmap_size, sb->total_blocks);

    // write bitmap blocks 1-4
    for (int i = 0; i < sb->bitmap_blocks; i++)
    {
        memset(block_buffer, 0, sb->block_size);
        memcpy(block_buffer, bitmap + (i * sb->block_size), sb->block_size);

        if (disk_write_block(disk_file_pointer, sb->bitmap_start + i, sb->block_size, sb->total_blocks, block_buffer) == 0)
        {
            free(block_buffer);
            free(bitmap);
            return 0;
        }
    }

    // clear inode table
    inode_bytes = sb->inode_blocks * sb->block_size;
    inode_table = (inode_t *)malloc(inode_bytes);

    if (inode_table == NULL)
    {
        fprintf(stderr, "Error allocating inode table\n");
        free(block_buffer);
        free(bitmap);
        return 0;
    }

    initialize_inode_table(inode_table, inode_bytes);

    // write inode table blocks 5-20
    for (int i = 0; i < sb->inode_blocks; i++)
    {
        memset(block_buffer, 0, sb->block_size);
        memcpy(block_buffer, ((char *)inode_table) + (i * sb->block_size), sb->block_size);

        if (disk_write_block(disk_file_pointer, sb->inode_start + i, sb->block_size, sb->total_blocks, block_buffer) == 0)
        {
            free(block_buffer);
            free(bitmap);
            free(inode_table);
            return 0;
        }
    }

    free(block_buffer);
    free(bitmap);
    free(inode_table);

    printf("Disk formatted successfully.\n");
    return 1;
}

// bitmap helper functions
int load_bitmap(FILE *disk_file_pointer, superblock_t *sb, char *bitmap)
{
    char *block_buffer = (char *)malloc(sb->block_size);

    if (block_buffer == NULL)
    {
        fprintf(stderr, "Error allocating block buffer\n");
        return 0;
    }

    // read bitmap blocks 1-4
    for (int i = 0; i < sb->bitmap_blocks; i++)
    {
        if (disk_read_block(disk_file_pointer, sb->bitmap_start + i, sb->block_size, sb->total_blocks, block_buffer) == 0)
        {
            free(block_buffer);
            return 0;
        }

        memcpy(bitmap + (i * sb->block_size), block_buffer, sb->block_size);
    }

    free(block_buffer);
    return 1;
}

int save_bitmap(FILE *disk_file_pointer, superblock_t *sb, char *bitmap)
{
    char *block_buffer = (char *)malloc(sb->block_size);

    if (block_buffer == NULL)
    {
        fprintf(stderr, "Error allocating block buffer\n");
        return 0;
    }

    // write bitmap blocks 1-4
    for (int i = 0; i < sb->bitmap_blocks; i++)
    {
        memset(block_buffer, 0, sb->block_size);
        memcpy(block_buffer, bitmap + (i * sb->block_size), sb->block_size);

        if (disk_write_block(disk_file_pointer, sb->bitmap_start + i, sb->block_size, sb->total_blocks, block_buffer) == 0)
        {
            free(block_buffer);
            return 0;
        }
    }

    free(block_buffer);
    return 1;
}

int find_contiguous_blocks(char *bitmap, superblock_t *sb, int blocks_needed)
{
    int start_block = -1;
    int count = 0;

    // search for free contiguous blocks in data region
    for (int i = sb->data_start; i < sb->total_blocks; i++)
    {
        if (bitmap[i] == 0)
        {
            if (count == 0)
            {
                start_block = i;
            }

            count++;

            if (count == blocks_needed)
            {
                return start_block;
            }
        }
        else
        {
            count = 0;
            start_block = -1;
        }
    }

    return -1;
}

// inode helper functions
int load_inode_table(FILE *disk_file_pointer, superblock_t *sb, inode_t *inode_table)
{
    char *block_buffer = (char *)malloc(sb->block_size);

    if (block_buffer == NULL)
    {
        fprintf(stderr, "Error allocating block buffer\n");
        return 0;
    }

    // read inode table blocks 5-20
    for (int i = 0; i < sb->inode_blocks; i++)
    {
        if (disk_read_block(disk_file_pointer, sb->inode_start + i, sb->block_size, sb->total_blocks, block_buffer) == 0)
        {
            free(block_buffer);
            return 0;
        }

        memcpy(((char *)inode_table) + (i * sb->block_size), block_buffer, sb->block_size);
    }

    free(block_buffer);
    return 1;
}

int save_inode_table(FILE *disk_file_pointer, superblock_t *sb, inode_t *inode_table)
{
    char *block_buffer = (char *)malloc(sb->block_size);

    if (block_buffer == NULL)
    {
        fprintf(stderr, "Error allocating block buffer\n");
        return 0;
    }

    // write inode table blocks 5-20
    for (int i = 0; i < sb->inode_blocks; i++)
    {
        memset(block_buffer, 0, sb->block_size);
        memcpy(block_buffer, ((char *)inode_table) + (i * sb->block_size), sb->block_size);

        if (disk_write_block(disk_file_pointer, sb->inode_start + i, sb->block_size, sb->total_blocks, block_buffer) == 0)
        {
            free(block_buffer);
            return 0;
        }
    }

    free(block_buffer);
    return 1;
}

int create_file(FILE *disk_file_pointer, superblock_t *sb, char *filename)
{
    inode_t *inode_table;
    int max_inodes;
    int inode_bytes;
    int free_index = -1;

    inode_bytes = sb->inode_blocks * sb->block_size;
    max_inodes = inode_bytes / sizeof(inode_t);
    inode_table = (inode_t *)malloc(inode_bytes);

    if (inode_table == NULL)
    {
        fprintf(stderr, "Error allocating inode table\n");
        return 0;
    }

    if (load_inode_table(disk_file_pointer, sb, inode_table) == 0)
    {
        free(inode_table);
        return 0;
    }

    // find duplicate file or first free inode
    for (int i = 0; i < max_inodes; i++)
    {
        if (inode_table[i].used == 1 && strcmp(inode_table[i].filename, filename) == 0)
        {
            fprintf(stderr, "Error: file already exists: %s\n", filename);
            free(inode_table);
            return 0;
        }

        if (inode_table[i].used == 0 && free_index == -1)
        {
            free_index = i;
        }
    }

    if (free_index == -1)
    {
        fprintf(stderr, "Error: no free inode available\n");
        free(inode_table);
        return 0;
    }

    // create empty file metadata
    strncpy(inode_table[free_index].filename, filename, sizeof(inode_table[free_index].filename) - 1);
    inode_table[free_index].filename[sizeof(inode_table[free_index].filename) - 1] = '\0';
    inode_table[free_index].size = 0;
    inode_table[free_index].start_block = -1;
    inode_table[free_index].block_count = 0;
    inode_table[free_index].used = 1;

    if (save_inode_table(disk_file_pointer, sb, inode_table) == 0)
    {
        free(inode_table);
        return 0;
    }

    printf("File created: %s\n", filename);

    free(inode_table);
    return 1;
}

int write_file(FILE *disk_file_pointer, superblock_t *sb, char *filename, char *data)
{
    inode_t *inode_table;
    char *bitmap;
    char *block_buffer;

    int inode_bytes;
    int max_inodes;
    int bitmap_size;
    int file_index = -1;
    int data_size;
    int blocks_needed;
    int start_block;

    inode_bytes = sb->inode_blocks * sb->block_size;
    max_inodes = inode_bytes / sizeof(inode_t);
    bitmap_size = sb->bitmap_blocks * sb->block_size;

    inode_table = (inode_t *)malloc(inode_bytes);
    bitmap = (char *)malloc(bitmap_size);
    block_buffer = (char *)malloc(sb->block_size);

    if (inode_table == NULL || bitmap == NULL || block_buffer == NULL)
    {
        fprintf(stderr, "Error allocating memory for write\n");
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    if (load_inode_table(disk_file_pointer, sb, inode_table) == 0)
    {
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    if (load_bitmap(disk_file_pointer, sb, bitmap) == 0)
    {
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    // find file inode
    for (int i = 0; i < max_inodes; i++)
    {
        if (inode_table[i].used == 1 && strcmp(inode_table[i].filename, filename) == 0)
        {
            file_index = i;
            break;
        }
    }

    if (file_index == -1)
    {
        fprintf(stderr, "Error: file not found: %s\n", filename);
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    if (inode_table[file_index].block_count > 0)
    {
        fprintf(stderr, "Error: file already has data. delete it before rewriting: %s\n", filename);
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    data_size = strlen(data);
    blocks_needed = (data_size + sb->block_size - 1) / sb->block_size;

    start_block = find_contiguous_blocks(bitmap, sb, blocks_needed);

    if (start_block == -1)
    {
        fprintf(stderr, "Error: not enough contiguous free space\n");
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    // write file data into contiguous blocks
    for (int i = 0; i < blocks_needed; i++)
    {
        int bytes_left = data_size - (i * sb->block_size);
        int bytes_to_copy = sb->block_size;

        if (bytes_left < sb->block_size)
        {
            bytes_to_copy = bytes_left;
        }

        memset(block_buffer, 0, sb->block_size);
        memcpy(block_buffer, data + (i * sb->block_size), bytes_to_copy);

        if (disk_write_block(disk_file_pointer, start_block + i, sb->block_size, sb->total_blocks, block_buffer) == 0)
        {
            free(inode_table);
            free(bitmap);
            free(block_buffer);
            return 0;
        }

        bitmap[start_block + i] = 1;
    }

    // update inode metadata
    inode_table[file_index].size = data_size;
    inode_table[file_index].start_block = start_block;
    inode_table[file_index].block_count = blocks_needed;

    if (save_inode_table(disk_file_pointer, sb, inode_table) == 0)
    {
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    if (save_bitmap(disk_file_pointer, sb, bitmap) == 0)
    {
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    printf("Wrote file: %s\n", filename);
    printf("Start block: %d\n", start_block);
    printf("Blocks used: %d\n", blocks_needed);

    free(inode_table);
    free(bitmap);
    free(block_buffer);
    return 1;
}

// func to read file
int read_file(FILE *disk_file_pointer, superblock_t *sb, char *filename)
{
    inode_t *inode_table;
    char *block_buffer;
    char *file_data;

    int inode_bytes;
    int max_inodes;
    int file_index = -1;
    int data_offset = 0;

    inode_bytes = sb->inode_blocks * sb->block_size;
    max_inodes = inode_bytes / sizeof(inode_t);

    inode_table = (inode_t *)malloc(inode_bytes);
    block_buffer = (char *)malloc(sb->block_size);

    if (inode_table == NULL || block_buffer == NULL)
    {
        fprintf(stderr, "Error allocating memory for read\n");
        free(inode_table);
        free(block_buffer);
        return 0;
    }

    if (load_inode_table(disk_file_pointer, sb, inode_table) == 0)
    {
        free(inode_table);
        free(block_buffer);
        return 0;
    }

    // find file inode
    for (int i = 0; i < max_inodes; i++)
    {
        if (inode_table[i].used == 1 && strcmp(inode_table[i].filename, filename) == 0)
        {
            file_index = i;
            break;
        }
    }

    if (file_index == -1)
    {
        fprintf(stderr, "Error: file not found: %s\n", filename);
        free(inode_table);
        free(block_buffer);
        return 0;
    }

    if (inode_table[file_index].block_count == 0)
    {
        printf("File is empty: %s\n", filename);
        free(inode_table);
        free(block_buffer);
        return 1;
    }

    file_data = (char *)malloc(inode_table[file_index].size + 1);

    if (file_data == NULL)
    {
        fprintf(stderr, "Error allocating file data\n");
        free(inode_table);
        free(block_buffer);
        return 0;
    }

    // read file data from contiguous blocks
    for (int i = 0; i < inode_table[file_index].block_count; i++)
    {
        int bytes_left = inode_table[file_index].size - data_offset;
        int bytes_to_copy = sb->block_size;

        if (bytes_left < sb->block_size)
        {
            bytes_to_copy = bytes_left;
        }

        if (disk_read_block(disk_file_pointer,
                            inode_table[file_index].start_block + i,
                            sb->block_size,
                            sb->total_blocks,
                            block_buffer) == 0)
        {
            free(inode_table);
            free(block_buffer);
            free(file_data);
            return 0;
        }

        memcpy(file_data + data_offset, block_buffer, bytes_to_copy);
        data_offset += bytes_to_copy;
    }

    file_data[inode_table[file_index].size] = '\0';

    printf("File: %s\n", filename);
    printf("Data: %s\n", file_data);

    free(inode_table);
    free(block_buffer);
    free(file_data);

    return 1;
}

// func to delete file
int delete_file(FILE *disk_file_pointer, superblock_t *sb, char *filename)
{
    inode_t *inode_table;
    char *bitmap;
    char *block_buffer;

    int inode_bytes;
    int max_inodes;
    int bitmap_size;
    int file_index = -1;

    inode_bytes = sb->inode_blocks * sb->block_size;
    max_inodes = inode_bytes / sizeof(inode_t);
    bitmap_size = sb->bitmap_blocks * sb->block_size;

    inode_table = (inode_t *)malloc(inode_bytes);
    bitmap = (char *)malloc(bitmap_size);
    block_buffer = (char *)malloc(sb->block_size);

    if (inode_table == NULL || bitmap == NULL || block_buffer == NULL)
    {
        fprintf(stderr, "Error allocating memory for delete\n");
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    if (load_inode_table(disk_file_pointer, sb, inode_table) == 0)
    {
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    if (load_bitmap(disk_file_pointer, sb, bitmap) == 0)
    {
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    // find file inode
    for (int i = 0; i < max_inodes; i++)
    {
        if (inode_table[i].used == 1 && strcmp(inode_table[i].filename, filename) == 0)
        {
            file_index = i;
            break;
        }
    }

    if (file_index == -1)
    {
        fprintf(stderr, "Error: file not found: %s\n", filename);
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    // free data blocks in bitmap and clear data blocks
    for (int i = 0; i < inode_table[file_index].block_count; i++)
    {
        int block_number = inode_table[file_index].start_block + i;

        bitmap[block_number] = 0;

        memset(block_buffer, 0, sb->block_size);
        if (disk_write_block(disk_file_pointer, block_number, sb->block_size, sb->total_blocks, block_buffer) == 0)
        {
            free(inode_table);
            free(bitmap);
            free(block_buffer);
            return 0;
        }
    }

    // clear inode metadata
    memset(&inode_table[file_index], 0, sizeof(inode_t));

    if (save_inode_table(disk_file_pointer, sb, inode_table) == 0)
    {
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    if (save_bitmap(disk_file_pointer, sb, bitmap) == 0)
    {
        free(inode_table);
        free(bitmap);
        free(block_buffer);
        return 0;
    }

    printf("File deleted: %s\n", filename);

    free(inode_table);
    free(bitmap);
    free(block_buffer);

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
    char command_name[32];
    char filename[32];
    char data[MAX_COMMAND_LENGTH];

    if (strcmp(command, "format") == 0)
    {
        return format_disk(disk_file_pointer, sb);
    }

    if (sscanf(command, "%31s %31s", command_name, filename) >= 2)
    {
        if (strcmp(command_name, "create") == 0)
        {
            return create_file(disk_file_pointer, sb, filename);
        }

        if (strcmp(command_name, "write") == 0)
        {
            if (sscanf(command, "%31s %31s %[^\n]", command_name, filename, data) == 3)
            {
                return write_file(disk_file_pointer, sb, filename, data);
            }

            fprintf(stderr, "Error: write command needs filename and data\n");
            return 0;
        }

        if (strcmp(command_name, "read") == 0)
        {
            return read_file(disk_file_pointer, sb, filename);
        }

        if (strcmp(command_name, "delete") == 0)
        {
            return delete_file(disk_file_pointer, sb, filename);
        }
    
    }

    printf("Executing command: %s\n", command);
    return 1;
}