FLAGS = -std=gnu11 -O0 -ggdb3 -march=native -Werror
TFLAGS = ${FLAGS} -DTESTS
LFLAGS = -lsqlite3 -ljson-c -lssl -lcrypto
builddir = build
objects = ${builddir}/main.o ${builddir}/sql.o ${builddir}/util.o ${builddir}/crypt.o ${builddir}/1pass.o
Tobjects = ${builddir}/main-test.o ${builddir}/sql-test.o ${builddir}/util-test.o ${builddir}/crypt-test.o ${builddir}/1pass-test.o

all: cpass

cpass: Makefile ${objects}
	${CC} -o cpass ${objects} ${LFLAGS}

cpass-test: Makefile ${Tobjects}
	${CC} -o cpass-test ${Tobjects} ${LFLAGS}

${builddir}/.file:
	mkdir ${builddir}
	touch ${builddir}/.file

${builddir}/%-test.o: src/%.c Makefile $(wildcard src/*.h) ${builddir}/.file
	${CC} ${TFLAGS} -c -o $@ $<

${builddir}/%.o: src/%.c Makefile $(wildcard src/*.h) ${builddir}/.file
	${CC} ${FLAGS} -c -o $@ $<

clean:
	-rm -rf ${builddir}

run:
	make clean
	make all
	./cpass 1password10.sqlite

test: cpass-test
	./cpass-test

debug: cpass
	gdb ./cpass

.PHONY: clean run all test
