#include <string.h>

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

/*
 * Add file, symbols/Delete symbols, file/Query
 */

int cs_symbol_add(csdb_t *handle, struct cs_symbol *sym)
{
	int i, nr_columns;
	int ret;
	void *stmt;
	csdb_column_t columns[8];
	ret = csdb_prepare_query(handle, &stmt, CSDB_FMT_INSERT,
				"usr, kind, name, type, is_def, file, start_line, start_col",
				"?, ?, ?, ?, ?, ?, ?, ?");
	if (ret != CSDB_ERR_OK)
		return ret;

	columns[0].type = CSDB_TYPE_TEXT;
	columns[0].ptr.data = sym->cs_usr;
	columns[0].ptr.size = strlen(sym->cs_usr);

	columns[1].type = CSDB_TYPE_INT;
	columns[1].non_ptr.integer = sym->cs_kind;

	columns[2].type = CSDB_TYPE_TEXT;
	columns[2].ptr.data = sym->cs_symbol_name;
	columns[2].ptr.size = strlen(sym->cs_symbol_name);

	columns[3].type = CSDB_TYPE_INT;
	columns[3].non_ptr.integer = sym->cs_type;

	columns[4].type = CSDB_TYPE_INT;
	columns[4].non_ptr.integer = sym->cs_is_def ? 1 : 0;

	columns[5].type = CSDB_TYPE_TEXT;
	columns[5].ptr.data = sym->cs_file_path;
	columns[5].ptr.size = strlen(sym->cs_file_path);

	columns[6].type = CSDB_TYPE_INT;
	columns[6].non_ptr.integer = sym->cs_start_line;

	columns[7].type = CSDB_TYPE_INT;
	columns[7].non_ptr.integer = sym->cs_start_col;

	for (i = 0, nr_columns = csdb_column_count(stmt);
			i < 8; i++)
		csdb_bind(stmt, i, columns + i);

	ret = csdb_query(handle, stmt, NULL, NULL);
	csdb_free_query(stmt);
	return ret;
}
