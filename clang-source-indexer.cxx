#include <string.h>

#include <clang-c/Index.h>
#include <msgpack.hpp>

enum CXChildVisitResult cs_source_visitor(
		CXCursor cursor,
		CXCursor parent,
		CXClientData data)
{
	struct walker *visitor = (struct walker *)data;

	CXCursor referenced = clang_getCursorReferenced(cursor);
	CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	CXFile file;

	std::string fname;
	std::string usr_str = cursor_usr_str(referenced);
	std::string name_str = cursor_name_str(cursor);
	std::string type_str = cursor_type_str(cursor);
	std::string kind_str = cursor_kind_str(cursor);
	CXSourceLocation location = clang_getCursorLocation(cursor);

	unsigned int line, column, offs;
	uint32_t prop = 0;
	walker_ref ref;

	visitor->retval = 0;

	clang_getSpellingLocation(location, &file, &line, &column, &offs);
	fname = file_name_str(file);

	printf("Referenced USR: %s, cursor_kind: %s, "
		"cursor_linkage: %s, "
		"cursor_type: %s, spelling: %s, "
		"line: %d, column: %d, offs: %d, "
		"fname: %s\n",
		usr_str.c_str(), kind_str.c_str(),
		cursor_linkage_str(cursor).c_str(),
		type_str.c_str(), name_str.c_str(),
		line, column, offs,
		fname.c_str());

	if (clang_isDeclaration(cursor_kind))
		prop = REF_PROP_DECL;

	if (clang_isCursorDefinition(cursor))
		prop |= REF_PROP_DEF;

	clang_getCursorReferenced(cursor);
	if ((prop & REF_PROP_DECL) && !usr_str.empty()) {
		ref.assign(
			prop,
			line, column, offs, &fname);
		visitor->retval = visitor->symbol_op(
					usr_str.c_str(),
					&ref,
					visitor->args);
		if (visitor->retval) {
			fprintf(stderr, "Failed to do operation: %d\n", visitor->retval);
			return CXChildVisit_Break;
		}
	}

	return CXChildVisit_Recurse;
}
