#ifndef EVO_FORMAT_H
#define EVO_FORMAT_H

#include <stdint.h>

#define EVO_MAGIC "EVOFILE"

struct evo_header {
    char magic[8];        // Magic number for .evo files
    uint32_t version;     // Version of the .evo format
    uint32_t metadata_size; // Size of metadata section
    uint64_t data_size;   // Size of the data section
};

struct evo_footer {
    uint32_t checksum;    // Checksum for data integrity
};

#endif // EVO_FORMAT_H
