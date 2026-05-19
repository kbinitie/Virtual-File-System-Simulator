# SP26 CPSC 380 Programming Assignment 5 – Virtual File System Simulator

## Contributors
Brent Matthew Ortizo  
Student ID: 2452997  
Email: ortizo@chapman.edu  

Kayode Binitie  
Student ID: 2461327  
Email: binitie@chapman.edu  

---

## Repository

GitHub Repository:  
https://github.com/kbinitie/Virtual-File-System-Simulator

---

## Description

This project implements a simplified virtual file system simulator that operates on top of a virtual disk file. The simulator models how operating systems manage files using block-based secondary storage.

The project includes:

- A virtual disk interface
- Block-level disk access
- A superblock
- A free-space bitmap
- An inode table
- Contiguous file allocation
- File creation, reading, writing, and deletion
- SSTF (Shortest Seek Time First) disk scheduling simulation

The file system stores all information directly inside a virtual disk image and simulates how metadata and file contents are organized on a real disk.

---

## Program Execution

Program format:
```
./fs -f <disk_file> -d <disk_size> -b <block_size> -t <total_blocks> -w <workload_file>
```

Example:
```
./fs -f vdisk.img -d 1048576 -b 512 -t 2048 -w workload.txt
```

Arguments:

- `disk_file` — name of the virtual disk image
- `disk_size` — total virtual disk size in bytes
- `block_size` — size of each block
- `total_blocks` — total number of blocks
- `workload_file` — text file containing file-system commands

---

## Design Approach

The virtual file system is built using a layered architecture similar to a real operating system.

The implementation consists of:

1. Disk Interface Layer
2. Metadata Management
3. Free-Space Management
4. File Operations
5. Disk Scheduling Simulation

The simulator treats the virtual disk image as a block device and only accesses storage one full block at a time.

---

## Disk Layout

The virtual disk is divided into several regions.

| Blocks | Purpose |
|---|---|
| Block 0 | Superblock |
| Blocks 1–4 | Free-space bitmap |
| Blocks 5–20 | Inode table |
| Blocks 21+ | File data blocks |

This structure simulates how operating systems organize secondary storage devices.

---

## Superblock

The superblock is stored in block 0 and contains information describing the layout of the virtual disk.

The superblock stores:

- Magic number
- Disk size
- Block size
- Total blocks
- Bitmap starting block
- Bitmap block count
- Inode table starting block
- Inode table block count
- Data region starting block

Structure used:

`````c
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
`````

The superblock acts as the master metadata record for the file system.

---

## Inode Table

The inode table is stored in blocks 5–20.

Each inode stores metadata for one file, including:

- Filename
- File size
- Starting block
- Number of allocated blocks
- Used/free flag

Structure used:

`````c
typedef struct
{
    char filename[32];
    int size;
    int start_block;
    int block_count;
    int used;
} inode_t;
`````

The inode table stores metadata only. File contents themselves are stored in the data block region.

---

## Free-Space Bitmap

The free-space bitmap tracks which blocks are allocated and which blocks are free.

Bitmap behavior:

- `1` = allocated block
- `0` = free block

After formatting:

- Block 0 is allocated for the superblock
- Blocks 1–4 are allocated for the bitmap
- Blocks 5–20 are allocated for the inode table
- Remaining blocks are free for file data

Example bitmap:
```
111111111111111111111000000000000000…
```

The bitmap updates dynamically whenever files are written or deleted.

---

## Contiguous Allocation

Files use contiguous allocation.

Example:
```
file1.txt -> blocks 21, 22, 23
```

When writing a file:

1. The bitmap is scanned for contiguous free blocks
2. Blocks are allocated sequentially
3. The inode stores the starting block and block count
4. Bitmap entries are updated

When files are deleted:

1. Allocated blocks are cleared
2. Bitmap entries are reset to free
3. The inode entry is cleared

---

## Disk Interface

The disk interface handles all low-level storage operations.

Implemented functions:

- `disk_create()`
- `disk_open()`
- `disk_close()`
- `disk_read_block()`
- `disk_write_block()`

The virtual disk is treated as a block device, meaning all reads and writes occur one full block at a time.

---

## File-System Commands

The workload file contains commands automatically executed by the simulator.

Supported commands:

| Command | Description |
|---|---|
| `format` | Initialize disk |
| `create <filename>` | Create file |
| `write <filename> <data>` | Write file data |
| `read <filename>` | Read file |
| `delete <filename>` | Delete file |
| `ls` | List files |
| `stat <filename>` | Display metadata |
| `bitmap` | Display bitmap |

---

## SSTF Disk Scheduling

For every read and write operation, the simulator records the blocks accessed and computes disk head movement using SSTF (Shortest Seek Time First).

Example:

Initial head position:
```
0
```

Accessed blocks:
```
21, 22, 23
```

Program output:
```
SSTF Service Order:
21 -> 22 -> 23
Total Head Movement: 23
```

