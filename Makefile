vpath %.c src

X=.exe

.PHONY: clean all

all: wintty$X

clean:
	$(RM) wintty$X test-host$X wintty.o test-host.o

test: wintty$X test-host$X
	./wintty$X test-host$X

wintty$X: wintty.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

test-host$X: test-host.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

