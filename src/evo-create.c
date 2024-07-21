#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <getopt.h>
#include "evo_format.h"
#include "evo_metadata.h"

static struct option long_options[] = {
    {"input", required_argument, 0, 'i'},
    {"output", required_argument, 0, 'o'},
    {"supported-architectures", required_argument, 0, 'a'},
    {"min-screen-width", required_argument, 0, 'w'},
    {"min-screen-height", required_argument, 0, 'h'},
    {"required-permissions", required_argument, 0, 'p'},
    {"target-sdk-version", required_argument, 0, 's'},
    {"min-os-version", required_argument, 0, 'v'},
    {0, 0, 0, 0}
};

void parse_supported_architectures(evo_metadata *metadata, const char *architectures);
void parse_required_permissions(evo_metadata *metadata, const char *permissions);

#define BUFFER_SIZE 4096

#define MAX_ARCHITECTURES 10
#define MAX_PERMISSIONS 50

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s --input <input_file> --output <output_file.evo> [OPTIONS]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --supported-architectures <arch1,arch2,...>  Comma-separated list of supported mobile architectures\n");
    fprintf(stderr, "  --min-screen-width <width>                   Minimum screen width\n");
    fprintf(stderr, "  --min-screen-height <height>                 Minimum screen height\n");
    fprintf(stderr, "  --required-permissions <perm1,perm2,...>     Comma-separated list of required permissions\n");
    fprintf(stderr, "  --target-sdk-version <version>               Target SDK version for mobile platforms\n");
    fprintf(stderr, "  --min-os-version <version>                   Minimum supported mobile OS version\n");
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


void initialize_default_metadata(evo_metadata *metadata) {
    strncpy(metadata->name, "Default Package", EVO_MAX_NAME_LENGTH);
    strncpy(metadata->version, "1.0.0", EVO_MAX_VERSION_LENGTH);
    strncpy(metadata->description, "Default description", EVO_MAX_DESCRIPTION_LENGTH);
    metadata->architecture = 0;
    metadata->installed_size = 0;
    strncpy(metadata->maintainer, "Default Maintainer", EVO_MAX_NAME_LENGTH);
    metadata->num_dependencies = 0;
    metadata->package_type = 0;

    // Initialize mobile-specific fields
    metadata->num_supported_architectures = 0;
    metadata->min_screen_size.width = 0;
    metadata->min_screen_size.height = 0;
    metadata->num_required_permissions = 0;
    metadata->target_sdk_version = 0;
    strncpy(metadata->min_os_version, "0.0", EVO_MAX_VERSION_LENGTH);
}