The SSTF implementation selects the closest unvisited block to minimize total seek distance.

For demonstration purposes, the sample workload uses a smaller block size (64 bytes) so larger file writes span multiple contiguous blocks. This allows the SSTF scheduling behavior and contiguous allocation system to be clearly observed in the output.

---

## Compilation

Compilation of the program is done with:

```
gcc fs.c -o fs
```

---

## Execution Example

```
./fs -f vdisk.img -d 16384 -b 64 -t 256 -w workload.txt
```

---

## Sample Workload File

Example workload:
```
format
ls
bitmap
create big.txt
write big.txt abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghij
read big.txt
stat big.txt
bitmap
create small.txt
write small.txt hello world
read small.txt
stat small.txt
ls
delete big.txt
bitmap
create reused.txt
write reused.txt reused block
read reused.txt
stat reused.txt
ls
bitmap
delete small.txt
delete reused.txt
ls
bitmap
```

---

## Example Output

Note: Blocks 21–23 were allocated contiguously for big.txt.

```
Disk formatted successfully.

Files:
No files found.

Bitmap:
111111111111111111111000000000000000...

File created: big.txt

Wrote file: big.txt
Start block: 21
Blocks used: 3

SSTF Service Order:
21 -> 22 -> 23
Total Head Movement: 23

File: big.txt
Data: abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghij

SSTF Service Order:
21 -> 22 -> 23
Total Head Movement: 23

File metadata:
Filename: big.txt
Size: 140 bytes
Start block: 21
Block count: 3
Used: 1

Bitmap:
111111111111111111111111000000000000...

File created: small.txt

Wrote file: small.txt
Start block: 24
Blocks used: 1

SSTF Service Order:
24
Total Head Movement: 24

File: small.txt
Data: hello world

File metadata:
Filename: small.txt
Size: 11 bytes
Start block: 24
Block count: 1
Used: 1

Files:
big.txt
small.txt

File deleted: big.txt

Bitmap:
111111111111111111111000100000000000...

File created: reused.txt

Wrote file: reused.txt
Start block: 21
Blocks used: 1

SSTF Service Order:
21
Total Head Movement: 21

File: reused.txt
Data: reused block

File metadata:
Filename: reused.txt
Size: 12 bytes
Start block: 21
Block count: 1
Used: 1

Files:
reused.txt
small.txt

Bitmap:
111111111111111111111100100000000000...

File deleted: small.txt
File deleted: reused.txt

Files:
No files found.

Bitmap:
111111111111111111111000000000000000...
```

---

## Bitmap Observations

Note: The bitmap examples below are shortened for readability. The actual program prints the full bitmap for all blocks.

Initial bitmap after formatting:
```
111111111111111111111000000000...
```

After writing `big.txt` across three blocks:
```
111111111111111111111111000000...
```

After deleting `big.txt` while small.txt still exists:
```
111111111111111111111000100000...
```

After deleting all files:
```
111111111111111111111000000000...
```

This demonstrates that:

- Metadata blocks always remain allocated
- File blocks are dynamically allocated
- Files use contiguous block allocation
- File blocks are properly freed after deletion
- Freed blocks can be reused by future files

---

## Block Reuse Verification

The simulator correctly reuses freed blocks.

Example:

1. `big.txt` used blocks `21`, `22`, and `23`
2. `big.txt` was deleted
3. `reused.txt` reused block `21`

This verifies that the bitmap correctly tracks freed space and allows freed blocks to be reused by future files.

---

## Data Verification

After deleting all files, the virtual disk was verified using:
```
strings vdisk.img
```

No deleted file contents remained visible, confirming that deleted blocks were cleared successfully.

---

## Files Included

- `fs.c` — main implementation
- `workload.txt` — sample workload file
- `README.md` — project documentation

---

## Error Handling

The simulator handles:

- Invalid command-line arguments
- Invalid block accesses
- Missing workload files
- Duplicate file creation
- Writing to nonexistent files
- Reading nonexistent files
- Deleting nonexistent files
- Insufficient contiguous space
- Memory allocation failures

---

## Assumptions

- Files use contiguous allocation only
- One bitmap entry corresponds to one block
- The bitmap uses one byte per block
- The workload file contains valid commands
- Block size is large enough for the superblock
- Disk accesses occur one block at a time

---

## Key Insights

- Contiguous allocation simplifies file management
- The bitmap efficiently tracks free space
- Inodes separate metadata from actual file contents
- SSTF reduces total disk head movement
- File deletion properly frees reusable space
- Disk organization strongly affects storage management

---

## Conclusion

This project demonstrates how operating systems manage secondary storage using block-based file systems. By implementing a virtual disk, bitmap allocation, inode metadata management, contiguous allocation, and SSTF scheduling, we were able to simulate many of the core concepts behind real-world file systems.

The project provided hands-on experience with low-level storage management, file-system organization, metadata handling, and disk scheduling algorithms.