/*
 * Copyright (c) 2018 Jeff Boody
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "xml_ostream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#define LOG_TAG "xml"
#include "../libcc/cc_log.h"
#include "../libcc/cc_memory.h"

/***********************************************************
* private                                                  *
***********************************************************/

// internal mode
#define XML_OSTREAM_MODE_FILE   0
#define XML_OSTREAM_MODE_GZFILE 1
#define XML_OSTREAM_MODE_BUFFER 2

// internal state
#define XML_OSTREAM_STATE_INIT    0
#define XML_OSTREAM_STATE_BODY    1
#define XML_OSTREAM_STATE_NESTED  2
#define XML_OSTREAM_STATE_CONTENT 3
#define XML_OSTREAM_STATE_EOF     4

static void xml_ostream_close(xml_ostream_t* self)
{
	ASSERT(self);

	if((self->mode == XML_OSTREAM_MODE_FILE) &&
	   self->of.close)
	{
		char pname[256];
		snprintf(pname, 256, "%s.part", self->of.fname);

		fclose(self->of.f);
		if(self->error)
		{
			unlink(pname);
		}
		else
		{
			rename(pname, self->of.fname);
		}
		self->of.close = 0;
	}
	else if((self->mode == XML_OSTREAM_MODE_GZFILE) &&
	        self->oz.close)
	{
		char pname[256];
		snprintf(pname, 256, "%s.part", self->oz.gzname);

		gzclose(self->oz.f);
		if(self->error)
		{
			unlink(pname);
		}
		else
		{
			rename(pname, self->oz.gzname);
		}
		self->oz.close = 0;
	}
}

static int xml_ostream_write(xml_ostream_t* self,
                             const char* buf)
{
	ASSERT(self);
	ASSERT(buf);

	// ignore writes on error
	if(self->error)
	{
		return 0;
	}

	int len = strlen(buf);
	if(self->mode == XML_OSTREAM_MODE_FILE)
	{
		if(fwrite(buf, len*sizeof(char), 1, self->of.f) != 1)
		{
			LOGE("fwrite failed");
			self->error = 1;
			return 0;
		}
	}
	else if(self->mode == XML_OSTREAM_MODE_GZFILE)
	{
		while(len > 0)
		{
			int bytes_written = gzwrite(self->oz.f, (const void*) buf, len);
			if(bytes_written == 0)
			{
				LOGE("gzwrite failed");
				self->error = 1;
				return 0;
			}
			buf += bytes_written;
			len -= bytes_written;
		}
	}
	else
	{
		int len2  = len + self->ob.len;
		int len21 = len2 + 1;
		char* buffer = (char*)
		               REALLOC(self->ob.buffer,
		                       len21*sizeof(char));
		if(buffer == NULL)
		{
			LOGE("relloc failed");
			self->error = 1;
			return 0;
		}
		self->ob.buffer = buffer;

		char* dst = &(self->ob.buffer[self->ob.len]);
		memcpy(dst, buf, len);
		self->ob.buffer[len2] = '\0';
		self->ob.len = len2;
	}

	return 1;
}

static int xml_ostream_writef(xml_ostream_t* self,
                              const char* fmt, ...)
{
	ASSERT(self);
	ASSERT(fmt);

	char buf[256];
	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(buf, 256, fmt, argptr);
	va_end(argptr);

	return xml_ostream_write(self, buf);
}

static int xml_ostream_indent(xml_ostream_t* self)
{
	ASSERT(self);

	int i;
	for(i = 0; i < self->depth; ++i)
	{
		if(xml_ostream_write(self, "\t") == 0)
		{
			return 0;
		}
	}

	return 1;
}

static int xml_ostream_endln(xml_ostream_t* self)
{
	ASSERT(self);

	return xml_ostream_write(self, "\n");
}

