#include "cache.h"
#include "string-list.h"

/*
 * A "string_list_each_func_t" function that normalizes an entry from
 * GIT_CEILING_DIRECTORIES.  If the path is unusable for some reason,
 * die with an explanation.
 */
static int normalize_ceiling_entry(struct string_list_item *item, void *unused)
{
	const char *ceil = item->string;
	int len = strlen(ceil);
	char buf[PATH_MAX+1];

	if (len == 0)
		die("Empty path is not supported");
	if (len > PATH_MAX)
		die("Path \"%s\" is too long", ceil);
	if (!is_absolute_path(ceil))
		die("Path \"%s\" is not absolute", ceil);
	if (normalize_path_copy(buf, ceil) < 0)
		die("Path \"%s\" could not be normalized", ceil);
	len = strlen(buf);
	free(item->string);
	item->string = xstrdup(buf);
	return 1;
}

static void normalize_argv_string(const char **var, const char *input)
{
	if (!strcmp(input, "<null>"))
		*var = NULL;
	else if (!strcmp(input, "<empty>"))
		*var = "";
	else
		*var = input;

	if (*var && (**var == '<' || **var == '('))
		die("Bad value: %s\n", input);
}

struct test_data {
	char *from;  /* input:  transform from this ... */
	char *to;    /* output: ... to this.            */
};

static int test_function(struct test_data *data, char *(*func)(char *input),
	const char *funcname)
{
	int failed = 0, i;
	static char buffer[1024];
	char *to;

	for (i = 0; data[i].to; i++) {
		if (!data[i].from)
			to = func(NULL);
		else {
			strcpy(buffer, data[i].from);
			to = func(buffer);
		}
		if (strcmp(to, data[i].to)) {
			error("FAIL: %s(%s) => '%s' != '%s'\n",
				funcname, data[i].from, to, data[i].to);
			failed++;
		}
	}
	return !!failed;
}

static struct test_data basename_data[] = {
	/* --- POSIX type paths --- */
	{ NULL,              "."    },
	{ "",                "."    },
	{ ".",               "."    },
	{ "..",              ".."   },
	{ "/",               "/"    },
#if defined(__CYGWIN__) && !defined(NO_LIBGEN_H)
	{ "//",              "//"   },
	{ "///",             "//"   },
	{ "////",            "//"   },
#else
	{ "//",              "/"    },
	{ "///",             "/"    },
	{ "////",            "/"    },
#endif
	{ "usr",             "usr"  },
	{ "/usr",            "usr"  },
	{ "/usr/",           "usr"  },
	{ "/usr//",          "usr"  },
	{ "/usr/lib",        "lib"  },
	{ "usr/lib",         "lib"  },
	{ "usr/lib///",      "lib"  },

#if defined(__MINGW32__) || defined(_MSC_VER)

	/* --- win32 type paths --- */
	{ "\\usr",           "usr"  },
	{ "\\usr\\",         "usr"  },
	{ "\\usr\\\\",       "usr"  },
	{ "\\usr\\lib",      "lib"  },
	{ "usr\\lib",        "lib"  },
	{ "usr\\lib\\\\\\",  "lib"  },
	{ "C:/usr",          "usr"  },
	{ "C:/usr",          "usr"  },
	{ "C:/usr/",         "usr"  },
	{ "C:/usr//",        "usr"  },
	{ "C:/usr/lib",      "lib"  },
	{ "C:usr/lib",       "lib"  },
	{ "C:usr/lib///",    "lib"  },
	{ "C:",              "."    },
	{ "C:a",             "a"    },
	{ "C:/",             "/"    },
	{ "C:///",           "/"    },
#if defined(NO_LIBGEN_H)
	{ "\\",              "\\"   },
	{ "\\\\",            "\\"   },
	{ "\\\\\\",          "\\"   },
#else

	/* win32 platform variations: */
#if defined(__MINGW32__)
	{ "\\",              "/"    },
	{ "\\\\",            "/"    },
	{ "\\\\\\",          "/"    },
#endif

#if defined(_MSC_VER)
	{ "\\",              "\\"   },
	{ "\\\\",            "\\"   },
	{ "\\\\\\",          "\\"   },
#endif

#endif
#endif
	{ NULL,              "."    },
	{ NULL,              NULL   }
};

static struct test_data dirname_data[] = {
	/* --- POSIX type paths --- */
	{ NULL,              "."      },
	{ "",                "."      },
	{ ".",               "."      },
	{ "..",              "."      },
	{ "/",               "/"      },
	{ "//",              "//"     },
#if defined(__CYGWIN__) && !defined(NO_LIBGEN_H)
	{ "///",             "//"     },
	{ "////",            "//"     },
#else
	{ "///",             "/"      },
	{ "////",            "/"      },
#endif
	{ "usr",             "."      },
	{ "/usr",            "/"      },
	{ "/usr/",           "/"      },
	{ "/usr//",          "/"      },
	{ "/usr/lib",        "/usr"   },
	{ "usr/lib",         "usr"    },
	{ "usr/lib///",      "usr"    },

#if defined(__MINGW32__) || defined(_MSC_VER)

