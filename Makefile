X=.exe

all: wintty$X

test: wintty$X test-host$X
	./wintty$X test-host$X

wintty$X: wintty.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

test-host$X: test-host.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