static int xml_ostream_filter(xml_ostream_t* self,
                              const char* a,
                              char* b)
{
	ASSERT(a);
	ASSERT(b);

	// initialize b
	b[0] = '\0';

	int i   = 0;
	int len = 0;
	while(1)
	{
		// eat invalid characters
		if((a[i] == '\n') ||
		   (a[i] == '\t') ||
		   (a[i] == '\r'))
		{
			++i;
			continue;
		}

		// check for word boundary
		if(a[i] == '\0')
		{
			return 1;
		}

		if(a[i] == '&')
		{
			if((len + 5) >= 256)
			{
				LOGE("invalid %s", a);
				self->error = 1;
				return 0;
			}

			b[len]     = '&';
			b[len + 1] = 'a';
			b[len + 2] = 'm';
			b[len + 3] = 'p';
			b[len + 4] = ';';
			b[len + 5] = '\0';
			len += 5;
			++i;
		}
		else if(a[i] == '"')
		{
			if((len + 6) >= 256)
			{
				LOGE("invalid %s", a);
				self->error = 1;
				return 0;
			}

			b[len]     = '&';
			b[len + 1] = 'q';
			b[len + 2] = 'u';
			b[len + 3] = 'o';
			b[len + 4] = 't';
			b[len + 5] = ';';
			b[len + 6] = '\0';
			len += 6;
			++i;
		}
		else if(a[i] == '\'')
		{
			if((len + 6) >= 256)
			{
				LOGE("invalid %s", a);
				self->error = 1;
				return 0;
			}

			b[len]     = '&';
			b[len + 1] = 'a';
			b[len + 2] = 'p';
			b[len + 3] = 'o';
			b[len + 4] = 's';
			b[len + 5] = ';';
			b[len + 6] = '\0';
			len += 6;
			++i;
		}
		else if(a[i] == '<')
		{
			if((len + 4) >= 256)
			{
				LOGE("invalid %s", a);
				self->error = 1;
				return 0;
			}

			b[len]     = '&';
			b[len + 1] = 'l';
			b[len + 2] = 't';
			b[len + 3] = ';';
			b[len + 4] = '\0';
			len += 4;
			++i;
		}
		else if(a[i] == '>')
		{
			if((len + 4) >= 256)
			{
				LOGE("invalid %s", a);
				self->error = 1;
				return 0;
			}

			b[len]     = '&';
			b[len + 1] = 'g';
			b[len + 2] = 't';
			b[len + 3] = ';';
			b[len + 4] = '\0';
			len += 4;
			++i;
		}
		else
		{
			if((len + 1) >= 256)
			{
				LOGE("invalid %s", a);
				self->error = 1;
				return 0;
			}

			// append character to word
			b[len]     = a[i];
			b[len + 1] = '\0';
			++len;
			++i;
		}
	}
}

static int xml_ostream_elemPush(xml_ostream_t* self,
                                const char* name)
{
	ASSERT(self);
	ASSERT(name);

	xml_ostreamElem_t* elem = (xml_ostreamElem_t*)
	                          MALLOC(sizeof(xml_ostreamElem_t));
	if(elem == NULL)
	{
		LOGE("MALLOC failed");
		self->error = 1;
		return 0;
	}

	snprintf(elem->name, 256, "%s", name);
	elem->next = self->elem;
	self->elem = elem;
	return 1;
}

static void xml_ostream_elemPop(xml_ostream_t* self)
{
	ASSERT(self);

	if(self->elem)
	{
		xml_ostreamElem_t* elem = self->elem;
		self->elem = self->elem->next;
		FREE(elem);
	}
}

static const char* xml_ostream_elemPeek(xml_ostream_t* self)
{
	ASSERT(self);

	if(self->elem)
	{
		return self->elem->name;
	}

	return NULL;
}

/***********************************************************
* public                                                   *
***********************************************************/

xml_ostream_t* xml_ostream_new(const char* fname)
{
	ASSERT(fname);

	xml_ostream_t* self = (xml_ostream_t*)
	                      MALLOC(sizeof(xml_ostream_t));
	if(self == NULL)
	{
		LOGE("MALLOC failed");
		return NULL;
	}

	char pname[256];
	snprintf(pname, 256, "%s.part", fname);
	snprintf(self->of.fname, 256, "%s", fname);

	FILE* f = fopen(pname, "w");
	if(f == NULL)
	{
		LOGE("fopen %s failed", pname);
		goto fail_fopen;
	}

	self->mode     = XML_OSTREAM_MODE_FILE;
	self->state    = XML_OSTREAM_STATE_INIT;
	self->error    = 0;
	self->depth    = 0;
	self->elem     = NULL;
	self->of.f     = f;
	self->of.close = 1;

	// success
	return self;

	// failure
	fail_fopen:
		FREE(self);
	return NULL;
}

