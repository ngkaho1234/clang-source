#include <string.h>

#include <clang-c/Index.h>
#include <msgpack.hpp>

enum CXChildVisitResult
cs_source_visitor(
	CXCursor cursor,
	CXCursor parent,
	CXClientData data)
{
	CXCursor referenced = clang_getCursorReferenced(cursor);
	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXFile file;

	const char *fname;
	const char *usr_str = cursor_usr_str(referenced);
	const char *name_str = cursor_name_str(cursor);
	const char *type_str = cursor_type_str(cursor);
	const char *kind_str = cursor_kind_str(cursor);
	CXSourceLocation location = clang_getCursorLocation(cursor);

	unsigned int line, column, offs;
	uint32_t prop = 0;

	clang_getSpellingLocation(location, &file, &line, &column, &offs);
	fname = file_name_str(file);

	printf("Referenced USR: %s, cursor_kind: %s, "
		"cursor_linkage: %s, "
		"cursor_type: %s, spelling: %s, "
		"line: %d, column: %d, offs: %d, "
		"fname: %s, isCursorDefinition: %s\n",
		usr_str, kind_str,
		cursor_linkage_str(cursor),
		type_str, name_str,
		line, column, offs,
		fname, clang_isCursorDefinition(cursor)?"Yes":"No");

	return CXChildVisit_Recurse;
}
