plugindir = $(RYGEL_PLUGIN_DIR)
plugin_LTLIBRARIES = librygel-media-export-gst-0-10.la
plugin_DATA = media-export-gst-0-10.plugin

librygel_media_export_gst_0_10_la_SOURCES = \
	rygel-media-export-collate.c \
	rygel-media-export-collate.h \
	rygel-media-export-database.c \
	rygel-media-export-database-cursor.c \
	rygel-media-export-database-cursor.h \
	rygel-media-export-database.h \
	rygel-media-export-db-container.c \
	rygel-media-export-db-container.h \
	rygel-media-export-dummy-container.c \
	rygel-media-export-dummy-container.h \
	rygel-media-export-errors.c \
	rygel-media-export-errors.h \
	rygel-media-export-harvester.c \
	rygel-media-export-harvester.h \
	rygel-media-export-harvesting-task.c \
	rygel-media-export-harvesting-task.h \
	rygel-media-export-item-factory.c \
	rygel-media-export-item-factory.h \
	rygel-media-export-jpeg-writer.c \
	rygel-media-export-jpeg-writer.h \
	rygel-media-export-leaf-query-container.c \
	rygel-media-export-leaf-query-container.h \
	rygel-media-export-media-cache.c \
	rygel-media-export-media-cache.h \
	rygel-media-export-media-cache-upgrader.c \
	rygel-media-export-media-cache-upgrader.h \
	rygel-media-export-metadata-extractor.c \
	rygel-media-export-metadata-extractor.h \
	rygel-media-export-music-item.c \
	rygel-media-export-music-item.h \
	rygel-media-export-node-query-container.c \
	rygel-media-export-node-query-container.h \
	rygel-media-export-null-container.c \
	rygel-media-export-null-container.h \
	rygel-media-export-object-factory.c \
	rygel-media-export-object-factory.h \
	rygel-media-export-photo-item.c \
	rygel-media-export-photo-item.h \
	rygel-media-export-plugin.c \
	rygel-media-export-plugin.h \
	rygel-media-export-query-container.c \
	rygel-media-export-query-container-factory.c \
	rygel-media-export-query-container-factory.h \
	rygel-media-export-query-container.h \
	rygel-media-export-recursive-file-monitor.c \
	rygel-media-export-recursive-file-monitor.h \
	rygel-media-export-root-container.c \
	rygel-media-export-root-container.h \
	rygel-media-export-sql-factory.c \
	rygel-media-export-sql-factory.h \
	rygel-media-export-sql-function.c \
	rygel-media-export-sql-function.h \
	rygel-media-export-sqlite-wrapper.c \
	rygel-media-export-sqlite-wrapper.h \
	rygel-media-export-sql-operator.c \
	rygel-media-export-sql-operator.h \
	rygel-media-export-string-utils.c \
	rygel-media-export-string-utils.h \
	rygel-media-export-trackable-db-container.c \
	rygel-media-export-trackable-db-container.h \
	rygel-media-export-uuid.c \
	rygel-media-export-uuid.h \
	rygel-media-export-video-item.c \
	rygel-media-export-video-item.h \
	rygel-media-export-writable-db-container.c \
	rygel-media-export-writable-db-container.h

librygel_media_export_gst_0_10_la_CFLAGS = \
	-DG_LOG_DOMAIN='"MediaExport-GStreamer-0.10"' \
	-DPRESET_DIR='"$(presetdir)"' \
	-include config.h \
	$(DEPS_CFLAGS) \
	$(COLLATION_CFLAGS)

librygel_media_export_gst_0_10_la_LIBADD = \
	$(DEPS_LIBS) \
	$(COLLATION_LIBS)

librygel_media_export_gst_0_10_la_LDFLAGS = $(RYGEL_PLUGIN_LINKER_FLAGS)

EXTRA_DIST = media-export-gst-0-10.plugin.in