xml_ostream_t* xml_ostream_newGz(const char* gzname)
{
	ASSERT(gzname);

	xml_ostream_t* self = (xml_ostream_t*)
	                      MALLOC(sizeof(xml_ostream_t));
	if(self == NULL)
	{
		LOGE("MALLOC failed");
		return NULL;
	}

	char pname[256];
	snprintf(pname, 256, "%s.part", gzname);
	snprintf(self->oz.gzname, 256, "%s", gzname);

	gzFile f = gzopen(pname, "wb");
	if(f == NULL)
	{
		LOGE("gzopen %s failed", pname);
		goto fail_gzopen;
	}

	self->mode     = XML_OSTREAM_MODE_GZFILE;
	self->state    = XML_OSTREAM_STATE_INIT;
	self->error    = 0;
	self->depth    = 0;
	self->elem     = NULL;
	self->oz.f     = f;
	self->oz.close = 1;

	// success
	return self;

	// failure
	fail_gzopen:
		FREE(self);
	return NULL;
}

xml_ostream_t* xml_ostream_newFile(FILE* f)
{
	ASSERT(f);

	xml_ostream_t* self = (xml_ostream_t*)
	                      MALLOC(sizeof(xml_ostream_t));
	if(self == NULL)
	{
		LOGE("MALLOC failed");
		return NULL;
	}

	self->mode     = XML_OSTREAM_MODE_FILE;
	self->state    = XML_OSTREAM_STATE_INIT;
	self->error    = 0;
	self->depth    = 0;
	self->elem     = NULL;
	self->of.f     = f;
	self->of.close = 0;

	return self;
}

xml_ostream_t* xml_ostream_newBuffer(void)
{
	xml_ostream_t* self = (xml_ostream_t*)
	                      MALLOC(sizeof(xml_ostream_t));
	if(self == NULL)
	{
		LOGE("MALLOC failed");
		return NULL;
	}

	self->mode      = XML_OSTREAM_MODE_BUFFER;
	self->state     = XML_OSTREAM_STATE_INIT;
	self->error     = 0;
	self->depth     = 0;
	self->elem      = NULL;
	self->ob.buffer = NULL;
	self->ob.len    = 0;

	return self;
}

void xml_ostream_delete(xml_ostream_t** _self)
{
	ASSERT(_self);

	xml_ostream_t* self = *_self;
	if(self)
	{
		if(self->mode == XML_OSTREAM_MODE_BUFFER)
		{
			FREE(self->ob.buffer);
		}
		else
		{
			xml_ostream_close(self);
		}

		while(self->elem)
		{
			xml_ostream_elemPop(self);
		}

		FREE(self);
		*_self = NULL;
	}
}

int xml_ostream_begin(xml_ostream_t* self,
                      const char* name)
{
	ASSERT(self);
	ASSERT(name);

	if(self->state == XML_OSTREAM_STATE_INIT)
	{
		if(xml_ostream_write(self, "<?xml version='1.0' encoding='UTF-8'?>") == 0)
		{
			return 0;
		}
	}

	if(xml_ostream_elemPush(self, name) == 0)
	{
		return 0;
	}

	if(self->state == XML_OSTREAM_STATE_INIT)
	{
		if(xml_ostream_endln(self) &&
		   xml_ostream_writef(self, "<%s", name))
		{
			self->state = XML_OSTREAM_STATE_BODY;
			++self->depth;
			return 1;
		}
	}
	else if(self->state == XML_OSTREAM_STATE_BODY)
	{
		if(xml_ostream_write(self, ">") &&
		   xml_ostream_endln(self)      &&
		   xml_ostream_indent(self)     &&
		   xml_ostream_writef(self, "<%s", name))
		{
			++self->depth;
			return 1;
		}
	}
	else if(self->state == XML_OSTREAM_STATE_NESTED)
	{
		if(xml_ostream_endln(self)  &&
		   xml_ostream_indent(self) &&
		   xml_ostream_writef(self, "<%s", name))
		{
			self->state = XML_OSTREAM_STATE_BODY;
			++self->depth;
			return 1;
		}
	}
	else
	{
		LOGE("invalid state=%i", self->state);
	}

	xml_ostream_elemPop(self);
	self->error = 1;
	return 0;
}