	/* --- win32 type paths --- */
	{ "\\",              "\\"     },
	{ "\\\\",            "\\\\"   },
	{ "\\usr",           "\\"     },
	{ "\\usr\\",         "\\"     },
	{ "\\usr\\\\",       "\\"     },
	{ "\\usr\\lib",      "\\usr"  },
	{ "usr\\lib",        "usr"    },
	{ "usr\\lib\\\\\\",  "usr"    },
	{ "C:a",             "C:."    },
	{ "C:/",             "C:/"    },
	{ "C:///",           "C:/"    },
	{ "C:/usr",          "C:/"    },
	{ "C:/usr/",         "C:/"    },
	{ "C:/usr//",        "C:/"    },
	{ "C:/usr/lib",      "C:/usr" },
	{ "C:usr/lib",       "C:usr"  },
	{ "C:usr/lib///",    "C:usr"  },
	{ "\\\\\\",          "\\"     },
	{ "\\\\\\\\",        "\\"     },
#if defined(NO_LIBGEN_H)
	{ "C:",              "C:."    },
#else

	/* win32 platform variations: */
#if defined(__MINGW32__)
	/* the following is clearly wrong ... */
	{ "C:",              "."      },
#endif

#if defined(_MSC_VER)
	{ "C:",              "C:."    },
#endif

#endif
#endif
	{ NULL,              "."      },
	{ NULL,              NULL     }
};

int main(int argc, char **argv)
{
	if (argc == 3 && !strcmp(argv[1], "normalize_path_copy")) {
		char *buf = xmalloc(PATH_MAX + 1);
		int rv = normalize_path_copy(buf, argv[2]);
		if (rv)
			buf = "++failed++";
		puts(buf);
		return 0;
	}

	if (argc >= 2 && !strcmp(argv[1], "real_path")) {
		while (argc > 2) {
			puts(real_path(argv[2]));
			argc--;
			argv++;
		}
		return 0;
	}

	if (argc >= 2 && !strcmp(argv[1], "absolute_path")) {
		while (argc > 2) {
			puts(absolute_path(argv[2]));
			argc--;
			argv++;
		}
		return 0;
	}

	if (argc == 4 && !strcmp(argv[1], "longest_ancestor_length")) {
		int len;
		struct string_list ceiling_dirs = STRING_LIST_INIT_DUP;
		char *path = xstrdup(argv[2]);

		/*
		 * We have to normalize the arguments because under
		 * Windows, bash mangles arguments that look like
		 * absolute POSIX paths or colon-separate lists of
		 * absolute POSIX paths into DOS paths (e.g.,
		 * "/foo:/foo/bar" might be converted to
		 * "D:\Src\msysgit\foo;D:\Src\msysgit\foo\bar"),
		 * whereas longest_ancestor_length() requires paths
		 * that use forward slashes.
		 */
		if (normalize_path_copy(path, path))
			die("Path \"%s\" could not be normalized", argv[2]);
		string_list_split(&ceiling_dirs, argv[3], PATH_SEP, -1);
		filter_string_list(&ceiling_dirs, 0,
				   normalize_ceiling_entry, NULL);
		len = longest_ancestor_length(path, &ceiling_dirs);
		string_list_clear(&ceiling_dirs, 0);
		free(path);
		printf("%d\n", len);
		return 0;
	}

	if (argc >= 4 && !strcmp(argv[1], "prefix_path")) {
		char *prefix = argv[2];
		int prefix_len = strlen(prefix);
		int nongit_ok;
		setup_git_directory_gently(&nongit_ok);
		while (argc > 3) {
			puts(prefix_path(prefix, prefix_len, argv[3]));
			argc--;
			argv++;
		}
		return 0;
	}

	if (argc == 4 && !strcmp(argv[1], "strip_path_suffix")) {
		char *prefix = strip_path_suffix(argv[2], argv[3]);
		printf("%s\n", prefix ? prefix : "(null)");
		return 0;
	}

	if (argc == 3 && !strcmp(argv[1], "print_path")) {
		puts(argv[2]);
		return 0;
	}

	if (argc == 4 && !strcmp(argv[1], "relative_path")) {
		struct strbuf sb = STRBUF_INIT;
		const char *in, *prefix, *rel;
		normalize_argv_string(&in, argv[2]);
		normalize_argv_string(&prefix, argv[3]);
		rel = relative_path(in, prefix, &sb);
		if (!rel)
			puts("(null)");
		else
			puts(strlen(rel) > 0 ? rel : "(empty)");
		strbuf_release(&sb);
		return 0;
	}

	if (argc == 2 && !strcmp(argv[1], "basename"))
		return test_function(basename_data, basename, argv[1]);

	if (argc == 2 && !strcmp(argv[1], "dirname"))
		return test_function(dirname_data, dirname, argv[1]);

	fprintf(stderr, "%s: unknown function name: %s\n", argv[0],
		argv[1] ? argv[1] : "(there was none)");
	return 1;
}
