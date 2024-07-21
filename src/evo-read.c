#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "evo_format.h"
#include "evo_metadata.h"

#define BUFFER_SIZE 4096
#define MIN(a,b) ((a) < (b) ? (a) : (b))

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s --input <input_file.evo>\n", program_name);
}

uint32_t crc32(const void *data, size_t length) {
    uint32_t crc = 0xffffffff;
    const uint8_t *p = (const uint8_t *)data;
    while (length--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void display_header(const struct evo_header *header) {
    printf("EVO File Header:\n");
    printf("Magic: %.8s\n", header->magic);
    printf("Version: %u\n", header->version);
    printf("Metadata size: %u bytes\n", header->metadata_size);
    printf("Data size: %lu bytes\n", header->data_size);
}

void display_metadata(const evo_metadata *metadata) {
    printf("\nMetadata:\n");
    printf("Name: %s\n", metadata->name);
    printf("Version: %s\n", metadata->version);
    printf("Description: %s\n", metadata->description);
    printf("Architecture: %u\n", metadata->architecture);
    printf("Installed size: %lu bytes\n", metadata->installed_size);
    printf("Maintainer: %s\n", metadata->maintainer);
    printf("Dependencies (%u):\n", metadata->num_dependencies);
    for (uint32_t i = 0; i < metadata->num_dependencies; i++) {
        printf("  %s\n", metadata->dependencies[i]);
    }
    printf("Package type: %u\n", metadata->package_type);
}

int main(int argc, char *argv[]) {
    char *input_file = NULL;

    // Parse command-line arguments
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            print_usage(argv[0]);
            return 1;
        }
        if (strcmp(argv[i], "--input") == 0) {
            input_file = argv[i + 1];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_file == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    // Open input file
    int fd = open(input_file, O_RDONLY);
    if (fd == -1) {
        perror("Error opening input file");
        return 1;
    }
    printf("Input file descriptor: %d\n", fd);

    // Open a new file descriptor for checksum calculation
    int checksum_fd = open(input_file, O_RDONLY);
    if (checksum_fd == -1) {
        perror("Error opening file for checksum calculation");
        close(fd);
        return 1;
    }
    printf("Checksum file descriptor: %d\n", checksum_fd);

    // Read header
    struct evo_header header;
    if (read(fd, &header, sizeof(header)) != sizeof(header)) {
        perror("Error reading header");
        close(fd);
        return 1;
    }

    // Verify magic number
    if (memcmp(header.magic, EVO_MAGIC, sizeof(header.magic)) != 0) {
        fprintf(stderr, "Invalid EVO file format\n");
        close(fd);
        return 1;
    }

    display_header(&header);

    // Read metadata
    evo_metadata metadata;
    if (read(fd, &metadata, sizeof(metadata)) != sizeof(metadata)) {
        perror("Error reading metadata");
        close(fd);
        return 1;
    }

    display_metadata(&metadata);

    // Verify data integrity
    uint32_t calculated_checksum = 0xffffffff;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    ssize_t total_bytes_read = 0;
    printf("Starting checksum calculation...\n");
    printf("Initial checksum value: 0x%08x\n", calculated_checksum);

    // Reset file pointer to the beginning for checksum calculation
    if (lseek(checksum_fd, 0, SEEK_SET) == -1) {
        perror("Error seeking to beginning of file for checksum calculation");
        close(fd);
        close(checksum_fd);
        return 1;
    }

    // Calculate checksum for the entire file content (except footer)
    off_t file_size = lseek(checksum_fd, 0, SEEK_END);
    if (file_size == -1) {
        perror("Error getting file size");
        close(fd);
        close(checksum_fd);
        return 1;
    }
    if (lseek(checksum_fd, 0, SEEK_SET) == -1) {
        perror("Error seeking to beginning of file");
        close(fd);
        close(checksum_fd);
        return 1;
    }
    off_t bytes_to_read = file_size - sizeof(struct evo_footer);
    printf("Total file size: %ld\n", file_size);
    printf("Bytes to read for checksum: %ld\n", bytes_to_read);
    printf("File pointer position after seek: %ld\n", lseek(checksum_fd, 0, SEEK_CUR));

    while (bytes_to_read > 0 && (bytes_read = read(checksum_fd, buffer, MIN(BUFFER_SIZE, bytes_to_read))) > 0) {
        calculated_checksum = crc32(buffer, bytes_read) ^ calculated_checksum;
        total_bytes_read += bytes_read;
        bytes_to_read -= bytes_read;
        printf("Bytes read for checksum: %zd (Remaining: %ld)\n", bytes_read, bytes_to_read);
        printf("Current calculated checksum: 0x%08x\n", calculated_checksum);
        printf("Total bytes read so far: %zd\n", total_bytes_read);
    }

    if (bytes_read == -1) {
        perror("Error reading data for checksum calculation");
        close(fd);
        close(checksum_fd);
        return 1;
    }

    printf("Calculated checksum (before final XOR): 0x%08x\n", calculated_checksum);
    calculated_checksum = ~calculated_checksum;
    printf("Calculated checksum (after final XOR): 0x%08x\n", calculated_checksum);
    printf("Final checksum in decimal: %u\n", calculated_checksum);
    printf("Total bytes read for checksum: %zd\n", total_bytes_read);


    // Adjust file pointer to read the footer
    printf("File position before reading footer: %ld\n", lseek(fd, 0, SEEK_CUR));
    if (lseek(fd, -sizeof(struct evo_footer), SEEK_END) == -1) {
        perror("Error seeking to footer");
        close(fd);
        close(checksum_fd);
        return 1;
    }

    struct evo_footer footer;
    if (read(fd, &footer, sizeof(footer)) != sizeof(footer)) {
        perror("Error reading footer");
        close(fd);
        close(checksum_fd);
        return 1;
    }
    printf("Footer data: checksum=%u\n", footer.checksum);

    printf("\nData Integrity:\n");
    printf("File size: %ld bytes\n", file_size);
    printf("Data read for checksum: %ld bytes\n", file_size - bytes_to_read);
    printf("Calculated checksum: 0x%08X (%u)\n", calculated_checksum, calculated_checksum);
    printf("Stored checksum:     0x%08X (%u)\n", footer.checksum, footer.checksum);
    printf("Checksum verification: %s\n", (calculated_checksum == footer.checksum) ? "PASSED" : "FAILED");

    // Close files
    printf("Closing file descriptors. fd: %d, checksum_fd: %d\n", fd, checksum_fd);
    close(fd);
    close(checksum_fd);

    return 0;
}
