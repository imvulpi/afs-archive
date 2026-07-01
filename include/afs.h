#ifdef AFS_NO_STD
    // STANDALONE ENVIRONMENT
    typedef unsigned char afs_uint8;
    
    #if defined(__INT_MAX__) && (__INT_MAX__ == 32767)
        typedef unsigned int afs_uint16;
    #else
        typedef unsigned short afs_uint16;
    #endif

    #if defined(__INT_MAX__) && (__INT_MAX__ == 2147483647)
        typedef unsigned int afs_uint32;
    #else
        typedef unsigned long afs_uint32;
    #endif

    typedef unsigned long long afs_uint64;

    #if defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8)
        typedef unsigned long long afs_size_t;
    #else
        typedef unsigned long afs_size_t;
    #endif

    #define AFS_NULL ((void*)0)
#else
    #include <stdint.h>
    #include <stddef.h>
    
    typedef uint64_t afs_uint64;
    typedef uint32_t afs_uint32;
    typedef uint16_t afs_uint16;
    typedef uint8_t  afs_uint8;
    typedef size_t   afs_size_t;
    
    #define AFS_NULL NULL
#endif

typedef afs_size_t (*AfsReadFn)(void *context, void *buffer, afs_size_t bytes_to_read);

typedef struct {
    void *context;
    AfsReadFn read;
} AfsStream;

typedef struct
{
    afs_uint32 magic;
    afs_uint16 version;
    afs_uint16 flags;
    afs_extension_block* extension_block;
} afs_header;

typedef struct
{
    afs_uint16 count;
    afs_extension* extensions;
} afs_extension_block;

typedef struct
{
    afs_uint16 id;
    afs_uint32 size;
    afs_uint8* data;
} afs_extension;

typedef struct
{
    afs_uint16 bitmask;
    char* path;
    afs_uint64 file_size;
    afs_uint32 permissions;
    afs_uint8 date_bitmask;
    afs_uint32 dates;
    afs_extension_block* extensions;
} afs_entry_t;
