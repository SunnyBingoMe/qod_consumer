ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libcon.la
bin_PROGRAMS = consumer-ls

if BUILD_GTK
lib_LTLIBRARIES += libglutils.la
bin_PROGRAMS += consumer-ip
endif

if BUILD_PYTHON
pyexec_LTLIBRARIES = consumer.la
endif

libcon_la_CFLAGS = -Wall ${libcap_stream_CFLAGS}
libcon_la_LIBADD = ${libcap_stream_LIBS} -lrt
libcon_la_SOURCES = consumer.c

libglutils_la_CXXFLAGS = -Wall
libglutils_la_LIBADD = -lGL -lGLU -lGLEW
libglutils_la_SOURCES = glutils.cpp 

consumer_la_CFLAGS = -Wall ${libcap_stream_CFLAGS} ${PYTHON_INCLUDE}
consumer_la_LDFLAGS = -avoid-version -module -pthread -lrt
consumer_la_LIBADD = libcon.la
consumer_la_SOURCES = \
	consumer-python.c \
	pyconsumer/frame.c \
	pyconsumer/iterator.c \
	pyconsumer/packet.c \
	pyconsumer/ethhdr.c \
	pyconsumer/iphdr.c \
	pyconsumer/tcphdr.c \
	pyconsumer/udphdr.c \
	pyconsumer/icmphdr.c

consumer_ls_CFLAGS = -Wall ${libcap_stream_CFLAGS}
consumer_ls_LDFLAGS = -pthread -lrt
consumer_ls_LDADD = libcon.la
consumer_ls_SOURCES = main.c

consumer_ip_CFLAGS = -Wall ${libcap_stream_CFLAGS} ${gtk_CFLAGS} ${gtkglext_CFLAGS} ${glib_CFLAGS}
consumer_ip_CXXFLAGS = ${consumer_ip_CFLAGS}
consumer_ip_LDFLAGS = -pthread -lrt ${gtk_LIBS} ${gtkglext_LIBS} ${glib_LIBS} ${libcap_stream_LIBS}
consumer_ip_LDADD = libcon.la libglutils.la -lSDL -lcap_filter
consumer_ip_SOURCES = ip.cpp
