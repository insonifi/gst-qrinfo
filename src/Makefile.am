# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstqrplugin.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstqrplugin_la_SOURCES = gstqr.c gstqr.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstqrplugin_la_CFLAGS = $(GST_CFLAGS)
libgstqrplugin_la_LIBADD = $(GST_LIBS)
libgstqrplugin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstqrplugin_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstplugin.h
