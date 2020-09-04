#include <tarantool/module.h>
#include <cinttypes>
#include <cstring>
#include "msgpuck/msgpuck.h"

#include "bitmap.h"

#define popcount __builtin_popcount


int luaopen_bitmap(lua_State *L) {
    luaL_newmetatable(L, libname);

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    luaL_setfuncs(L, bitmap_m, 0);
    luaL_register(L, libname, bitmap_f);

    return 1;
}


inline bitmap_t *check_bitmap(lua_State *L, int idx) {
    return static_cast<bitmap_t *>(luaL_checkudata(L, idx, libname));
}


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
    size_t udata_size = sizeof(bitmap_t) + (bin_header_size + len - 1) * sizeof(uint8_t);
    bitmap_t *bitmap = (bitmap_t *) lua_newuserdata(L, udata_size);

    bitmap->size = len;
    bitmap->bin_header_size = bin_header_size;

    char *bitmap_body = mp_encode_binl((char *) bitmap->msgpack, len);
    memset(bitmap_body, 0, len * sizeof(uint8_t));

    luaL_getmetatable(L, libname);
    lua_setmetatable(L, -2);

    return 1;
}


int lnew_from_string(lua_State *L) {
    size_t len;
    const char *str = lua_tolstring(L, 1, &len);

    size_t bin_header_size = get_header_len(len);
    size_t udata_size = sizeof(bitmap_t) + (bin_header_size + len - 1) * sizeof(uint8_t);
    bitmap_t *bitmap = (bitmap_t *) lua_newuserdata(L, udata_size);

    bitmap->size = len;
    bitmap->bin_header_size = bin_header_size;

    char *bitmap_body = mp_encode_binl((char *) bitmap->msgpack, len);
    memcpy(bitmap_body, str, len);

    luaL_getmetatable(L, libname);
    lua_setmetatable(L, -2);

    return 1;
}


bitmap_t *new_from_tuple(lua_State *L, box_tuple_t *tuple, uint64_t field_no) {
    const char *src_msgpack = box_tuple_field(tuple, field_no);
    if (src_msgpack == nullptr) {
        return nullptr;
    }
    const char *src_msgpack_cursor = src_msgpack;
    // TODO handle wrong type
    uint64_t len = mp_decode_binl(&src_msgpack_cursor);
    uint64_t bin_header_size = src_msgpack_cursor - src_msgpack;

    size_t udata_size = sizeof(bitmap_t) + (bin_header_size + len - 1) * sizeof(uint8_t);
    bitmap_t *bitmap = (bitmap_t *) lua_newuserdata(L, udata_size);

    bitmap->size = len;
    bitmap->bin_header_size = bin_header_size;
    memcpy(bitmap->msgpack, src_msgpack, bin_header_size + len);

    luaL_getmetatable(L, libname);
    lua_setmetatable(L, -2);

    return bitmap;
}


int lnew_from_tuple(lua_State *L) {
    box_tuple_t *tuple = luaT_istuple(L, 1);
    if (tuple == nullptr) {
        return luaL_error(L, "Usage bitmap.new_from_tuple(tuple, field_no)");
    }
    uint64_t field_no = luaL_checkuint64(L, 2);

    box_tuple_ref(tuple);
    new_from_tuple(L, tuple, field_no - 1);
    box_tuple_unref(tuple);

    return 1;
}


template<Op op>
int lbitwise_in_place(lua_State *L) {
    bitmap_t *dst_bitmap = check_bitmap(L, 1);
    bitmap_t *src_bitmap = check_bitmap(L, 2);

    op(dst_bitmap->msgpack + dst_bitmap->bin_header_size,
       src_bitmap->msgpack + src_bitmap->bin_header_size,
       src_bitmap->size);

    return 0;
}


