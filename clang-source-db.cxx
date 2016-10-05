#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <string>

#include "clang-source-db.h"

using namespace std;

enum cs_db_table_id {
	CS_TABLE_SYMBOLS_ID,
	CS_TABLE_FILES_ID,
	CS_TABLE_MAX
};
#define CS_TABLE_SYMBOLS_NAME "cs_table_symbols"
#define CS_TABLE_FILES_NAME "cs_table_files"

#define CS_DB_TABLE_ENTRY(id, name, init_fn) \
	[id] = {(name), (init_fn)},
struct cs_db_table_ops {
	char *t_name;
	int (*t_init)(cs_db_t *handle, const struct cs_db_table_ops *table);
};

static int cs_db_init_table_symbol(cs_db_t *handle, const struct cs_db_table_ops *table)
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

static const struct cs_db_table_ops cs_db_table_ops[CS_TABLE_MAX] = {
	CS_DB_TABLE_ENTRY(
		CS_TABLE_SYMBOLS_ID,
		CS_TABLE_SYMBOLS_NAME,
		cs_db_init_table_symbol)
};

static int cs_db_create_prepare(cs_db_t *handle)
{
	int i;
	int ret = 0;

	ret = sqlite3_exec(handle, "BEGIN;", NULL, NULL, NULL);
	if (ret != SQLITE_OK)
		return ret;

	for (i = 0; i < CS_TABLE_MAX; i++) {
		if (cs_db_table_ops[i].t_init) {
			ret = cs_db_table_ops[i].t_init(
							handle,
							&cs_db_table_ops[i]);
			if (ret != SQLITE_OK)
				break;
		}
	}
	if (ret == SQLITE_OK)
		ret = sqlite3_exec(handle, "COMMIT;", NULL, NULL, NULL);
	else
		sqlite3_exec(handle, "ROLLBACK;", NULL, NULL, NULL);

	return ret;
}

int cs_db_open(
	const char *filename,
	cs_db_t **handle,
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

	ret = cs_db_create_prepare(*handle);
	if (ret != SQLITE_OK)
		sqlite3_close_v2(*handle);

	return ret;
}

void cs_db_close(cs_db_t *handle)
{
	sqlite3_close_v2(handle);
}

int cs_db_start_txn(cs_db_t *handle)
{
	return sqlite3_exec(handle, "BEGIN;", NULL, NULL, NULL);
}

int cs_db_commit_txn(cs_db_t *handle)
{
	return sqlite3_exec(handle, "COMMIT;", NULL, NULL, NULL);
}

void cs_db_rollback_txn(cs_db_t *handle)
{
	sqlite3_exec(handle, "ROLLBACK;", NULL, NULL, NULL);
}

