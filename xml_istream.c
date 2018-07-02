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

#include "xml_istream.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define LOG_TAG "xml"
#include "xml_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

typedef struct
{
	int error;

	// callbacks
	void* priv;
	xml_istream_start_fn start_fn;
	xml_istream_end_fn   end_fn;

	// buffered content
	char* content_buf;
	int   content_len;

	// Expat parser
	XML_Parser parser;
} xml_istream_t;

static void xml_istream_start(void* _self,
                              const XML_Char* name,
                              const XML_Char** atts)
{
	assert(_self);
	assert(name);
	assert(atts);

	xml_istream_t* self = (xml_istream_t*) _self;
	xml_istream_start_fn start_fn = self->start_fn;

	int line = XML_GetCurrentLineNumber(self->parser);
	if((*start_fn)(self->priv, line, name, atts) == 0)
	{
		self->error = 1;
	}
}

static void xml_istream_end(void* _self,
                            const XML_Char* name)
{
	assert(_self);
	assert(name);

	xml_istream_t* self = (xml_istream_t*) _self;
	xml_istream_end_fn end_fn = self->end_fn;

	int line = XML_GetCurrentLineNumber(self->parser);

	// skip leading whitespace
	char* buf = self->content_buf;
	if(buf)
	{
		while(buf[0] != '\0')
		{
			char c = buf[0];
			if((c == '\t') ||
			   (c == '\n') ||
			   (c == '\r') ||
			   (c == ' '))
			{
				++buf;
			}
			else
			{
				break;
			}
		}
	}

	if((*end_fn)(self->priv, line, name, buf) == 0)
	{
		self->error = 1;
	}

	free(self->content_buf);
	self->content_buf = NULL;
	self->content_len = 0;
}

static void xml_istream_content(void *_self,
                                const char *content,
                                int len)
{
	assert(_self);
	assert(content);

	xml_istream_t* self = (xml_istream_t*) _self;

	int len2  = len + self->content_len;
	int len21 = len2 + 1;
	char* buffer = (char*)
	               realloc(self->content_buf,
	                       len21*sizeof(char));
	if(buffer == NULL)
	{
		LOGE("relloc failed");
		self->error = 1;
		return;
	}
	self->content_buf = buffer;

	char* dst = &(self->content_buf[self->content_len]);
	memcpy(dst, content, len);
	self->content_buf[len2] = '\0';
	self->content_len       = len2;
}

