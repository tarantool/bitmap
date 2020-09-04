# Bitmap for Tarantool
Special data type for bitmaps and bitwise operations. Several operations perform bitwise OR|AND|XOR operation between multiple tuples/bitmaps and store the result in the bitmap instance or destination tuple. 

![Test bitmap](https://github.com/tarantool/bitmap/workflows/Test%20bitmap/badge.svg)

## Requirements
- git
- make
- cmake >= 2.8
- c++11 compiler
- SSE4, AVX (>= Sandy Bridge or >= Bulldozer)

## API
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
