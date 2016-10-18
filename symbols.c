#include "symbols.h"

int csdb_init_table_symbol(
		csdb_t *handle, const struct csdb_table_ops *table)
{
	int ret;
	const char *sql_command =
		"CREATE TABLE IF NOT EXISTS " CS_TABLE_SYMBOLS_NAME
		" ( "
		"id integer PRIMARY KEY, "
		"usr text NOT NULL, "
		"kind integer NOT NULL, "
		"name text NOT NULL, "
		"type text, "
		"is_def integer NOT NULL, "
		"file text NOT NULL, "
		"start_line integer NOT NULL, "
		"start_col integer NOT NULL"
		" );";
	(void)table;

	ret = sqlite3_exec(handle, sql_command, NULL, NULL, NULL);
	if (ret != SQLITE_OK)
		return ret;

	ret = sqlite3_exec(
			handle,
			"CREATE INDEX IF NOT EXISTS idx_symbols_name ON " CS_TABLE_SYMBOLS_NAME " (name);",
			NULL, NULL, NULL);
	if (ret != SQLITE_OK)
		return ret;

	ret = sqlite3_exec(
			handle,
			"CREATE INDEX IF NOT EXISTS idx_symbols_file ON " CS_TABLE_SYMBOLS_NAME " (file);",
			NULL, NULL, NULL);
	return ret;
}

int csdb_symbol_add(csdb_t *handle, struct cs_symbol *sym)
{
	int ret;
	void *stmt;
	ret = csdb_prepare_query(handle, &stmt, CSDB_FMT_INSERT,
				"usr, kind, name, type, is_def, file, start_line, start_col",
				"?, ?, ?, ?, ?, ?, ?, ?");
	if (ret != CSDB_ERR_OK)
		return ret;

	
}