int main(int argc, char *argv[]) {
    char *input_file = NULL;
    char *output_file = NULL;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    evo_metadata metadata;
    initialize_default_metadata(&metadata);

    // Parse command-line arguments
    int opt;
    int option_index = 0;
    static struct option long_options[] = {
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"supported-architectures", required_argument, 0, 'a'},
        {"min-screen-width", required_argument, 0, 'w'},
        {"min-screen-height", required_argument, 0, 'h'},
        {"required-permissions", required_argument, 0, 'p'},
        {"target-sdk-version", required_argument, 0, 's'},
        {"min-os-version", required_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        printf("Parsing option: %c\n", opt);
        switch (opt) {
            case 'i':
                input_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'a':
                parse_supported_architectures(&metadata, optarg);
                break;
            case 'w':
                metadata.min_screen_size.width = atoi(optarg);
                break;
            case 'h':
                metadata.min_screen_size.height = atoi(optarg);
                break;
            case 'p':
                parse_required_permissions(&metadata, optarg);
                break;
            case 's':
                metadata.target_sdk_version = atoi(optarg);
                break;
            case 'v':
                strncpy(metadata.min_os_version, optarg, EVO_MAX_VERSION_LENGTH - 1);
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (input_file == NULL || output_file == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    // Open input file
    int input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        perror("Error opening input file");
        return 1;
    }
    printf("Input file descriptor opened: %d\n", input_fd);
    printf("Input file path: %s\n", input_file);

    // Get input file size
    struct stat input_stat;
    if (fstat(input_fd, &input_stat) == -1) {
        perror("Error getting input file size");
        close(input_fd);
        return 1;
    }

    // Prepare header
    struct evo_header header;
    memcpy(header.magic, EVO_MAGIC, sizeof(header.magic));
    header.version = 1;
    header.data_size = input_stat.st_size;

    // Set metadata size in header
    header.metadata_size = sizeof(evo_metadata);



    // Open output file
    int output_fd = open(output_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1) {
        perror("Error opening output file");
        close(input_fd);
        return 1;
    }
    printf("Output file descriptor: %d\n", output_fd);

    // Write header
    if (write(output_fd, &header, sizeof(header)) != sizeof(header)) {
        perror("Error writing header");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Write metadata
    if (write(output_fd, &metadata, sizeof(metadata)) != sizeof(metadata)) {
        perror("Error writing metadata");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Copy input file content to output file
    while ((bytes_read = read(input_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(output_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("Error writing data");
            close(input_fd);
            close(output_fd);
            return 1;
        }
    }

    if (bytes_read == -1) {
        perror("Error reading input file");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Calculate checksum for the entire file content (except footer)
    uint32_t calculated_checksum = 0xffffffff;
    printf("Starting checksum calculation...\n");
    printf("Initial checksum value: 0x%08x\n", calculated_checksum);

    // Get total file size
    off_t file_size = lseek(output_fd, 0, SEEK_END);
    if (file_size == -1) {
        perror("Error getting file size");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Calculate bytes to read (excluding footer)
    off_t bytes_to_read = file_size - sizeof(struct evo_footer);
    printf("File size: %ld, Bytes to read: %ld\n", file_size, bytes_to_read);
    printf("Total file size: %ld\n", file_size);
    printf("Bytes to read for checksum: %ld\n", bytes_to_read);
    printf("Initial checksum value: 0x%08x\n", calculated_checksum);

    // Seek to the beginning of the output file
    if (lseek(output_fd, 0, SEEK_SET) == -1) {
        perror("Error seeking to beginning of file for checksum calculation");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Read from the output file for checksum calculation
    ssize_t total_bytes_read = 0;
    while (bytes_to_read > 0 && (bytes_read = read(output_fd, buffer, BUFFER_SIZE)) > 0) {
        uint32_t chunk_checksum = crc32(buffer, bytes_read);
        calculated_checksum = chunk_checksum ^ calculated_checksum;
        total_bytes_read += bytes_read;
        bytes_to_read -= bytes_read;
        printf("Bytes read for checksum: %zd (Remaining: %ld)\n", bytes_read, bytes_to_read);
        printf("Chunk checksum: 0x%08x, Current calculated checksum: 0x%08x\n", chunk_checksum, calculated_checksum);
    }

    if (bytes_read == -1) {
        perror("Error reading data for checksum calculation");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    printf("Calculated checksum (before final XOR): 0x%08x\n", calculated_checksum);
    uint32_t intermediate_checksum = calculated_checksum;
    calculated_checksum = ~calculated_checksum;
    printf("Intermediate checksum: 0x%08x\n", intermediate_checksum);
    printf("Calculated checksum (after final XOR): 0x%08x\n", calculated_checksum);
    printf("Total bytes read for checksum: %zd\n", total_bytes_read);
    printf("Final checksum in hexadecimal: 0x%08x\n", calculated_checksum);
    printf("Final checksum in decimal: %u\n", calculated_checksum);

    // Seek back to the end of the file for writing the footer
    if (lseek(output_fd, 0, SEEK_END) == -1) {
        perror("Error seeking to end of file after checksum calculation");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // No need to seek to the end of the file, as we're already at the end after writing all the data

    // Prepare and write footer
    struct evo_footer footer;
    footer.checksum = calculated_checksum;
    printf("Preparing footer with checksum: %u\n", footer.checksum);

    // No need to seek to the end of the file, as we're already at the end after writing all the data

    ssize_t footer_bytes_written = write(output_fd, &footer, sizeof(footer));
    if (footer_bytes_written != sizeof(footer)) {
        perror("Error writing footer");
        printf("Expected to write %zu bytes, but wrote %zd bytes\n", sizeof(footer), footer_bytes_written);
        close(input_fd);
        close(output_fd);
        return 1;
    }
    off_t final_size = lseek(output_fd, 0, SEEK_END);
    if (final_size == -1) {
        perror("Error getting final file size");
        close(input_fd);
        close(output_fd);
        return 1;
    }
    printf("Footer written successfully. Final file size: %ld bytes\n", final_size);
    printf("Footer data: checksum=%u\n", footer.checksum);

    // Close files
    printf("Closing file descriptors. input_fd: %d, output_fd: %d\n", input_fd, output_fd);
    if (close(input_fd) == -1) {
        perror("Error closing input file");
    }
    if (close(output_fd) == -1) {
        perror("Error closing output file");
        return 1;
    }

    printf("Successfully created %s\n", output_file);
    return 0;
}

void parse_supported_architectures(evo_metadata *metadata, const char *architectures) {
    char *token = strtok((char *)architectures, ",");
    while (token != NULL && metadata->num_supported_architectures < EVO_MAX_ARCHITECTURES) {
        strncpy(metadata->supported_architectures[metadata->num_supported_architectures], token, EVO_MAX_NAME_LENGTH - 1);
        metadata->num_supported_architectures++;
        token = strtok(NULL, ",");
    }
}

void parse_required_permissions(evo_metadata *metadata, const char *permissions) {
    char *token = strtok((char *)permissions, ",");
    while (token != NULL && metadata->num_required_permissions < EVO_MAX_PERMISSIONS) {
        strncpy(metadata->required_permissions[metadata->num_required_permissions], token, EVO_MAX_PERMISSION_LENGTH - 1);
        metadata->num_required_permissions++;
        token = strtok(NULL, ",");
    }
}
