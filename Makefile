#
# TCP Proxy Server
# ~~~~~~~~~~~~~~~~~~~
#
# Copyright (c) 2007 Arash Partow (http://www.partow.net)
# URL: http://www.partow.net/programming/tcpproxy/index.html
#
# Distributed under the Boost Software License, Version 1.0.
#


COMPILER         = $(CXX)
OPTIMIZATION_OPT = -O3
OPTIONS          = -pedantic -ansi -Wall -Werror $(OPTIMIZATION_OPT) -g
PTHREAD          = -lpthread
LINKER_OPT       = -lstdc++ $(PTHREAD) -lboost_thread -lboost_system -levent

BUILD_LIST+=tcpproxy

all: $(BUILD_LIST)

tcpproxy: tcpproxy.cpp
	$(COMPILER) $(OPTIONS) $(EXTA_CFLAGS) -o tcpproxy tcpproxy.cpp $(LINKER_OPT)

strip_bin :
	strip -s tcpproxy

clean:
	rm -f core *.o *.bak *~ *stackdump *#
