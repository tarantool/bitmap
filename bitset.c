#include "bitset.h"
#include <tarantool/module.h>
#include "msgpuck/msgpuck.h"

size_t get_header_len(uint64_t len) {
    if (len <= UINT8_MAX) {
        return 2;
    }
    if (len <= UINT16_MAX) {
        return 3;
    }
    return 5;
}

int lnew(lua_State *L) {
    uint64_t len = luaL_checkuint64(L, 1);

    size_t bin_header_size = get_header_len(len);
    size_t udata_size = sizeof(bitset_t) + (bin_header_size + len - 1) * sizeof(uint8_t);
    bitset_t *bitset = lua_newuserdata(L, udata_size);

    bitset->size = len;
    bitset->bin_header_size = bin_header_size;

    char *bitset_body = mp_encode_binl((char *) bitset->msgpack, len);
    memset(bitset_body, 0, len * sizeof(uint8_t));

    luaL_getmetatable(L, "bitset");
    lua_setmetatable(L, -2);

    return 1;
}

int lnew_from_string(lua_State *L) {
    size_t len;
    const char *str = lua_tolstring(L, 1, &len);

    size_t bin_header_size = get_header_len(len);
    size_t udata_size = sizeof(bitset_t) + (bin_header_size + len - 1) * sizeof(uint8_t);
    bitset_t *bitset = lua_newuserdata(L, udata_size);

    bitset->size = len;
    bitset->bin_header_size = bin_header_size;

    char *bitset_body = mp_encode_binl((char *) bitset->msgpack, len);
    memcpy(bitset_body, str, len);

    luaL_getmetatable(L, "bitset");
    lua_setmetatable(L, -2);

    return 1;
}

bitset_t *new_from_tuple(lua_State *L, box_tuple_t *tuple, uint64_t field_no) {
    const char *src_msgpack = box_tuple_field(tuple, field_no - 1);
    const char *src_msgpack_cursor = src_msgpack;
    uint64_t len = mp_decode_binl(&src_msgpack_cursor);
    uint64_t bin_header_size = src_msgpack_cursor - src_msgpack;

    size_t udata_size = sizeof(bitset_t) + (bin_header_size + len - 1) * sizeof(uint8_t);
    bitset_t *bitset = lua_newuserdata(L, udata_size);

    bitset->size = len;
    bitset->bin_header_size = bin_header_size;
    memcpy(bitset->msgpack, src_msgpack, bin_header_size + len);

    luaL_getmetatable(L, "bitset");
    lua_setmetatable(L, -2);

    return bitset;
}

int lnew_from_tuple(lua_State *L) {
    box_tuple_t *tuple = luaT_istuple(L, 1);
    uint64_t field_no = luaL_checkuint64(L, 2);

    box_tuple_ref(tuple);
    new_from_tuple(L, tuple, field_no);
    box_tuple_unref(tuple);

    return 1;
}

void bor_in_place(uint8_t *dst, const uint8_t *src, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) {
        dst[i] |= src[i];
    }
}

int lbor_in_place(lua_State *L) {
    bitset_t *dst_bitset = lua_touserdata(L, 1);
    bitset_t *src_bitset = lua_touserdata(L, 2);

    bor_in_place(dst_bitset->msgpack + dst_bitset->bin_header_size,
                 src_bitset->msgpack + src_bitset->bin_header_size,
                 src_bitset->size);

    return 0;
}

void bor_tuple_in_place(bitset_t *dst_bitset, box_tuple_t *src_tuple, uint64_t field_no) {
    const uint8_t *src = (const uint8_t *) box_tuple_field(src_tuple, field_no - 1);
    mp_decode_binl((const char **) &src); // decode binary length

    bor_in_place(dst_bitset->msgpack + dst_bitset->bin_header_size, src, dst_bitset->size);
}

int lbor_tuple_in_place(lua_State *L) {
    bitset_t *bitset = lua_touserdata(L, 1);
    box_tuple_t *tuple = luaT_istuple(L, 2);
    uint64_t field_no = luaL_checkuint64(L, 3);

    box_tuple_ref(tuple);
    bor_tuple_in_place(bitset, tuple, field_no);
    box_tuple_unref(tuple);

    return 0;
}

int lbor_uint_key(lua_State *L) {
    const int space_id_index = 1;
    const int index_id_index = 2;
    const int field_no_index = 3;
    const int keys_table_index = 4;

    const uint64_t space_id = luaL_checkuint64(L, space_id_index);
    const uint64_t index_id = luaL_checkuint64(L, index_id_index);
    const uint64_t field_no = luaL_checkuint64(L, field_no_index);
    if (!lua_istable(L, keys_table_index)) {
        fprintf(stderr, "%s\n", "third argument must be array");
        return 0;
    }

    size_t keys_table_len = lua_objlen(L, keys_table_index);
    if (keys_table_len < 1) {
        return 0;
    }

    lua_rawgeti(L, keys_table_index, 1);
    uint64_t id = luaL_checkuint64(L, -1);
    lua_pop(L, 1);

    const int key_msgpack_len = 10;  // max uint64 msgpack size + array header
    char key_msgpack[key_msgpack_len];
    char *key_part = mp_encode_array(key_msgpack, 1);
    mp_encode_uint(key_part, id);

    box_tuple_t *tuple;
    int err = box_index_get(space_id, index_id,
                            key_msgpack, key_msgpack + key_msgpack_len,
                            &tuple);
    if (err != 0) {
        fprintf(stderr, "%s\n", box_error_message(box_error_last()));
        return 0;
    }
    if (tuple == NULL) {
        fprintf(stderr, "%s\n", "not found");
        return 0;
    }

    box_tuple_ref(tuple);
    bitset_t *bitset = new_from_tuple(L, tuple, field_no); // pushes bitset to lua stack
    box_tuple_unref(tuple);

    for (size_t i = 2; i <= keys_table_len; ++i) {
        lua_rawgeti(L, keys_table_index, i);
        id = luaL_checkuint64(L, -1);
        lua_pop(L, 1);
        mp_encode_uint(key_part, id);

        err = box_index_get(space_id, index_id,
                            key_msgpack, key_msgpack + key_msgpack_len,
                            &tuple);
        if (err != 0) {
            fprintf(stderr, "%s\n", box_error_message(box_error_last()));
            return 0;
        }
        if (tuple == NULL) {
            fprintf(stderr, "%s\n", "not found");
            return 0;
        }

        box_tuple_ref(tuple);
        bor_tuple_in_place(bitset, tuple, field_no);
        box_tuple_unref(tuple);
    }

    return 1;
}

box_tuple_t *to_tuple(const bitset_t *bitset) {
    const uint8_t *tuple_body_begin = bitset->msgpack;
    const uint8_t *tuple_body_end = bitset->msgpack + bitset->bin_header_size + bitset->size;
    return box_tuple_new(box_tuple_format_default(),
                         (const char *) tuple_body_begin,
                         (const char *) tuple_body_end);
}

int lto_tuple(lua_State *L) {
    bitset_t *bitset = lua_touserdata(L, 1);
    box_tuple_t *tuple = to_tuple(bitset);
    luaT_pushtuple(L, tuple);
    return 1;
}

int lto_string(lua_State *L) {
    bitset_t *bitset = lua_touserdata(L, 1);
    lua_pushlstring(L, (const char *) bitset->msgpack + bitset->bin_header_size, bitset->size);
    return 1;
}
