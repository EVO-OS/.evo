#ifndef EVO_METADATA_H
#define EVO_METADATA_H

#include <stdint.h>

#define EVO_MAX_NAME_LENGTH 256
#define EVO_MAX_VERSION_LENGTH 64
#define EVO_MAX_DESCRIPTION_LENGTH 1024
#define EVO_MAX_DEPENDENCIES 100
#define EVO_MAX_DEPENDENCY_LENGTH 256
#define EVO_MAX_ARCHITECTURES 10
#define EVO_MAX_PERMISSIONS 50
#define EVO_MAX_PERMISSION_LENGTH 128

typedef struct {
    uint32_t width;
    uint32_t height;
} screen_size;

typedef struct {
    char name[EVO_MAX_NAME_LENGTH];
    char version[EVO_MAX_VERSION_LENGTH];
    char description[EVO_MAX_DESCRIPTION_LENGTH];
    uint32_t architecture;
    uint64_t installed_size;
    char maintainer[EVO_MAX_NAME_LENGTH];
    char dependencies[EVO_MAX_DEPENDENCIES][EVO_MAX_DEPENDENCY_LENGTH];
    uint32_t num_dependencies;
    uint32_t package_type; // 0 for generic, 1 for deb-like, 2 for apk-like

    // Mobile-specific fields
    char supported_architectures[EVO_MAX_ARCHITECTURES][EVO_MAX_NAME_LENGTH];
    uint32_t num_supported_architectures;
    screen_size min_screen_size;
    char required_permissions[EVO_MAX_PERMISSIONS][EVO_MAX_PERMISSION_LENGTH];
    uint32_t num_required_permissions;
    uint32_t target_sdk_version;
    char min_os_version[EVO_MAX_VERSION_LENGTH];
} evo_metadata;

#endif // EVO_METADATA_H