int cs_db_select_general(
		cs_db_t *handle,
		enum cs_db_table_id table_id,
		cs_db_column_t *col,
		int nr_col,
		enum cs_db_loopctl_t (*iter)(
			cs_db_t *handle,
			enum cs_db_table_id table_id,
			cs_db_column_t *col,
			int nr_col,
			void *arg
		),
		void *iter_arg
)
{
	string sql_command =
			string("SELECT * FROM ") +
			string(cs_db_table_ops[table_id].t_name);
	int i, ret = CS_DB_ERR_OK;
	sqlite3_stmt *stmt;
	int nr_res_col;
	cs_db_column_t *res_col = NULL;

	if (nr_col)
		sql_command += " WHERE ";

	for (i = 0; i < nr_col; i++) {
		sql_command += string(col[i].name) + " = ?";
		if (i != nr_col - 1)
			sql_command += " AND ";
	}

	ret = sqlite3_prepare_v2(
			handle,
			sql_command.c_str(),
			sql_command.length(),
			&stmt,
			NULL);
	if (ret)
		return ret;

	for (i = 0; i < nr_col; i++) {
		switch (col[i].type) {
		case CS_DB_TYPE_INT:
			ret = sqlite3_bind_int64(stmt, i + 1, col[i].non_ptr.integer);
			break;
		case CS_DB_TYPE_FLOAT:
			ret = sqlite3_bind_double(stmt, i + 1, col[i].non_ptr.fp);
			break;
		case CS_DB_TYPE_NULL:
			ret = sqlite3_bind_null(stmt, i + 1);
			break;
		case CS_DB_TYPE_TEXT:
			ret = sqlite3_bind_text64(
					stmt,
					i + 1,
					(char *)col[i].ptr.data,
					col[i].ptr.size,
					col[i].ptr.post_op_cb,
					SQLITE_UTF8);
			break;
		case CS_DB_TYPE_BLOB:
			ret = sqlite3_bind_blob64(
					stmt,
					i + 1,
					(char *)col[i].ptr.data,
					col[i].ptr.size,
					col[i].ptr.post_op_cb);
			break;
		default:
			ret = CS_DB_ERR_INVAL;
		}
		if (ret != SQLITE_OK)
			goto cleanup;
	}

	nr_res_col = sqlite3_column_count(stmt);
	if (nr_res_col) {
		res_col = new cs_db_column_t[nr_res_col];
		if (!res_col)
			goto cleanup;
	}
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		for (i = 0; i < nr_res_col; i++) {
			int res_col_size = sqlite3_column_bytes(stmt, i + 1);
			res_col[i].name = sqlite3_column_name(stmt, i);
			switch (sqlite3_column_type(stmt, i + 1)) {
			case SQLITE_INTEGER:
				res_col[i].type = CS_DB_TYPE_INT;
				res_col[i].non_ptr.integer =
					sqlite3_column_int64(stmt, i + 1);
				break;
			case SQLITE_FLOAT:
				res_col[i].type = CS_DB_TYPE_FLOAT;
				res_col[i].non_ptr.fp =
					sqlite3_column_double(stmt, i + 1);
				break;
			case SQLITE_NULL:
				res_col[i].type = CS_DB_TYPE_NULL;
				break;
			case SQLITE3_TEXT:
				res_col[i].type = CS_DB_TYPE_TEXT;
				res_col[i].ptr.data =
					sqlite3_column_text(stmt, i + 1);
				res_col[i].ptr.size = res_col_size;
				res_col[i].ptr.post_op_cb = NULL;
				break;
			case SQLITE_BLOB:
				res_col[i].type = CS_DB_TYPE_BLOB;
				res_col[i].ptr.data =
					sqlite3_column_blob(stmt, i + 1);
				res_col[i].ptr.size = res_col_size;
				res_col[i].ptr.post_op_cb = NULL;
				break;
			default:
				throw CS_DB_EXCEPT_UNKNOWN;
			}
		}
		if (iter) {
			enum cs_db_loopctl_t loopctl;
			loopctl = iter(
					handle,
					table_id,
					(nr_res_col)?res_col:NULL,
					nr_res_col,
					iter_arg);
			if (loopctl != CS_DB_LOOP_BREAK) {
				ret = SQLITE_DONE;
				break;
			}
		}
	}

	if (ret == SQLITE_DONE)
		ret = SQLITE_OK;
cleanup:
	sqlite3_finalize(stmt);

	if (res_col)
		delete[] res_col;

	return ret;
}

