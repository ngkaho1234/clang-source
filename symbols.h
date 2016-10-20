#ifndef __CLANG_SOURCE_SYMBOLS_H__
#define __CLANG_SOURCE_SYMBOLS_H__

#include <stdbool.h>
#include <stdint.h>

#include <clang-c/Index.h>

#include "db_lowlevel.h"
#include "db_tables.h"

struct cs_symbol {
	const char	*cs_usr;
	uint32_t	 cs_kind;
	const char	*cs_symbol_name;
	uint32_t	 cs_type;
	bool		 cs_is_def;
	const char	*cs_file_path;
	uint32_t	 cs_start_line;
	uint32_t	 cs_start_col;
};

int csdb_init_table_symbol(
		csdb_t *handle, const struct csdb_table_ops *table);

#endif /* __CLANG_SOURCE_SYMBOLS_H__ */
