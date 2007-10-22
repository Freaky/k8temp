
CC?= cc
RM?= rm
CFLAGS?= -O -Wall -pedantic -ansi
PREFIX?=/usr/local

all: k8temp

k8temp: k8temp.c
	${CC} ${CFLAGS} k8temp.c -o k8temp

clean:
	${RM} -f k8temp