int cs_db_insert_general(
		cs_db_t *handle,
		enum cs_db_table_id table_id,
		cs_db_column_t *col,
		int nr_col)
{
	string sql_command;
	int i, ret = CS_DB_ERR_OK;
	sqlite3_stmt *stmt;

	sql_command =
		string("INSERT INTO ") +
		string(cs_db_table_ops[table_id].t_name) +
		string("(");
	for (i = 0; i < nr_col; i++) {
		sql_command += col[i].name;
		if (i != nr_col - 1)
			sql_command += ", ";
	}
	sql_command += ")";
	sql_command +=
		string(" VALUES ") +
		string("(");
	for (i = 0; i < nr_col; i++) {
		sql_command += "?";
		if (i != nr_col - 1)
			sql_command += ", ";
	}
	sql_command += ")";

	ret = sqlite3_prepare_v2(
			handle,
			sql_command.c_str(),
			sql_command.length(),
			&stmt,
			NULL);
	if (ret)
		return ret;

	for (i = 0; i < nr_col; i++) {
		switch (col[i].type) {
		case CS_DB_TYPE_INT:
			ret = sqlite3_bind_int64(stmt, i + 1, col[i].non_ptr.integer);
			break;
		case CS_DB_TYPE_FLOAT:
			ret = sqlite3_bind_double(stmt, i + 1, col[i].non_ptr.fp);
			break;
		case CS_DB_TYPE_NULL:
			ret = sqlite3_bind_null(stmt, i + 1);
			break;
		case CS_DB_TYPE_TEXT:
			ret = sqlite3_bind_text64(
					stmt,
					i + 1,
					(char *)col[i].ptr.data,
					col[i].ptr.size,
					col[i].ptr.post_op_cb,
					SQLITE_UTF8);
			break;
		case CS_DB_TYPE_BLOB:
			ret = sqlite3_bind_blob64(
					stmt,
					i + 1,
					(char *)col[i].ptr.data,
					col[i].ptr.size,
					col[i].ptr.post_op_cb);
			break;
		default:
			ret = CS_DB_ERR_INVAL;
		}
		if (ret != SQLITE_OK)
			goto cleanup;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW);

	if (ret == SQLITE_DONE)
		ret = SQLITE_OK;
cleanup:
	sqlite3_finalize(stmt);
	return ret;
}

int cs_db_update_general(
		cs_db_t *handle,
		enum cs_db_table_id table_id,
		cs_db_id_t id,
		cs_db_column_t *col,
		int nr_col)
{
	string sql_command =
			string("UPDATE ") +
			string(cs_db_table_ops[table_id].t_name) +
			string(" SET ");
	int i, ret = CS_DB_ERR_OK;
	sqlite3_stmt *stmt;
	for (i = 0; i < nr_col; i++) {
		sql_command += string(col[i].name) + " = ?";
		if (i != nr_col - 1)
			sql_command += ", ";
	}
	sql_command += " WHERE id = ?";

	ret = sqlite3_prepare_v2(
			handle,
			sql_command.c_str(),
			sql_command.length(),
			&stmt,
			NULL);
	if (ret)
		return ret;

	for (i = 0; i < nr_col; i++) {
		switch (col[i].type) {
		case CS_DB_TYPE_INT:
			ret = sqlite3_bind_int64(stmt, i + 1, col[i].non_ptr.integer);
			break;
		case CS_DB_TYPE_FLOAT:
			ret = sqlite3_bind_double(stmt, i + 1, col[i].non_ptr.fp);
			break;
		case CS_DB_TYPE_NULL:
			ret = sqlite3_bind_null(stmt, i + 1);
			break;
		case CS_DB_TYPE_TEXT:
			ret = sqlite3_bind_text64(
					stmt,
					i + 1,
					(char *)col[i].ptr.data,
					col[i].ptr.size,
					col[i].ptr.post_op_cb,
					SQLITE_UTF8);
			break;
		case CS_DB_TYPE_BLOB:
			ret = sqlite3_bind_blob64(
					stmt,
					i + 1,
					(char *)col[i].ptr.data,
					col[i].ptr.size,
					col[i].ptr.post_op_cb);
			break;
		default:
			ret = CS_DB_ERR_INVAL;
		}
		if (ret != SQLITE_OK)
			goto cleanup;
	}
	ret = sqlite3_bind_int64(stmt, i + 1, id);
	if (ret != SQLITE_OK)
		goto cleanup;

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW);

	if (ret == SQLITE_DONE)
		ret = SQLITE_OK;
