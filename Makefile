GUI = gtk
FLAGS = -std=gnu11 -O0 -ggdb3 -march=native -Wall -Wno-pointer-sign -Wno-switch -Werror -DMAKEDEF
TFLAGS = ${FLAGS} -DTESTS -DMAKEDEF
LFLAGS = -lsqlite3 -ljson-c -lssl -lcrypto
builddir = build
objects = ${builddir}/main.o ${builddir}/sql.o ${builddir}/util.o ${builddir}/crypt.o ${builddir}/1pass.o ${builddir}/gui.o
ifneq (,$(findstring gtk,$(GUI)))
	gobjects += ${builddir}/gui/gui-gtk.o
	FLAGS += -DGFX_GTK
	gINCFLAGS = $(shell pkg-config --cflags gtk+-3.0)
	gLFLAGS = $(shell pkg-config --libs gtk+-3.0)
endif
ifneq (,$(findstring curses,$(GUI)))
	gobjects += ${builddir}/gui/gui-curses.o
	FLAGS += -DGFX_CURSES
endif
ifneq (,$(findstring kde,$(GUI)))
	gobjects += ${builddir}/gui/gui-kde.o
	FLAGS += -DGFX_KDE
endif
Tobjects = $(subst .o,-test.o,${objects})
#Tobjects = ${builddir}/main-test.o ${builddir}/sql-test.o ${builddir}/util-test.o ${builddir}/crypt-test.o ${builddir}/1pass-test.o
Tgobjects = $(subst .o,-test.o,${gobjects})

all: cpass

cpass: Makefile ${objects} ${gobjects}
	${CC} -o cpass ${objects} ${gobjects} ${LFLAGS} ${gLFLAGS}

cpass-test: Makefile ${Tobjects} ${Tgobjects}
	${CC} -o cpass-test ${Tobjects} ${Tgobjects} ${LFLAGS} ${gLFLAGS}

${builddir}/.file:
	mkdir ${builddir}
	touch ${builddir}/.file

${builddir}/gui/.file:
	mkdir ${builddir}/gui
	touch ${builddir}/gui/.file

${builddir}/%-test.o: src/%.c Makefile $(wildcard src/*.h) ${builddir}/.file
	${CC} ${TFLAGS} -c -o $@ $<

${builddir}/%.o: src/%.c Makefile $(wildcard src/*.h) ${builddir}/.file
	${CC} ${FLAGS} -c -o $@ $<

${builddir}/gui/%.o: src/gui/%.c Makefile $(wildcard src/*.h) $(wildcard src/gui/*.h) ${builddir}/gui/.file
	${CC} ${FLAGS} ${gINCFLAGS} -c -o $@ $<

${builddir}/gui/%-test.o: src/gui/%.c Makefile $(wildcard src/*.h) $(wildcard src/gui/*.h) ${builddir}/gui/.file
	${CC} ${FLAGS} ${gINCFLAGS} -c -o $@ $<

clean:
	-rm -rf ${builddir}
	-rm cpass
	-rm cpass-test

run:
	make clean
	make all
	./cpass 1password10.sqlite

test: cpass-test
	./cpass-test

debug: cpass
	gdb ./cpass

.PHONY: clean run all test
