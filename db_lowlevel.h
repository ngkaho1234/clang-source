#ifndef __CLANG_SOURCE_DB_H__
#define __CLANG_SOURCE_DB_H__

#include <sqlite3.h>

/*
 * Error code.
 *
 * SQLite3 error code should be positive integers, while
 * CSDB-specific error code should be negative integers.
 */
#define CSDB_ERR_OK SQLITE_OK
#define CSDB_ERR_CORRUPTED (CSDB_ERR_OK - 1)
#define CSDB_ERR_NOMEM (CSDB_ERR_OK - 2)
#define CSDB_ERR_INVAL (CSDB_ERR_OK - 3)

/*
 * Query macros
 */
#define CSDB_OP_INSERT "INSERT INTO "
#define CSDB_OP_DELETE "DELETE FROM "
#define CSDB_OP_UPDATE "UPDATE "
#define CSDB_OP_SELECT "SELECT "
#define CSDB_OP_CONDITIONS " WHERE "

#define CSDB_FMT_INSERT "INSERT INTO %s (%s) VALUES (%s)"

/*
 * Loop control
 */
enum csdb_loopctl_t {
	CSDB_LOOP_CONT = 0,
	CSDB_LOOP_BREAK = 1,
};

typedef sqlite3 csdb_t;
typedef sqlite3_uint64 csdb_id_t;

enum csdb_type_t {
	CSDB_TYPE_INT,
	CSDB_TYPE_FLOAT,
	CSDB_TYPE_TEXT,
	CSDB_TYPE_BLOB,
	CSDB_TYPE_NULL,
};

typedef struct csdb_column_s {
	enum csdb_type_t type;
	union {
		union {
			long long integer;
			double real;
		} non_ptr;
		struct {
			const void *data;
			sqlite3_uint64 size;
		} ptr;
	};
} csdb_column_t;


/*
 * csdb_open - Open/Create a clang-source database
 */
int csdb_open(
	const char *filename,
	csdb_t **handle,
	int ro,
	int create);
int csdb_create_prepare(csdb_t *handle);
/*
 * csdb_close - Close the handle to a clang-source database
 */
void csdb_close(csdb_t *handle);

int csdb_start_txn(csdb_t *handle);
int csdb_commit_txn(csdb_t *handle);
void csdb_rollback_txn(csdb_t *handle);

int csdb_prepare_query(csdb_t *handle, void **stmt, const char *fmt, ...);
int csdb_clear_bindings(void *stmt);
int csdb_column_count(void *stmt);
int csdb_bind(void *stmt, int idx, csdb_column_t *column);
int csdb_query(
	csdb_t *handle,
	void *stmt,
	enum csdb_loopctl_t (*iter)(
		csdb_t *handle,
		const char **columns_name,
		csdb_column_t *columns,
		int nr_columns,
		void *arg
	),
	void *iter_arg
);
void csdb_free_query(void *stmt);

#endif /* __CLANG_SOURCE_DB_H__ */
