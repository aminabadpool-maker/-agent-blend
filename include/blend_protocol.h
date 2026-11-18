#ifndef BLEND_PROTOCOL_H
#define BLEND_PROTOCOL_H

#include <stdint.h>

#define BLEND_MAGIC 0x424C4E44 // "BLND"
#define BLEND_VERSION 1

typedef enum {
    BLEND_CMD_PING = 0x01,
    BLEND_CMD_EXEC = 0x02,
    BLEND_CMD_RENDER = 0x03
} blend_cmd_type_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t cmd_type;
    uint32_t payload_len;
} blend_header_t;
#pragma pack(pop)

#endif
