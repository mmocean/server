AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = server client

server_SOURCES = log.c  multiprocess.c  multipthread.c socket.c server.c
server_LDADD = -lpthread

client_SOURCES = client.c

include_HEADERS = client.h  config.h  defs.h  log.h  multiprocess.h  multipthread.h  server.h  socket.h