int xml_ostream_end(xml_ostream_t* self)
{
	ASSERT(self);

	const char* name = xml_ostream_elemPeek(self);
	if(name == NULL)
	{
		LOGE("invalid elem");
		self->error = 1;
		return 0;
	}

	if(self->state == XML_OSTREAM_STATE_BODY)
	{
		--self->depth;
		if(self->depth == 0)
		{
			self->state = XML_OSTREAM_STATE_EOF;
		}
		else
		{
			self->state = XML_OSTREAM_STATE_NESTED;
		}

		if(xml_ostream_write(self, " />"))
		{
			xml_ostream_elemPop(self);
			return 1;
		}
	}
	else if(self->state == XML_OSTREAM_STATE_NESTED)
	{
		--self->depth;
		if(self->depth == 0)
		{
			self->state = XML_OSTREAM_STATE_EOF;
		}

		if(xml_ostream_endln(self)  &&
		   xml_ostream_indent(self) &&
		   xml_ostream_writef(self, "</%s>", name))
		{
			xml_ostream_elemPop(self);
			return 1;
		}
	}
	else if(self->state == XML_OSTREAM_STATE_CONTENT)
	{
		--self->depth;
		if(self->depth == 0)
		{
			self->state = XML_OSTREAM_STATE_EOF;
		}
		else
		{
			self->state = XML_OSTREAM_STATE_NESTED;
		}

		if(xml_ostream_writef(self, "</%s>", name))
		{
			xml_ostream_elemPop(self);
			return 1;
		}
	}
	else
	{
		LOGE("invalid state=%i", self->state);
	}

	self->error = 1;
	return 0;
}

int xml_ostream_attr(xml_ostream_t* self,
                     const char* name,
                     const char* val)
{
	ASSERT(self);
	ASSERT(name);
	ASSERT(val);

	if(self->state == XML_OSTREAM_STATE_BODY)
	{
		char val2[256];
		xml_ostream_filter(self, val, val2);
		return xml_ostream_writef(self, " %s=\"%s\"",
		                          name, val2);
	}
	else
	{
		LOGE("invalid name=%s, state=%i", name, self->state);
		self->error = 1;
		return 0;
	}
}

int xml_ostream_attrf(xml_ostream_t* self,
                      const char* name,
                      const char* fmt, ...)
{
	ASSERT(self);
	ASSERT(name);
	ASSERT(fmt);

	char val[256];
	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(val, 256, fmt, argptr);
	va_end(argptr);

	return xml_ostream_attr(self, name, val);
}

int xml_ostream_content(xml_ostream_t* self,
                        const char* content)
{
	ASSERT(self);
	ASSERT(content);

	// filter the content
	char filtered[256];
	xml_ostream_filter(self, content, filtered);

	if(self->state == XML_OSTREAM_STATE_BODY)
	{
		self->state = XML_OSTREAM_STATE_CONTENT;

		if(xml_ostream_write(self, ">") &&
		   xml_ostream_write(self, filtered))
		{
			return 1;
		}
	}
	else if(self->state == XML_OSTREAM_STATE_CONTENT)
	{
		if(xml_ostream_write(self, filtered))
		{
			return 1;
		}
	}
	else
	{
		LOGE("invalid state=%i", self->state);
	}

	self->error = 1;
	return 0;
}

int xml_ostream_contentf(xml_ostream_t* self,
                         const char* fmt, ...)
{
	ASSERT(self);
	ASSERT(fmt);

	char val[256];
	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(val, 256, fmt, argptr);
	va_end(argptr);

	return xml_ostream_content(self, val);
}

const char* xml_ostream_buffer(xml_ostream_t* self,
                               int acquire,
                               int* len)
{
	ASSERT(self);
	ASSERT(len);

	if(xml_ostream_complete(self) &&
	   (self->mode == XML_OSTREAM_MODE_BUFFER))
	{
		*len = self->ob.len;
		char* buffer = self->ob.buffer;
		if(acquire)
		{
			self->ob.len    = 0;
			self->ob.buffer = NULL;
		}
		return buffer;
	}
	else
	{
		return NULL;
	}
}

int xml_ostream_complete(xml_ostream_t* self)
{
	ASSERT(self);

	if(self->state == XML_OSTREAM_STATE_EOF)
	{
		xml_ostream_close(self);
		return self->error ? 0 : 1;
	}

	return 0;
}
