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

#endif /* __CLANG_SOURCE_DB_H__ */
