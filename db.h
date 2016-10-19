#ifndef __CLANG_SOURCE_DB_H__
#define __CLANG_SOURCE_DB_H__

#include "db_lowlevel.h"
#include "db_tables.h"

#include "symbols.h"

typedef struct cs_query_s {
	csdb_t *q_conn;		/* Database connection */

} cs_query_t;

#endif /* __CLANG_SOURCE_DB_H__ */
