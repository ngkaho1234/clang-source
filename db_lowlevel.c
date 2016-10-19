#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>

#include "db_lowlevel.h"

int csdb_start_txn(csdb_t *handle)
{
	sqlite3_stmt *stmt;
	const char sql_command[] = "BEGIN";
	int ret = sqlite3_prepare_v2(
				handle,
				sql_command,
				sizeof(sql_command) - 1,
				&stmt,
				NULL);
	if (ret != SQLITE_OK)
		return ret;

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW);
	if (ret == SQLITE_DONE)
		ret = SQLITE_OK;

	sqlite3_finalize(stmt);

	return ret;
}

int csdb_commit_txn(csdb_t *handle)
{
	sqlite3_stmt *stmt;
	const char sql_command[] = "COMMIT";
	int ret = sqlite3_prepare_v2(
				handle,
				sql_command,
				sizeof(sql_command) - 1,
				&stmt,
				NULL);
	if (ret != SQLITE_OK)
		return ret;

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW);
	if (ret == SQLITE_DONE)
		ret = SQLITE_OK;

	sqlite3_finalize(stmt);

	return ret;
}

void csdb_rollback_txn(csdb_t *handle)
{
	sqlite3_stmt *stmt;
	const char sql_command[] = "ROLLBACK";
	int ret = sqlite3_prepare_v2(
				handle,
				sql_command,
				sizeof(sql_command) - 1,
				&stmt,
				NULL);
	if (ret != SQLITE_OK)
		return;

	while (sqlite3_step(stmt) == SQLITE_ROW);
	sqlite3_finalize(stmt);
}

int csdb_open(
	const char *filename,
	csdb_t **handle,
	int ro,
	int create)
{
	int ret;
	int flags = ro ? (SQLITE_OPEN_READONLY) :
				(SQLITE_OPEN_READWRITE);
	if (!ro && create)
		flags |= SQLITE_OPEN_CREATE;

	ret = sqlite3_open_v2(filename, handle, flags, NULL);
	if (ret != SQLITE_OK) {
		sqlite3_close_v2(*handle);
		return ret;
	}

	ret = csdb_create_prepare(*handle);
	if (ret != SQLITE_OK)
		sqlite3_close_v2(*handle);

	return ret;
}

void csdb_close(csdb_t *handle)
{
	sqlite3_close_v2(handle);
}

int csdb_vasprintf(char **strp, const char *fmt, va_list args)
{
	int strsz;
	char *buf;
	strsz = vsnprintf(NULL, 0, fmt, args);
	if (strsz < 0 || !strp)
		return strsz;

	buf = (char *)malloc(strsz + 1);
	if (!buf) {
		*strp = NULL;
		return -1;
	}

	assert(vsnprintf(buf, strsz + 1, fmt, args) >= 0);
	*strp = buf;
	return strsz;
}

int csdb_prepare_query(csdb_t *handle, void **stmt, const char *fmt, ...)
{
	int ret = CSDB_ERR_NOMEM;
	char *str;
	va_list args;

	va_start(args, fmt);
	ret = csdb_vasprintf(&str, fmt, args);
	va_end(args);
	if (ret == -1)
		return CSDB_ERR_NOMEM;

	ret = sqlite3_prepare_v2(
			handle,
			str,
			strlen(str),
			(sqlite3_stmt **)stmt,
			NULL);

	free(str);
	return ret;
}

int csdb_clear_bindings(void *stmt)
{
	return sqlite3_clear_bindings(stmt);
}

static void csdb_destroy_column(void *data)
{
	free(data);
}

int csdb_column_count(void *stmt)
{
	return sqlite3_column_count(stmt);
}

/*
 * Index starts from 0
 */
