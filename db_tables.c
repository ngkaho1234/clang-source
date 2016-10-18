#include "db_lowlevel.h"
#include "db_tables.h"

#include "symbols.h"

const struct csdb_table_ops csdb_table_ops[CS_TABLE_MAX] = {
	CSDB_TABLE_ENTRY(
		CS_TABLE_SYMBOLS_ID,
		CS_TABLE_SYMBOLS_NAME,
		csdb_init_table_symbol)
};
