# Bitmap for Tarantool
![Test bitmap](https://github.com/tarantool/bitmap/workflows/Test%20bitmap/badge.svg)

Special data type for bitmaps and bitwise operations. Several operations perform bitwise OR|AND|XOR operation between multiple tuples/bitmaps and store the result in the bitmap instance or destination tuple. 

Stores bitmaps as MessagePack binary fields.

**Not recommended for production use.**

## Requirements
- git
- make
- cmake >= 2.8
- c++11 compiler
- SSE4, AVX (>= Sandy Bridge or >= Bulldozer)

## Installation
```bash
tarantoolctl rocks install https://raw.githubusercontent.com/tarantool/bitmap/master/bitmap-scm-1.rockspec
```

## Examples
[More examples in tests](https://github.com/tarantool/bitmap/blob/master/test/bitset_test.lua)

### Batch bitwise operations on spaces by uint keys
Bitwise OR example
```lua
local bitmap = require('bitset')

box.cfg{}
box.schema.create_space('space1')
box.space.space1:create_index('pk', { parts = { { 1, 'integer' } } })

local bs1 = bitmap.new_from_string('\xFF\x00\x00\x00\xFF\x00\x00\x00')
box.space.space1:insert {
    3,              -- id
    bs1:to_tuple(), -- bitmap
}

local bs2 = bitmap.new_from_string('\x00\x00\xFF\x00\x00\x00\xFF\x00')
box.space.space1:insert {
    4,              -- id
    bs2:to_tuple(), -- bitmap
}

local result = bitmap.bor_uint_keys(box.space.space1.id, -- space id
                                    0,                   -- index id
                                    2,                   -- bitmap field index
                                    { 3, 4 })            -- keys
print(result) -- \xFF\x00\xFF\x00\xFF\x00\xFF\x00
```

### Batch bitwise operations on spaces by iterator
Bitwise OR example
```lua
local bitmap = require('bitset')

box.cfg{}
box.schema.create_space('space2')
box.space.space2:create_index('pk', { parts = { { 1, 'integer' } } })
box.space.space2:create_index('secondary', { parts = { { 2, 'integer' } }, unique = false })

local bs1 = bitmap.new_from_string('\xFF\x00\x00\x00\xFF\x00\x00\x00')
box.space.space2:insert {
    1,             -- primary key
    1,             -- secondary key
    bs1:to_tuple() -- bitmap field
}

local bs2 = bitmap.new_from_string('\x00\x00\xFF\x00\x00\x00\xFF\x00')
box.space.space2:insert {
    2,              -- primary key
    1,              -- secondary key
    bs2:to_tuple(), -- bitmap field
}

local result = bitmap.bor_uint_iter(box.space.space2.id,                 -- space id
                                    box.space.space2.index.secondary.id, -- index id
                                    1,                                   -- key
                                    3,                                   -- bitmap field index
                                    box.index.EQ)                        -- iterator type
print(result) -- \xFF\x00\xFF\x00\xFF\x00\xFF\x00
```

### Bitwise operations bitmap - bitmap
In-place bitwise OR example
```lua
local bitmap = require('bitset')

local bs1 = bitmap.new_from_string('\xFF\x00\xFF\x00\xFF\x00\x00\x00')
local bs2 = bitmap.new_from_string('\x00\x00\xFF\x00\xFF\x00\xFF\x00')

bs1:bor_in_place(bs2) -- argument is another bitmap
print(bs1:tostring()) -- \xFF\x00\xFF\x00\xFF\x00\xFF\x00
```

### Bitwise operations bitmap - tuple
In-place bitwise OR example
```lua
local bitmap = require('bitset')

box.cfg{}
box.schema.create_space('space1')
box.space.space1:create_index('pk', { parts = { { 1, 'integer' } } })

local bs1 = bitmap.new_from_string('\xFF\x00\x00\x00\xFF\x00\x00\x00')
local bs2 = bitmap.new_from_string('\x00\x00\xFF\x00\x00\x00\xFF\x00')

box.space.space1:insert {
    2,              -- id
    bs2:to_tuple(), -- bitmap
}
local bs2_tuple = box.space.space1:get(2)

bs1:bor_tuple_in_place(bs2_tuple, -- tuple
                       2)         -- bitmap field index
print(bs1:tostring()) -- \xFF\x00\xFF\x00\xFF\x00\xFF\x00
```

## Module reference
### `bitmap.new(byte_count)`
Creates bitmap with size in bytes

### `bitmap.new_from_string(string)`
Creates bitmap from string

### `bitmap.new_from_tuple(tuple, field_no)`
Creates bitmap from tuple binary field

### `bitmap.bor_uint_keys(space_id, index_id, field_no, keys)`
Bitwise OR tuples by uint keys

### `bitmap.band_uint_keys(space_id, index_id, field_no, keys)`
Bitwise AND tuples by uint keys

### `bitmap.bxor_uint_keys(space_id, index_id, field_no, keys)`
Bitwise XOR tuples by uint keys

### `bitmap.set_bit_in_tuple_uint_key(space_id, index_id, key, field_no, bit_index)`
Sets bit in tuple field

### `bitmap.bor_uint_iter(space_id, index_id, key, field_no, iterator)`
Bitwise OR tuples with iterator by uint key

### `bitmap.band_uint_iter(space_id, index_id, key, field_no, iterator)`
Bitwise AND tuples with iterator by uint key

### `bitmap.bxor_uint_iter(space_id, index_id, key, field_no, iterator)`
Bitwise XOR tuples with iterator by uint key

## Bitmap class method reference
### `bitmap:bor_in_place(bitmap)`
In-place bitwise OR bitmap with another

### `bitmap:band_in_place(bitmap)`
In-place bitwise AND bitmap with another

### `bitmap:bxor_in_place(bitmap)`
In-place bitwise XOR bitmap with another

### `bitmap:bor_tuple_in_place(tuple, field_no)`
In-place bitwise OR bitmap with tuple binary field

### `bitmap:band_tuple_in_place(tuple, field_no)`
In-place bitwise AND bitmap with tuple binary field

### `bitmap:bxor_tuple_in_place(tuple, field_no)`
In-place bitwise XOR bitmap with tuple binary field

### `bitmap:to_tuple()`
Casts bitmap to Tarantool tuple

### `bitmap:to_string()`
Casts bitmap to string

### `bitmap:get_bit(index)`
Gets bit by index

### `bitmap:set_bit(index, value)`
Sets bit by index to given value

### `bitmap:all()`
Checks if all bits are set to true

### `bitmap:any()`
Checks if any bits are set to true

### `bitmap:none()`
Checks if all bits are set to false

### `bitmap:count()`
Returns the number of bits set to true

### `bitmap:set()`
Sets bits to true
 
### `bitmap:reset()`
Sets bits to false
