TARGET   = libxmlstream.a
CLASS    = xml_ostream xml_istream
SOURCE   = $(CLASS:%=%.c)
OBJECTS  = $(SOURCE:.c=.o)
HFILES   = $(CLASS:%=%.h)
OPT      = -O2 -Wall -Wno-format-truncation
CFLAGS   = $(OPT) -I.
LDFLAGS  = -lm -L/usr/lib
AR       = ar

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)

$(OBJECTS): $(HFILES)