template<Op op>
int bitwise_tuple_in_place(bitmap_t *dst_bitmap, box_tuple_t *src_tuple, uint64_t field_no) {
    const auto *src = (const uint8_t *) box_tuple_field(src_tuple, field_no);
    if (src == nullptr) {
        return 1;
    }

    // TODO handle wrong type
    mp_decode_binl((const char **) &src); // decode binary length

    op(dst_bitmap->msgpack + dst_bitmap->bin_header_size, src, dst_bitmap->size);
    return 0;
}

template<Op op>
int lbitwise_tuple_in_place(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    box_tuple_t *tuple = luaT_istuple(L, 2);
    uint64_t field_no = luaL_checkuint64(L, 3);

    box_tuple_ref(tuple);
    int err = bitwise_tuple_in_place<op>(bitmap, tuple, field_no - 1);
    box_tuple_unref(tuple);

    if (err != 0) {
        return luaL_error(L, "Wrong field index");
    }

    return 0;
}


template<Op op>
int lbitwise_uint_keys(lua_State *L) {
    const int space_id_index = 1;
    const int index_id_index = 2;
    const int field_no_index = 3;
    const int keys_table_index = 4;

    const uint64_t space_id = luaL_checkuint64(L, space_id_index);
    const uint64_t index_id = luaL_checkuint64(L, index_id_index);
    const uint64_t field_no = luaL_checkuint64(L, field_no_index);
    if (!lua_istable(L, keys_table_index)) {
        return luaL_error(L,
                          "Usage bitmap.bor_uint_keys(space_id, index_id, field_no, keys_table)");
    }

    size_t keys_table_len = lua_objlen(L, keys_table_index);
    if (keys_table_len < 1) {
        return luaL_error(L,
                          "Usage bitmap.bor_uint_keys(space_id, index_id, field_no, keys_table)");
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
        return luaT_error(L);
    }
    if (tuple == nullptr) {
        return luaL_error(L, "Tuple with key %" PRIu64 " not found", id);
    }

    box_tuple_ref(tuple);
    bitmap_t *bitmap = new_from_tuple(L, tuple, field_no - 1); // pushes bitmap to lua stack
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
            return luaT_error(L);
        }
        if (tuple == nullptr) {
            return luaL_error(L, "Tuple with key %" PRId64 " not found", id);
        }

        box_tuple_ref(tuple);
        bitwise_tuple_in_place<op>(bitmap, tuple, field_no - 1);
        box_tuple_unref(tuple);
    }

    return 1;
}


template<Op op>
int lbitwise_uint_iter(lua_State *L) {
    const int space_id_index = 1;
    const int index_id_index = 2;
    const int key_index = 3;
    const int field_no_index = 4;
    const int iterator_type_index = 5;

    const uint64_t space_id = luaL_checkuint64(L, space_id_index);
    const uint64_t index_id = luaL_checkuint64(L, index_id_index);
    const uint64_t key = luaL_checkuint64(L, key_index);
    const uint64_t field_no = luaL_checkuint64(L, field_no_index);
    const uint64_t iterator_type = luaL_checkuint64(L, iterator_type_index);

    const int key_msgpack_len = 10;  // max uint64 msgpack size + array header
    char key_msgpack[key_msgpack_len];
    char *key_part = mp_encode_array(key_msgpack, 1);
    mp_encode_uint(key_part, key);

    box_iterator_t *iter = box_index_iterator(space_id, index_id, iterator_type,
                                              key_msgpack, key_msgpack + key_msgpack_len);
    if (iter == nullptr) {
        box_iterator_free(iter);
        return luaT_error(L);
    }

    box_tuple_t *tuple;
    int err = box_iterator_next(iter, &tuple);
    if (err != 0) {
        box_iterator_free(iter);
        return luaT_error(L);
    }
    if (tuple == nullptr) {
        box_iterator_free(iter);
        return luaL_error(L, "Tuple with key %" PRIu64 " not found", key);
    }

    box_tuple_ref(tuple);
    bitmap_t *bitmap = new_from_tuple(L, tuple, field_no - 1); // pushes bitmap to lua stack
    box_tuple_unref(tuple);

    while (true) {
        err = box_iterator_next(iter, &tuple);
        if (err != 0) {
            box_iterator_free(iter);
            return luaT_error(L);
        }
        if (tuple == nullptr) {
            break;
        }

        box_tuple_ref(tuple);
        bitwise_tuple_in_place<op>(bitmap, tuple, field_no - 1);
        box_tuple_unref(tuple);
    }

    return 1;
}


