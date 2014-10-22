noinst_PROGRAMS = test_simple
test_simple_SOURCES = test_simple.c
test_simple_CFLAGS = \
	$(DEPS_CFLAGS) \
	$(COLLATION_CFLAGS) \
	-I$(top_builddir)/src/media-export -I$(top_srcdir)/src/media-export
test_simple_LDADD = \
	$(DEPS_LIBS) \
	$(COLLATION_LIBS) \
	$(top_builddir)/src/media-export/librygel-media-export-gst-0-10.la


check_PROGRAMS = test_simple
TESTS = test_simple
