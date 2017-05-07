GUI = gtk
FLAGS = -DDEBUG -ggdb3 -O0 -std=gnu11 -march=native -Wall -Wno-pointer-sign -Wno-switch -Werror
TFLAGS = ${FLAGS} -DTESTS
LFLAGS = -lsqlite3 -ljson-c -lssl -lcrypto
builddir = build
objects = ${builddir}/main.o ${builddir}/sql.o ${builddir}/util.o ${builddir}/crypt.o ${builddir}/1pass.o ${builddir}/gui.o
xmlobjects =
ifneq (,$(findstring gtk,$(GUI)))
	gobjects += ${builddir}/gui/gui-gtk.o
	xmlobjects += $(subst .xml,.pxml,$(wildcard src/gui/xml/gtk*.xml))
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

src/gui/xml/%.pxml: src/gui/xml/%.xml Makefile
	src/gui/xml/xmlpreprocess.sh $< $@

${builddir}/gui/%-test.o: src/gui/%.c Makefile $(wildcard src/*.h) $(wildcard src/gui/*.h) ${builddir}/gui/.file ${xmlobjects}
	${CC} ${FLAGS} ${gINCFLAGS} -c -o $@ $<

${builddir}/gui/%.o: src/gui/%.c Makefile $(wildcard src/*.h) $(wildcard src/gui/*.h) ${builddir}/gui/.file ${xmlobjects}
	${CC} ${FLAGS} ${gINCFLAGS} -c -o $@ $<

${builddir}/%-test.o: src/%.c Makefile $(wildcard src/*.h) ${builddir}/.file ${xmlobjects}
	${CC} ${TFLAGS} -c -o $@ $<

${builddir}/%.o: src/%.c Makefile $(wildcard src/*.h) ${builddir}/.file ${xmlobjects}
	${CC} ${FLAGS} -c -o $@ $<

clean:
	-rm -rf ${builddir}
	-rm cpass
	-rm cpass-test
	-rm src/gui/xml/*.pxml

run:
	make clean
	make all
	./cpass 1password10.sqlite

test: cpass-test
	./cpass-test

debug: cpass
	gdb ./cpass

gladeTest: gladeTest.c Makefile
	src/gui/xml/xmlpreprocess.sh src/gui/xml/aoeu.glade src/gui/xml/aoeu.pxml
	${CC} -o gladeTest gladeTest.c ${LFLAGS} ${gLFLAGS} ${FLAGS} ${gINCFLAGS}
	./gladeTest
	rm gladeTest

.PHONY: clean run all test
