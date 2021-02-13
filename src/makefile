.DEFAULT_GOAL := romog

IDIR = ../include
CXX = g++
CPPFLAGS = -std=c++17 -g -fsanitize=address -O2 -I$(IDIR)

ODIR = obj
LDIR = ../libs

LIBS = -lcrypto -lpugixml -lxalan-c -lxerces-c -lstdc++fs -larchive -lyaml-cpp -lcurl

_DEPS = archive.h cache.h dat.h dir2dat.h fixdat.h gethashes.h interface.h paths.h rebuilder.h scanner.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o archive.o cache.o dat.o dir2dat.o fixdat.o gethashes.o interface.o rebuilder.o scanner.o docopt.o fort.o progress_bar.o zip.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CPPFLAGS) $(LIBS)

$(ODIR)/progress_bar.o: $(LDIR)/cpp_progress_bar/progress_bar.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CPPFLAGS)

$(ODIR)/docopt.o: $(LDIR)/docopt/docopt.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CPPFLAGS)

$(ODIR)/fort.o: $(LDIR)/libfort/lib/fort.c $(DEPS)
	$(CXX) -c -o $@ $< $(CPPFLAGS)

$(ODIR)/zip.o: $(LDIR)/zip/src/zip.c $(DEPS)
	$(CXX) -c -o $@ $< $(CPPFLAGS)

romog: $(OBJ)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LIBS)

clean:
	rm -f $(ODIR)/*.o romog

install:
	cp -f romog /usr/local/bin
	chmod 755 /usr/local/bin/romog

uninstall:
	rm -f /usr/local/bin/romog

.PHONY: clean