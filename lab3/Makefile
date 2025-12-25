TARGET = main
SRCS = http_parser.c main.c cache/cache.c cache/downloader.c

CC=gcc
RM=rm
CFLAGS= -g -Wall
LIBS=-lpthread
INCLUDE_DIR="."

all: ${TARGET}

${TARGET}: proxy.h http_parser.h cache/cache.h cache/downloader.h ${SRCS}
	${CC} ${CFLAGS} -I${INCLUDE_DIR} ${SRCS} ${LIBS} -o ${TARGET}

clean:
	${RM} -f *.o ${TARGET}
