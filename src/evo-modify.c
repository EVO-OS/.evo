#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include "evo_format.h"
#include "evo_metadata.h"

#define BUFFER_SIZE 4096
#define MAX_LINE_LENGTH 1024
#define MIN(a,b) ((a) < (b) ? (a) : (b))

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



void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s --input <input_file.evo> --changes <changes_file> --output <output_file.evo>\n", program_name);
}



int apply_changes(evo_metadata *metadata, const char *changes_file) {
    FILE *file = fopen(changes_file, "r");
    if (!file) {
        perror("Error opening changes file");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");

        if (key && value && *value) {
            if (strcmp(key, "name") == 0) {
                strncpy(metadata->name, value, EVO_MAX_NAME_LENGTH - 1);
                metadata->name[EVO_MAX_NAME_LENGTH - 1] = '\0';
            } else if (strcmp(key, "version") == 0) {
                strncpy(metadata->version, value, EVO_MAX_VERSION_LENGTH - 1);
                metadata->version[EVO_MAX_VERSION_LENGTH - 1] = '\0';
            } else if (strcmp(key, "description") == 0) {
                strncpy(metadata->description, value, EVO_MAX_DESCRIPTION_LENGTH - 1);
                metadata->description[EVO_MAX_DESCRIPTION_LENGTH - 1] = '\0';
            } else if (strcmp(key, "architecture") == 0) {
                metadata->architecture = atoi(value);
            } else if (strcmp(key, "installed_size") == 0) {
                metadata->installed_size = strtoull(value, NULL, 10);
            } else if (strcmp(key, "maintainer") == 0) {
                strncpy(metadata->maintainer, value, EVO_MAX_NAME_LENGTH - 1);
                metadata->maintainer[EVO_MAX_NAME_LENGTH - 1] = '\0';
            } else if (strcmp(key, "package_type") == 0) {
                metadata->package_type = atoi(value);
            }
            // Add more fields as needed
        }
    }

    fclose(file);
    return 0;
}

int main(int argc, char *argv[]) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char *input_file = NULL;
    char *changes_file = NULL;
    char *output_file = NULL;

    // Parse command-line arguments
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            print_usage(argv[0]);
            return 1;
        }
        if (strcmp(argv[i], "--input") == 0) {
            input_file = argv[i + 1];
        } else if (strcmp(argv[i], "--changes") == 0) {
            changes_file = argv[i + 1];
        } else if (strcmp(argv[i], "--output") == 0) {
            output_file = argv[i + 1];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_file == NULL || changes_file == NULL || output_file == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    // Open input file
    int input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        perror("Error opening input file");
        return 1;
    }

    // Get input file size
    struct stat input_stat;
    if (fstat(input_fd, &input_stat) == -1) {
        perror("Error getting input file size");
        close(input_fd);
        return 1;
    }
    off_t input_file_size = input_stat.st_size;

    // Read header
    struct evo_header header;
    if (read(input_fd, &header, sizeof(header)) != sizeof(header)) {
        perror("Error reading header");
        close(input_fd);
        return 1;
    }

    // Verify magic number
    if (memcmp(header.magic, EVO_MAGIC, sizeof(header.magic)) != 0) {
        fprintf(stderr, "Invalid EVO file format\n");
        close(input_fd);
        return 1;
    }

    // Read metadata
    evo_metadata metadata;
    if (read(input_fd, &metadata, sizeof(metadata)) != sizeof(metadata)) {
        perror("Error reading metadata");
        close(input_fd);
        return 1;
    }

    // Read old checksum from footer
    struct evo_footer old_footer;
    if (lseek(input_fd, -sizeof(struct evo_footer), SEEK_END) == -1 ||
        read(input_fd, &old_footer, sizeof(old_footer)) != sizeof(old_footer)) {
        perror("Error reading old checksum");
        close(input_fd);
        return 1;
    }

    // Apply changes
    if (apply_changes(&metadata, changes_file) != 0) {
        close(input_fd);
        return 1;
    }

    // Open output file
    int output_fd = open(output_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1) {
        perror("Error opening output file");
        close(input_fd);
        return 1;
    }

    // Write header
    if (write(output_fd, &header, sizeof(header)) != sizeof(header)) {
        perror("Error writing header");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Write updated metadata
    if (write(output_fd, &metadata, sizeof(metadata)) != sizeof(metadata)) {
        perror("Error writing metadata");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Copy the rest of the input file content to the output file
    off_t remaining_bytes = input_file_size - sizeof(struct evo_header) - sizeof(evo_metadata) - sizeof(struct evo_footer);

    while (remaining_bytes > 0 && (bytes_read = read(input_fd, buffer, MIN(BUFFER_SIZE, remaining_bytes))) > 0) {
        if (write(output_fd, buffer, bytes_read) != bytes_read) {
            perror("Error writing data to output file");
            close(input_fd);
            close(output_fd);
            return 1;
        }
        remaining_bytes -= bytes_read;
    }

    if (bytes_read == -1) {
        perror("Error reading data from input file");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Calculate checksum for the entire file content, excluding the footer
    uint32_t calculated_checksum = 0xffffffff;
    off_t file_size = lseek(output_fd, 0, SEEK_END);
    if (file_size == -1) {
        perror("Error getting file size");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Reset output file pointer to the beginning for checksum calculation
    if (lseek(output_fd, 0, SEEK_SET) == -1) {
        perror("Error seeking to beginning of output file for checksum calculation");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    off_t bytes_to_read = file_size - sizeof(struct evo_footer);
    while (bytes_to_read > 0 && (bytes_read = read(output_fd, buffer, MIN(BUFFER_SIZE, bytes_to_read))) > 0) {
        calculated_checksum = crc32(buffer, bytes_read) ^ calculated_checksum;
        bytes_to_read -= bytes_read;
    }

    if (bytes_read == -1) {
        perror("Error reading data for checksum calculation");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    calculated_checksum = ~calculated_checksum;

    // Prepare and write footer
    struct evo_footer footer;
    footer.checksum = calculated_checksum;

    if (lseek(output_fd, 0, SEEK_END) == -1) {
        perror("Error seeking to end of file for footer writing");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    if (write(output_fd, &footer, sizeof(footer)) != sizeof(footer)) {
        perror("Error writing footer");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Close files
    close(input_fd);
    close(output_fd);

    printf("Successfully modified %s\n", output_file);
    printf("Old checksum: 0x%08X (%u)\n", old_footer.checksum, old_footer.checksum);
    printf("New checksum: 0x%08X (%u)\n", calculated_checksum, calculated_checksum);
    return 0;
}
