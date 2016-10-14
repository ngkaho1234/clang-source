#include <string.h>
#include <stdio.h>

#include <clang-c/Index.h>

static const char *cs_source_cursor_linkage_str(CXCursor cursor)
{
	enum CXLinkageKind linkage = clang_getCursorLinkage(cursor);
	const char *linkage_str;

	/*
	 * This conversion table is taken from
	 * https://github.com/sabottenda/libclang-sample
	 */
	switch (linkage) {
		case CXLinkage_Invalid:        linkage_str = "Invalid"; break;
		case CXLinkage_NoLinkage:      linkage_str = "NoLinkage"; break;
		case CXLinkage_Internal:       linkage_str = "Internal"; break;
		case CXLinkage_UniqueExternal: linkage_str = "UniqueExternal"; break;
		case CXLinkage_External:       linkage_str = "External"; break;
		default:                       linkage_str = "Unknown"; break;
	}
	return linkage_str;
}

enum CXChildVisitResult
cs_source_visitor(
	CXCursor cursor,
	CXCursor parent,
	CXClientData data)
{
	CXSourceLocation location = clang_getCursorLocation(cursor);
	CXFile file;
	long depth = (long)data;
	unsigned int line, column, offs;

	clang_getSpellingLocation(location, &file, &line, &column, &offs);

	CXString path_cxstr;
	CXString usr_cxstr;
	CXString name_cxstr;
	CXString type_cxstr;
	CXString kind_cxstr;

	const char *path_str = clang_getCString(path_cxstr = clang_getFileName(file));
	const char *usr_str = clang_getCString(usr_cxstr = clang_getCursorUSR(cursor));
	const char *name_str = clang_getCString(name_cxstr = clang_getCursorSpelling(cursor));
	const char *type_str = clang_getCString(type_cxstr = clang_getTypeKindSpelling(clang_getCursorType(cursor).kind));
	const char *kind_str = clang_getCString(kind_cxstr = clang_getCursorKindSpelling(clang_getCursorKind(cursor)));

	printf("%*c""|-"
		"Referenced USR: %s, cursor_kind: %s, "
		"cursor_linkage: %s, "
		"cursor_type: %s, spelling: %s, "
		"line: %d, column: %d, offs: %d, "
		"path: %s, isCursorDefinition: %s\n",
		(int)depth, ' ',
		usr_str, kind_str,
		cs_source_cursor_linkage_str(cursor),
		type_str, name_str,
		line, column, offs,
		path_str, clang_isCursorDefinition(cursor)?"Yes":"No");

	clang_disposeString(path_cxstr);
	clang_disposeString(usr_cxstr);
	clang_disposeString(name_cxstr);
	clang_disposeString(type_cxstr);
	clang_disposeString(kind_cxstr);

	clang_visitChildren(cursor, cs_source_visitor, (CXClientData)(depth+1));
	return CXChildVisit_Continue;
}

#ifdef CLANG_SOURCE_INDEXER_TEST

static void usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s "
			"<The output AST> "
			"<Arguments passed to libclang>\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	int n;
	int nr_errors = 0;
	CXIndex cxindex;
	CXTranslationUnit tu;
	const char *ast_path = argv[1];

	if (argc < 3)
		usage(argc, argv);

	cxindex = clang_createIndex(0, 0);
	tu = clang_parseTranslationUnit(
			cxindex, 0,
			argv + 2, argc - 2, // Skip over AST
			0, 0,
			CXTranslationUnit_None);

	n = clang_getNumDiagnostics(tu);
	if (n > 0) {
		int i;
		for (i = 0; i < n; ++i) {
			CXDiagnostic diag = clang_getDiagnostic(tu, i);
			CXString string = clang_formatDiagnostic(diag,
						clang_defaultDiagnosticDisplayOptions());
			fprintf(stderr, "%s\n", clang_getCString(string));
			if (clang_getDiagnosticSeverity(diag) == CXDiagnostic_Error
					|| clang_getDiagnosticSeverity(diag) == CXDiagnostic_Fatal)
				nr_errors++;
		}
	}

	if (!nr_errors) {
		int ret;
		ret = clang_saveTranslationUnit(tu, ast_path, CXSaveTranslationUnit_None);
		if (ret != CXSaveError_None) {
			fprintf(stderr, "Failed to save AST to %s. Return value: %d\n",
				ast_path, ret);
			goto out;
		}
	}

	clang_visitChildren(
		clang_getTranslationUnitCursor(tu),
		cs_source_visitor,
		NULL);

out:
	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(cxindex);
	return 0;
}

#endif /* CLANG_SOURCE_INDEXER_TEST */