static xml_istream_t*
xml_istream_new(void* priv,
                xml_istream_start_fn start_fn,
                xml_istream_end_fn   end_fn)
{
	// priv may be NULL
	assert(start_fn);
	assert(end_fn);

	xml_istream_t* self = (xml_istream_t*)
	                      malloc(sizeof(xml_istream_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	XML_Parser parser = XML_ParserCreate("UTF-8");
	if(parser == NULL)
	{
		LOGE("XML_ParserCreate failed");
		goto fail_parser;
	}
	XML_SetUserData(parser, (void*) self);
	XML_SetElementHandler(parser,
	                      xml_istream_start,
	                      xml_istream_end);
	XML_SetCharacterDataHandler(parser,
	                            xml_istream_content);

	self->error       = 0;
	self->priv        = priv;
	self->start_fn    = start_fn;
	self->end_fn      = end_fn;
	self->content_buf = NULL;
	self->content_len = 0;
	self->parser      = parser;

	// success
	return self;

	// failure
	fail_parser:
		free(self);
	return NULL;
}

static void xml_istream_delete(xml_istream_t** _self)
{
	assert(_self);

	xml_istream_t* self = *_self;
	if(self)
	{
		XML_ParserFree(self->parser);
		free(self->content_buf);
		free(self);
		*_self = NULL;
	}
}

/***********************************************************
* public                                                   *
***********************************************************/

int xml_istream_parse(void* priv,
                      xml_istream_start_fn start_fn,
                      xml_istream_end_fn   end_fn,
                      const char* fname)
{
	// priv may be NULL
	assert(start_fn);
	assert(end_fn);
	assert(fname);

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		LOGE("fopen %s failed", fname);
		return 0;
	}

	// get file len
	if(fseek(f, (long) 0, SEEK_END) == -1)
	{
		LOGE("fseek_end fname=%s", fname);
		goto fail_fseek_end;
	}
	int len = (int) ftell(f);

	// rewind to start
	if(fseek(f, 0, SEEK_SET) == -1)
	{
		LOGE("fseek_set fname=%s", fname);
		goto fail_fseek_set;
	}

	int ret = xml_istream_parseFile(priv, start_fn,
	                                end_fn, f, len);
	fclose(f);

	// success
	return ret;

	// failure
	fail_fseek_set:
	fail_fseek_end:
		fclose(f);
	return 0;
}

int xml_istream_parseFile(void* priv,
                          xml_istream_start_fn start_fn,
                          xml_istream_end_fn   end_fn,
                          FILE* f, int len)
{
	// priv may be NULL
	assert(start_fn);
	assert(end_fn);
	assert(f);

	xml_istream_t* self = xml_istream_new(priv, start_fn, end_fn);
	if(self == NULL)
	{
		return 0;
	}

	// parse file
	int done = 0;
	while(done == 0)
	{
		void* buf = XML_GetBuffer(self->parser, 4096);
		if(buf == NULL)
		{
			LOGE("XML_GetBuffer buf=NULL");
			goto fail_parse;
		}

		int bytes = fread(buf, 1, len > 4096 ? 4096 : len, f);
		if(bytes < 0)
		{
			LOGE("read failed");
			goto fail_parse;
		}

		len -= bytes;
		done = (len == 0) ? 1 : 0;
		if(XML_ParseBuffer(self->parser, bytes, done) == 0)
		{
			// make sure str is null terminated
			char* str = (char*) buf;
			str[(bytes > 0) ? (bytes - 1) : 0] = '\0';

			enum XML_Error e = XML_GetErrorCode(self->parser);
			LOGE("XML_ParseBuffer err=%s, bytes=%i, buf=%s",
			     XML_ErrorString(e), bytes, str);
			goto fail_parse;
		}
		else if(self->error)
		{
			goto fail_parse;
		}
	}

	xml_istream_delete(&self);

	// succcess
	return 1;

	// failure
	fail_parse:
		xml_istream_delete(&self);
	return 0;
}

int xml_istream_parseBuffer(void* priv,
                            xml_istream_start_fn start_fn,
                            xml_istream_end_fn   end_fn,
                            const char* buffer,
                            int len)
{
	// priv may be NULL
	assert(start_fn);
	assert(end_fn);
	assert(buffer);

	xml_istream_t* self = xml_istream_new(priv, start_fn, end_fn);
	if(self == NULL)
	{
		return 0;
	}

	// parse buffer
	int done   = 0;
	int offset = 0;
	while(done == 0)
	{
		void* buf = XML_GetBuffer(self->parser, 4096);
		if(buf == NULL)
		{
			LOGE("XML_GetBuffer buf=NULL");
			goto fail_parse;
		}

		int left  = len - offset;
		int bytes = (left > 4096) ? 4096 : left;
		memcpy(buf, &buffer[offset], bytes);

		done = (bytes == 0) ? 1 : 0;
		if(XML_ParseBuffer(self->parser, bytes, done) == 0)
		{
			// make sure str is null terminated
			char* str = (char*) buf;
			str[(bytes > 0) ? (bytes - 1) : 0] = '\0';

			enum XML_Error e = XML_GetErrorCode(self->parser);
			LOGE("XML_ParseBuffer err=%s, bytes=%i, buf=%s",
			     XML_ErrorString(e), bytes, str);
			goto fail_parse;
		}
		else if(self->error)
		{
			goto fail_parse;
		}

		offset += bytes;
	}

	xml_istream_delete(&self);

	// success
	return 1;

	// failure
	fail_parse:
		xml_istream_delete(&self);
	return 0;
}
