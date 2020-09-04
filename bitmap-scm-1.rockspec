package = 'bitmap'
version = 'scm-1'
source  = {
    url    = 'git://github.com/tarantool/bitmap.git',
    branch = 'master',
}
description = {
    summary  = "BitMap for Tarantool",
    homepage = 'https://github.com/tarantool/bitmap/',
    license  = 'MIT',
}
dependencies = {
    'lua >= 5.1',
    'checks >= 3.0.1',
}
external_dependencies = {
    TARANTOOL = {
        header = "tarantool/module.h"
    },
}
build = {
    type = 'cmake',

    variables = {
        version = 'scm-1',
        CMAKE_BUILD_TYPE = 'Release',
        TARANTOOL_DIR = '$(TARANTOOL_DIR)',
        TARANTOOL_INSTALL_LIBDIR = '$(LIBDIR)',
        TARANTOOL_INSTALL_LUADIR = '$(LUADIR)',
    }
}

-- vim: syntax=lua