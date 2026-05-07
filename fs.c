#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

int main(int argc, char *argv[]) {
    int option;
    char *disk_file;
    char *disk_size;
    char *block_size;
    char *total_blocks;
    char *workload_file;

    // Define long options
    static struct option long_options[] = {
        {"help",        no_argument,        0, 'h'},
        {"version",     no_argument,        0, 'v'},
        {"diskFile",    required_argument,  0, 'f'},
        {"diskSize",    required_argument,  0, 'd'},
        {"blockSize",   required_argument,  0, 'b'},
        {"totalBlocks", required_argument,  0, 't'},
        {"workloadFile",required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    while ((option = getopt_long(argc, argv, "hvf:d:b:t:w:", long_options, NULL)) != -1) {
        switch (option) {
            case 'h':
                printf("Usage: %s [--help] [--version] [-f <disk_file> -d <disk_size> -b "
                       "<block_size> -t <total_blocks> -w <workload file>]\n", argv[0]);
                break;
            case 'v':
                printf("Version 0.6.7\n");
                break;
            case 'f':
                disk_file = optarg;
                break;
            case 'd':
               disk_size = optarg;
                break;
            case 'b':
              block_size = optarg;
                break;
            case 't':
                total_blocks = optarg;
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

    return 0;
}
