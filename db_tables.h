#ifndef __CLANG_SOURCE_DB_TABLES_H__
#define __CLANG_SOURCE_DB_TABLES_H__

enum csdb_table_id {
	CS_TABLE_SYMBOLS_ID,
	CS_TABLE_FILES_ID,
	CS_TABLE_MAX
};

#define CSDB_TABLE_ENTRY(id, name, init_fn) \
	[id] = {(name), (init_fn)},
struct csdb_table_ops {
	char *t_name;
	int (*t_init)(csdb_t *handle, const struct csdb_table_ops *table);
};

#define CS_TABLE_SYMBOLS_NAME "cs_table_symbols"
#define CS_TABLE_FILES_NAME "cs_table_files"

#endif /* __CLANG_SOURCE_DB_TABLES_H__ */
