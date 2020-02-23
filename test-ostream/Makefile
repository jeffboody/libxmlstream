TARGET   = test-ostream
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Llibxmlstream -lxmlstream -Llibcc -lcc -lz -lpthread -lm
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libcc xmlstream
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libcc xmlstream

libcc:
	$(MAKE) -C libcc

xmlstream:
	$(MAKE) -C libxmlstream

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libcc clean
	$(MAKE) -C libxmlstream clean
	rm libcc libxmlstream

$(OBJECTS): $(HFILES)