box_tuple_t *to_tuple(const bitmap_t *bitmap) {
    const uint8_t *tuple_body_begin = bitmap->msgpack;
    const uint8_t *tuple_body_end = bitmap->msgpack + bitmap->bin_header_size + bitmap->size;
    return box_tuple_new(box_tuple_format_default(),
                         (const char *) tuple_body_begin,
                         (const char *) tuple_body_end);
}

int lto_tuple(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    box_tuple_t *tuple = to_tuple(bitmap);
    luaT_pushtuple(L, tuple);
    return 1;
}


int lto_string(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    lua_pushlstring(L, (const char *) bitmap->msgpack + bitmap->bin_header_size, bitmap->size);
    return 1;
}


void set_bit(uint8_t *bitmap_raw, uint64_t index, bool value) {
    const uint64_t byte_index = index / 8;
    const uint8_t bit_index = index % 8;
    if (value) {
        bitmap_raw[byte_index] |= 0x01 << bit_index;
    } else {
        bitmap_raw[byte_index] &= ~(0x01 << bit_index);
    }
}

int lset_bit(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    uint64_t index = luaL_checkuint64(L, 2);
    if (!lua_isboolean(L, 3)) {
        return luaL_error(L, "Usage: bitmap.set(u64 index, bool value)");
    }
    bool value = lua_toboolean(L, 3);

    if (index < 1 || index > bitmap->size * 8) {
        return luaL_error(L, "Index must be greater than 0"
                             " and less than or equal to the size of the array");
    }
    uint8_t *bitmap_raw = bitmap->msgpack + bitmap->bin_header_size;
    set_bit(bitmap_raw, index - 1, value);

    lua_pushboolean(L, value);
    return 1;
}


bool get_bit(const uint8_t *bitmap_raw, uint64_t index) {
    const uint64_t byte_index = index / 8;
    const uint8_t bit_index = index % 8;
    return (bitmap_raw[byte_index] >> bit_index) & 0x01;
}

int lget_bit(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    uint64_t index = luaL_checkuint64(L, 2);
    if (index < 1 || index > bitmap->size * 8) {
        return luaL_error(L, "Index must be greater than 0"
                             " and less than or equal to the size of the array");
    }

    const uint8_t *bitmap_raw = bitmap->msgpack + bitmap->bin_header_size;
    bool bit = get_bit(bitmap_raw, index - 1);

    lua_pushboolean(L, bit);
    return 1;
}


bool all(const uint8_t *bs, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) {
        if (bs[i] != 0xFF) {
            return false;
        }
    }
    return true;
}

int lall(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    lua_pushboolean(L, all(bitmap->msgpack + bitmap->bin_header_size, bitmap->size));
    return 1;
}


bool any(const uint8_t *bs, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) {
        if (bs[i] != 0x00) {
            return true;
        }
    }
    return false;
}

int lany(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    lua_pushboolean(L, any(bitmap->msgpack + bitmap->bin_header_size, bitmap->size));
    return 1;
}


bool none(const uint8_t *bs, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) {
        if (bs[i] != 0x00) {
            return false;
        }
    }
    return true;
}

int lnone(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    lua_pushboolean(L, none(bitmap->msgpack + bitmap->bin_header_size, bitmap->size));
    return 1;
}


void set(uint8_t *bs, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) {
        bs[i] = 0xFF;
    }
}

int lset(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    set(bitmap->msgpack + bitmap->bin_header_size, bitmap->size);
    return 0;
}


