TARGET   = test-ostream
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Llibxmlstream -lxmlstream -lpthread -lm
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) xmlstream
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: xmlstream

xmlstream:
	$(MAKE) -C libxmlstream

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libxmlstream clean
	rm libxmlstream

$(OBJECTS): $(HFILES)
