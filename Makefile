X=.exe
S=.dll

all: wintty$X

test: wintty$X test-host$X
	./wintty$X test-host$X

wintty-hook$S: wintty-hook.o
	$(LINK.c) $^ -shared $(LOADLIBES) $(LDLIBS) -o $@

wintty$X: wintty.o wintty-hook$S
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

test-host$X: test-host.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