void reset(uint8_t *bs, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) {
        bs[i] = 0x00;
    }
}

int lreset(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    reset(bitmap->msgpack + bitmap->bin_header_size, bitmap->size);
    return 0;
}


uint64_t count(const uint8_t *bs, uint64_t len) {
    uint64_t counter = 0;
    for (uint64_t i = 0; i < len; ++i) {
        counter += popcount(bs[i]);
    }
    return counter;
}

int lcount(lua_State *L) {
    bitmap_t *bitmap = check_bitmap(L, 1);
    luaL_pushuint64(L, count(bitmap->msgpack + bitmap->bin_header_size, bitmap->size));
    return 1;
}

int lset_bit_in_tuple_uint_key(lua_State *L) {
    const uint64_t space_id = luaL_checkuint64(L, 1);
    const uint64_t index_id = luaL_checkuint64(L, 2);
    const uint64_t key = luaL_checkuint64(L, 3);
    const uint64_t field_no = luaL_checkuint64(L, 4);
    const uint64_t bit_index = luaL_checkuint64(L, 5);
    if (!lua_isboolean(L, 6)) {
        return luaL_error(L, "Usage: bitmap.set_bit_in_tuple_uint_key"
                             "(space_id, index_id, key, field_no, bit_index, value)");
    }
    bool value = lua_toboolean(L, 6);

    const int key_msgpack_len = 10;  // max uint64 msgpack size + array header
    char key_msgpack[key_msgpack_len];
    char *key_part = mp_encode_array(key_msgpack, 1);
    mp_encode_uint(key_part, key);

    box_txn_begin();
    box_tuple_t *tuple;
    int err = box_index_get(space_id, index_id,
                            key_msgpack, key_msgpack + key_msgpack_len,
                            &tuple);
    if (err != 0) {
        box_txn_rollback();
        return luaT_error(L);
    }
    if (tuple == nullptr) {
        box_txn_rollback();
        return luaL_error(L, "Tuple with key %u not found", key);
    }

    box_tuple_ref(tuple);

    const char *bitmap_msgpack = box_tuple_field(tuple, field_no - 1);
    if (bitmap_msgpack == nullptr) {
        box_txn_rollback();
        return luaL_error(L, "Invalid field index");
    }

    const char *bitmap_msgpack_end = bitmap_msgpack;
    uint32_t bitmap_len;
    const char *bitmap_raw = mp_decode_bin(&bitmap_msgpack_end, &bitmap_len);
    uint32_t bin_header_len = bitmap_raw - bitmap_msgpack;
    uint32_t bitmap_msgpack_len = bitmap_msgpack_end - bitmap_msgpack;

    uint32_t ops_len =
        1 // ops array header
        + 1 // op array header
        + 2 // op string with header
        + 9 // field_no with header (max possible)
        + bitmap_msgpack_len; // bitmap with header


    uint8_t *ops = (uint8_t *) box_txn_alloc(ops_len);
    uint8_t *ops_end = ops;
    ops_end = (uint8_t *) mp_encode_array((char *) ops_end, 1); // ops array header
    ops_end = (uint8_t *) mp_encode_array((char *) ops_end, 3); // op array header
    ops_end = (uint8_t *) mp_encode_str((char *) ops_end, "=", 1); // op string with header
    ops_end = (uint8_t *) mp_encode_uint((char *) ops_end, field_no - 1); // field_no with header
    std::memcpy(ops_end, bitmap_msgpack, bitmap_msgpack_len); // bitmap with header

    box_tuple_unref(tuple);

    set_bit(ops_end + bin_header_len, bit_index - 1, value);
    ops_end += bitmap_msgpack_len;

    err = box_update(space_id, index_id,
                     key_msgpack, key_msgpack + key_msgpack_len,
                     (const char *) ops, (const char *) ops_end,
                     0, &tuple);
    if (err != 0) {
        box_txn_rollback();
        return luaT_error(L);
    }

    box_txn_commit();
    return 0;
}
