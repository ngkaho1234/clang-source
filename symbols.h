#ifndef __CLANG_SOURCE_SYMBOLS_H__
#define __CLANG_SOURCE_SYMBOLS_H__

#include <stdint.h>

#include <clang-c/Index.h>

#include "db.h"

struct cs_symbol {
	const char *cs_file_path;
	const char *cs_symbol_name;
};

int csdb_init_table_symbol(
		csdb_t *handle, const struct csdb_table_ops *table);

#endif /* __CLANG_SOURCE_SYMBOLS_H__ */
