#ifndef __CLANG_SOURCE_DB_H__
#define __CLANG_SOURCE_DB_H__

#include <sqlite3.h>

/*
 * Error code.
 *
 * SQLite3 error code should be positive integers, while
 * CS_DB-specific error code should be negative integers.
 */
#define CS_DB_ERR_OK SQLITE_OK
#define CS_DB_ERR_CORRUPTED (CS_DB_ERR_OK - 1)
#define CS_DB_ERR_NOMEM (CS_DB_ERR_OK - 2)
#define CS_DB_ERR_INVAL (CS_DB_ERR_OK - 3)

/*
 * Loop control
 */
enum cs_db_loopctl_t {
	CS_DB_LOOP_CONT = 0,
	CS_DB_LOOP_BREAK = 1,
};

typedef sqlite3 cs_db_t;
typedef sqlite3_stmt cs_db_query_t;
typedef long long cs_db_id_t;

enum cs_db_type_t {
	CS_DB_TYPE_INT,
	CS_DB_TYPE_FLOAT,
	CS_DB_TYPE_TEXT,
	CS_DB_TYPE_BLOB,
	CS_DB_TYPE_NULL,
};

enum cs_db_exceptions {
	CS_DB_EXCEPT_NULL,
	CS_DB_EXCEPT_UNKNOWN,
};

typedef struct cs_db_column_s {
	const char *name;
	cs_db_type_t type;
	union {
		union {
			long long integer;
			double fp;
		} non_ptr;
		struct {
			const void *data;
			sqlite3_uint64 size;
			void (*post_op_cb)(void *);
		} ptr;
	};
} cs_db_column_t;

enum cs_db_table_id {
	CS_TABLE_SYMBOLS_ID,
	CS_TABLE_FILES_ID,
	CS_TABLE_MAX
};

#define CS_DB_TABLE_ENTRY(id, name, init_fn, nr_columns) \
	[id] = { .t_name = (name), .t_init = (init_fn), .t_nr_columns = (nr_columns) },
struct cs_db_table_ops {
	char *t_name;
	int (*t_init)(cs_db_t *handle, const struct cs_db_table_ops *table);

	int t_nr_columns; /* Nr. of columns. (ID is not accounted into this field) */
	cs_db_column_t *columns; /* Just ignore the data fields */
};

#define CS_TABLE_SYMBOLS_NAME "cs_table_symbols"
#define CS_TABLE_FILES_NAME "cs_table_files"


/*
 * cs_db_open - Open/Create a clang-source database
 */
int cs_db_open(
	const char *filename,
	cs_db_t **handle,
	int ro,
	int create);

/*
 * cs_db_close - Close the handle to a clang-source database
 */
void cs_db_close(cs_db_t *handle);

#endif /* __CLANG_SOURCE_DB_H__ */