cleanup:
	sqlite3_finalize(stmt);
	return ret;
}

int cs_db_insert_symbol(
		cs_db_t *handle,
		const char *usr,
		int kind,
		const char *name,
		const char *type,
		const char *file,
		int start_line,
		int start_col)
{
	cs_db_column_t col[7] = {
		{
			.name = "usr",
			.type = CS_DB_TYPE_TEXT,
			.ptr = {
				usr,
				string(usr).length(),
				NULL,
			}
		},
		{
			.name = "kind",
			.type = CS_DB_TYPE_INT,
			.non_ptr = {
				kind,
			}
		},
		{
			.name = "name",
			.type = CS_DB_TYPE_TEXT,
			.ptr = {
				name,
				string(name).length(),
				NULL,
			}
		},
		{
			.name = "type",
			.type = CS_DB_TYPE_TEXT,
			.ptr = {
				type,
				string(type).length(),
				NULL,
			}
		},
		{
			.name = "file",
			.type = CS_DB_TYPE_TEXT,
			.ptr = {
				file,
				string(file).length(),
				NULL,
			}
		},
		{
			.name = "start_line",
			.type = CS_DB_TYPE_INT,
			.non_ptr = {
				start_line,
			}
		},
		{
			.name = "start_col",
			.type = CS_DB_TYPE_INT,
			.non_ptr = {
				start_col,
			}
		},
	};
	return cs_db_insert_general(handle, CS_TABLE_SYMBOLS_ID, col, 7);
}

int cs_db_update_symbol(
		cs_db_t *handle,
		cs_db_id_t id,
		const char *usr,
		int kind,
		const char *name,
		const char *type,
		const char *file,
		int start_line,
		int start_col)
{
	cs_db_column_t col[7] = {
		{
			.name = "usr",
			.type = CS_DB_TYPE_TEXT,
			.ptr = {
				usr,
				string(usr).length(),
				NULL,
			}
		},
		{
			.name = "kind",
			.type = CS_DB_TYPE_INT,
			.non_ptr = {
				kind,
			}
		},
		{
			.name = "name",
			.type = CS_DB_TYPE_TEXT,
			.ptr = {
				name,
				string(name).length(),
				NULL,
			}
		},
		{
			.name = "type",
			.type = CS_DB_TYPE_TEXT,
			.ptr = {
				type,
				string(type).length(),
				NULL,
			}
		},
		{
			.name = "file",
			.type = CS_DB_TYPE_TEXT,
			.ptr = {
				file,
				string(file).length(),
				NULL,
			}
		},
		{
			.name = "start_line",
			.type = CS_DB_TYPE_INT,
			.non_ptr = {
				start_line,
			}
		},
		{
			.name = "start_col",
			.type = CS_DB_TYPE_INT,
			.non_ptr = {
				start_col,
			}
		},
	};
	return cs_db_update_general(handle, CS_TABLE_SYMBOLS_ID, id, col, 7);
}

