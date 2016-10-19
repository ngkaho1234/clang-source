#include "db_lowlevel.h"
#include "db_tables.h"

#include "symbols.h"

static const struct csdb_table_ops csdb_table_ops[CS_TABLE_MAX] = {
	CSDB_TABLE_ENTRY(
		CS_TABLE_SYMBOLS_ID,
		CS_TABLE_SYMBOLS_NAME,
		csdb_init_table_symbol)
};

int csdb_create_prepare(csdb_t *handle)
{
	int i;
	int ret = 0;

	ret = csdb_start_txn(handle);
	if (ret != SQLITE_OK)
		return ret;

	for (i = 0; i < CS_TABLE_MAX; i++) {
		if (csdb_table_ops[i].t_init) {
			ret = csdb_table_ops[i].t_init(
							handle,
							&csdb_table_ops[i]);
			if (ret != SQLITE_OK)
				break;
		}
	}
	if (ret == SQLITE_OK)
		ret = csdb_commit_txn(handle);
	else
		csdb_rollback_txn(handle);

	return ret;
}