int csdb_bind(void *stmt, int idx, csdb_column_t *column)
{
	int ret;
	void *data = NULL;
	switch (column->type) {
	case CSDB_TYPE_INT:
		ret = sqlite3_bind_int64(stmt, idx + 1, column->non_ptr.integer);
		break;
	case CSDB_TYPE_FLOAT:
		ret = sqlite3_bind_double(stmt, idx + 1, column->non_ptr.real);
		break;
	case CSDB_TYPE_NULL:
		ret = sqlite3_bind_null(stmt, idx + 1);
		break;
	case CSDB_TYPE_TEXT:
		data = malloc(column->ptr.size);
		if (!data) {
			ret = CSDB_ERR_NOMEM;
			break;
		}
		memcpy(data, column->ptr.data, column->ptr.size);
		ret = sqlite3_bind_text64(
				stmt,
				idx + 1,
				(char *)data,
				column->ptr.size,
				csdb_destroy_column,
				SQLITE_UTF8);
		break;
	case CSDB_TYPE_BLOB:
		data = malloc(column->ptr.size);
		if (!data) {
			ret = CSDB_ERR_NOMEM;
			break;
		}
		memcpy(data, column->ptr.data, column->ptr.size);
		ret = sqlite3_bind_blob64(
				stmt,
				idx + 1,
				(char *)data,
				column->ptr.size,
				csdb_destroy_column);
		break;
	default:
		ret = CSDB_ERR_INVAL;
	}

	return ret;
}

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
)
{
	int ret = CSDB_ERR_OK;
	const char **columns_name = NULL;
	csdb_column_t *columns = NULL;
	int nr_columns = sqlite3_column_count(stmt);
	if (nr_columns) {
		columns = malloc(sizeof(csdb_column_t) * nr_columns);
		if (!columns)
			goto cleanup;

		columns_name = malloc(sizeof(const char *) * nr_columns);
		if (!columns)
			goto cleanup;
	}
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		int i;
		for (i = 0; i < nr_columns; i++) {
			int res_col_size = sqlite3_column_bytes(stmt, i + 1);
			columns_name[i] = sqlite3_column_name(stmt, i);
			switch (sqlite3_column_type(stmt, i + 1)) {
			case SQLITE_INTEGER:
				columns[i].type = CSDB_TYPE_INT;
				columns[i].non_ptr.integer =
					sqlite3_column_int64(stmt, i + 1);
				break;
			case SQLITE_FLOAT:
				columns[i].type = CSDB_TYPE_FLOAT;
				columns[i].non_ptr.real =
					sqlite3_column_double(stmt, i + 1);
				break;
			case SQLITE_NULL:
				columns[i].type = CSDB_TYPE_NULL;
				break;
			case SQLITE3_TEXT:
				columns[i].type = CSDB_TYPE_TEXT;
				columns[i].ptr.data =
					sqlite3_column_text(stmt, i + 1);
				columns[i].ptr.size = res_col_size;
				break;
			case SQLITE_BLOB:
				columns[i].type = CSDB_TYPE_BLOB;
				columns[i].ptr.data =
					sqlite3_column_blob(stmt, i + 1);
				columns[i].ptr.size = res_col_size;
				break;
			default:
				ret = CSDB_ERR_CORRUPTED;
				goto cleanup;
			}
		}
		if (iter) {
			enum csdb_loopctl_t loopctl;
			loopctl = iter(handle,
				       (nr_columns)?columns_name:NULL,
				       (nr_columns)?columns:NULL,
				       nr_columns,
				       iter_arg);
			if (loopctl != CSDB_LOOP_BREAK) {
				ret = SQLITE_DONE;
				break;
			}
		}
	}
	if (ret == SQLITE_DONE)
		ret = SQLITE_OK;
cleanup:
	if (columns)
		free(columns);
	if (columns_name)
		free(columns_name);

	return ret;
}

void csdb_free_query(void *stmt)
{
	sqlite3_finalize(stmt);
}


#ifdef CLANG_SOURCE_DB_TEST

#include <stdio.h>
#include <stdlib.h>

#include "db_tables.h"

static int csdb_delete_file_symbols(csdb_t *handle, const char *file)
{
	void *stmt = NULL;
	csdb_column_t column;
	int ret = csdb_prepare_query(handle, &stmt,
			CSDB_OP_DELETE CS_TABLE_SYMBOLS_NAME CSDB_OP_CONDITIONS "file = ?");
	if (ret != CSDB_ERR_OK)
		return ret;

	column.type = CSDB_TYPE_TEXT;
	column.ptr.data = file;
	column.ptr.size = strlen(file);
	ret = csdb_bind(stmt, 0, &column);
	if (ret != CSDB_ERR_OK) {
		csdb_free_query(stmt);
		return ret;
	}

	ret = csdb_query(handle, stmt, NULL, NULL);
	csdb_free_query(stmt);
	return ret;
}

void usage(char **argv)
{
	fprintf(stderr, "Usage: %s <Database Path>\n", argv[0]);
}

int main(int argc, char **argv)
{
	int ret;
	csdb_t *handle;
	if (argc != 2) {
		usage(argv);
		return EXIT_FAILURE;
	}
	ret = csdb_open(argv[1], &handle, 0, 1);
	if (ret != CSDB_ERR_OK) {
		puts(sqlite3_errstr(ret));
		return EXIT_FAILURE;
	}

	ret = csdb_start_txn(handle);
	if (ret != CSDB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		csdb_close(handle);
		return EXIT_FAILURE;
	}
	ret = csdb_delete_file_symbols(handle, "test.c");
	if (ret != CSDB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		csdb_close(handle);
		return EXIT_FAILURE;
	}
	ret = csdb_commit_txn(handle);
	if (ret != CSDB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		csdb_close(handle);
		return EXIT_FAILURE;
	}
	csdb_close(handle);
	return EXIT_SUCCESS;
}

#endif /* CLANG_SOURCE_DB_TEST */