int cs_db_delete_general(
		cs_db_t *handle,
		enum cs_db_table_id table_id,
		cs_db_column_t *col,
		int nr_col)
{
	string sql_command =
			string("DELETE FROM ") +
			string(cs_db_table_ops[table_id].t_name);
	int i, ret = CS_DB_ERR_OK;
	sqlite3_stmt *stmt;

	if (nr_col)
		sql_command += " WHERE ";

	for (i = 0; i < nr_col; i++) {
		sql_command += string(col[i].name) + " = ?";
		if (i != nr_col - 1)
			sql_command += " AND ";
	}

	ret = sqlite3_prepare_v2(
			handle,
			sql_command.c_str(),
			sql_command.length(),
			&stmt,
			NULL);
	if (ret)
		return ret;

	for (i = 0; i < nr_col; i++) {
		switch (col[i].type) {
		case CS_DB_TYPE_INT:
			ret = sqlite3_bind_int64(stmt, i + 1, col[i].non_ptr.integer);
			break;
		case CS_DB_TYPE_FLOAT:
			ret = sqlite3_bind_double(stmt, i + 1, col[i].non_ptr.fp);
			break;
		case CS_DB_TYPE_NULL:
			ret = sqlite3_bind_null(stmt, i + 1);
			break;
		case CS_DB_TYPE_TEXT:
			ret = sqlite3_bind_text64(
					stmt,
					i + 1,
					(char *)col[i].ptr.data,
					col[i].ptr.size,
					col[i].ptr.post_op_cb,
					SQLITE_UTF8);
			break;
		case CS_DB_TYPE_BLOB:
			ret = sqlite3_bind_blob64(
					stmt,
					i + 1,
					(char *)col[i].ptr.data,
					col[i].ptr.size,
					col[i].ptr.post_op_cb);
			break;
		default:
			ret = CS_DB_ERR_INVAL;
		}
		if (ret != SQLITE_OK)
			goto cleanup;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW);

	if (ret == SQLITE_DONE)
		ret = SQLITE_OK;
cleanup:
	sqlite3_finalize(stmt);
	return ret;
}

int cs_db_delete_file_symbols(cs_db_t *handle, const char *filename)
{
	cs_db_column_t col = {
		.name = "file",
		.type = CS_DB_TYPE_TEXT,
		.ptr = {
			filename,
			string(filename).length(),
			NULL,
		}
	};
	return cs_db_delete_general(
			handle,
			CS_TABLE_SYMBOLS_ID,
			&col,
			1);
}

#if 1

#include <stdio.h>
#include <stdlib.h>

static enum cs_db_loopctl_t
cs_db_select_iter(
	cs_db_t *handle,
	enum cs_db_table_id table_id,
	cs_db_column_t *col,
	int nr_col,
	void *arg)
{
	int i;
	for (i = 0; i < nr_col; i++)
		printf("|%s|", col[i].name);

	puts("");
	return CS_DB_LOOP_CONT;
}

void usage(char **argv)
{
	fprintf(stderr, "Usage: %s <Database Path>\n", argv[0]);
}

int main(int argc, char **argv)
{
	int ret;
	cs_db_t *handle;
	if (argc != 2) {
		usage(argv);
		return EXIT_FAILURE;
	}
	ret = cs_db_open(argv[1], &handle, 0, 1);
	if (ret != CS_DB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		return EXIT_FAILURE;
	}

	ret = cs_db_delete_file_symbols(handle, "test.c");
	if (ret != CS_DB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		cs_db_close(handle);
		return EXIT_FAILURE;
	}
	ret = cs_db_start_txn(handle);
	if (ret != CS_DB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		cs_db_close(handle);
		return EXIT_FAILURE;
	}
	ret = cs_db_insert_symbol(
			handle,
			"c@func_test",
			200,
			"func_test",
			"void",
			"test.c",
			1,
			3);
	if (ret != CS_DB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		cs_db_close(handle);
		return EXIT_FAILURE;
	}
	ret = cs_db_commit_txn(handle);
	if (ret != CS_DB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		cs_db_close(handle);
		return EXIT_FAILURE;
	}
	ret = cs_db_update_symbol(
			handle,
			1,
			"c@func_test",
			200,
			"func_test",
			"void",
			"test.c",
			9,
			3);
	if (ret != CS_DB_ERR_OK) {
		puts(sqlite3_errmsg(handle));
		cs_db_close(handle);
		return EXIT_FAILURE;
	}

	ret = cs_db_select_general(
		handle,
		CS_TABLE_SYMBOLS_ID,
		NULL,
		0,
		cs_db_select_iter,
		NULL);

	cs_db_close(handle);
	return EXIT_SUCCESS;
}

#endif /* CLANG_SOURCE_DB_TEST */
