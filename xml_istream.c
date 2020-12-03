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

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define LOG_TAG "xml"
#include "../libcc/cc_log.h"
#include "../libcc/cc_memory.h"
#include "xml_istream.h"

/***********************************************************
* private                                                  *
***********************************************************/

typedef struct
{
	int error;

	float progress;

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
	ASSERT(_self);
	ASSERT(name);
	ASSERT(atts);

	xml_istream_t* self = (xml_istream_t*) _self;
	xml_istream_start_fn start_fn = self->start_fn;

	int line = XML_GetCurrentLineNumber(self->parser);
	if((*start_fn)(self->priv, line, self->progress,
	               name, atts) == 0)
	{
		self->error = 1;
	}
}

static void xml_istream_end(void* _self,
                            const XML_Char* name)
{
	ASSERT(_self);
	ASSERT(name);

	xml_istream_t* self = (xml_istream_t*) _self;
	xml_istream_end_fn end_fn = self->end_fn;

	int line = XML_GetCurrentLineNumber(self->parser);

	// trim leading whitespace
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

		// pass NULL for empty content
		if(buf[0] == '\0')
		{
			buf = NULL;
		}
	}

	if((*end_fn)(self->priv, line, self->progress,
	             name, buf) == 0)
	{
		self->error = 1;
	}

	FREE(self->content_buf);
	self->content_buf = NULL;
	self->content_len = 0;
}

static void xml_istream_content(void *_self,
                                const char *content,
                                int len)
{
	ASSERT(_self);
	ASSERT(content);

	xml_istream_t* self = (xml_istream_t*) _self;

	int len2  = len + self->content_len;
	int len21 = len2 + 1;
	char* buffer = (char*)
	               REALLOC(self->content_buf,
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
	ASSERT(start_fn);
	ASSERT(end_fn);

	xml_istream_t* self = (xml_istream_t*)
	                      CALLOC(1, sizeof(xml_istream_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
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

	self->priv     = priv;
	self->start_fn = start_fn;
	self->end_fn   = end_fn;
	self->parser   = parser;

	// success
	return self;

	// failure
	fail_parser:
		FREE(self);
	return NULL;
}

static void xml_istream_delete(xml_istream_t** _self)
{
	ASSERT(_self);

	xml_istream_t* self = *_self;
	if(self)
	{
		XML_ParserFree(self->parser);
		FREE(self->content_buf);
		FREE(self);
		*_self = NULL;
	}
}

static int
xml_istream_parseGzFile(void* priv,
                        xml_istream_start_fn start_fn,
                        xml_istream_end_fn   end_fn,
                        gzFile f, size_t len)
{
	// priv may be NULL
	ASSERT(start_fn);
	ASSERT(end_fn);
	ASSERT(f);

	if(len <= 0)
	{
		LOGE("invalid len=%i", (int) len);
		return 0;
	}

	xml_istream_t* self;
	self = xml_istream_new(priv, start_fn, end_fn);
	if(self == NULL)
	{
		return 0;
	}

	// parse file
	int    done  = 0;
	size_t part  = 0;
	size_t total = len;
	while(done == 0)
	{
		void* buf = XML_GetBuffer(self->parser, 4096);
		if(buf == NULL)
		{
			LOGE("XML_GetBuffer buf=NULL");
			goto fail_parse;
		}

		int bytes = gzread(f, buf, 4096);
		if((bytes == 0) && (gzeof(f) == 0))
		{
			LOGE("gzread failed");
			goto fail_parse;
		}

		done  = (bytes == 0) ? 1 : 0;
		part += bytes;
		self->progress = (float) ((double) part / (double) total);
		if(XML_ParseBuffer(self->parser, bytes, done) == 0)
		{
			// make sure str is null terminated
			char* str = (char*) buf;
			str[(bytes > 0) ? (bytes - 1) : 0] = '\0';

			enum XML_Error e = XML_GetErrorCode(self->parser);
			int line = XML_GetCurrentLineNumber(self->parser);
			LOGE("XML_ParseBuffer err=%s, line=%i, bytes=%i, buf=%s",
			     XML_ErrorString(e), line, bytes, str);
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

/***********************************************************
* public                                                   *
***********************************************************/

int xml_istream_parse(void* priv,
                      xml_istream_start_fn start_fn,
                      xml_istream_end_fn   end_fn,
                      const char* fname)
{
	// priv may be NULL
	ASSERT(start_fn);
	ASSERT(end_fn);
	ASSERT(fname);

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
	size_t len = ftell(f);

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

int xml_istream_parseGz(void* priv,
                        xml_istream_start_fn start_fn,
                        xml_istream_end_fn   end_fn,
                        const char* gzname)
{
	// priv may be NULL
	ASSERT(start_fn);
	ASSERT(end_fn);
	ASSERT(gzname);

	// read the uncompressed file size which is stored in the
	// last 4 bytes of the compressed file
	// assumes that the uncompressed file is less than 4GB
	// otherwise the file size may only be determined by
	// decompressing the entire file
	// the len is only used to provide a progress indicator
	// for the istream parser
	FILE* tmp = fopen(gzname, "r");
	if(tmp == NULL)
	{
		LOGE("fopen %s failed", gzname);
		return 0;
	}

	// seek the uncompressed file size
	fseek(tmp, (long) 0, SEEK_END);
	size_t len = ftell(tmp);
	if(len < 4)
	{
		LOGE("invalid len=%i", (int) len);
		fclose(tmp);
		return 0;
	}
	fseek(tmp, len - 4, SEEK_SET);

	// read the uncompressed file size bytes
	unsigned char b1;
	unsigned char b2;
	unsigned char b3;
	unsigned char b4;
	if((fread(&b4, sizeof(char), 1, tmp) != 1) ||
	   (fread(&b3, sizeof(char), 1, tmp) != 1) ||
	   (fread(&b2, sizeof(char), 1, tmp) != 1) ||
	   (fread(&b1, sizeof(char), 1, tmp) != 1))
	{
		LOGE("fread failed");
		fclose(tmp);
		return 0;
	}
	fclose(tmp);

	// decode the uncompressed file size
	unsigned int u1 = b1;
	unsigned int u2 = b2;
	unsigned int u3 = b3;
	unsigned int u4 = b4;
	len = (size_t) ((u1 << 24) | (u2 << 16) | (u3 << 8) | u4);

	gzFile f = gzopen(gzname, "rb");
	if(f == NULL)
	{
		LOGE("gzopen %s failed", gzname);
		return 0;
	}

	if(xml_istream_parseGzFile(priv, start_fn, end_fn, f,
	                           len) == 0)
	{
		goto fail_parse;
	}

	gzclose(f);

	// success
	return 1;

	// failure
	fail_parse:
		gzclose(f);
	return 0;
}

int xml_istream_parseFile(void* priv,
                          xml_istream_start_fn start_fn,
                          xml_istream_end_fn   end_fn,
                          FILE* f, size_t len)
{
	// priv may be NULL
	ASSERT(start_fn);
	ASSERT(end_fn);
	ASSERT(f);

	xml_istream_t* self;
	self = xml_istream_new(priv, start_fn, end_fn);
	if(self == NULL)
	{
		return 0;
	}

	// parse file
	int    done  = 0;
	size_t part  = 0;
	size_t total = len;
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

		len  -= bytes;
		done  = (len == 0) ? 1 : 0;
		part += bytes;
		self->progress = (float) ((double) part / (double) total);
		if(XML_ParseBuffer(self->parser, bytes, done) == 0)
		{
			// make sure str is null terminated
			char* str = (char*) buf;
			str[(bytes > 0) ? (bytes - 1) : 0] = '\0';

			enum XML_Error e = XML_GetErrorCode(self->parser);
			int line = XML_GetCurrentLineNumber(self->parser);
			LOGE("XML_ParseBuffer err=%s, line=%i, bytes=%i, buf=%s",
			     XML_ErrorString(e), line, bytes, str);
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
                            size_t len)
{
	// priv may be NULL
	ASSERT(start_fn);
	ASSERT(end_fn);
	ASSERT(buffer);

	xml_istream_t* self;
	self = xml_istream_new(priv, start_fn, end_fn);
	if(self == NULL)
	{
		return 0;
	}

	// parse buffer
	int    done   = 0;
	int    offset = 0;
	size_t part   = 0;
	size_t total  = len;
	while(done == 0)
	{
		void* buf = XML_GetBuffer(self->parser, 4096);
		if(buf == NULL)
		{
			LOGE("XML_GetBuffer buf=NULL");
			goto fail_parse;
		}

		size_t left  = len - offset;
		int    bytes = (left > 4096) ? 4096 : left;
		memcpy(buf, &buffer[offset], bytes);

		done  = (bytes == 0) ? 1 : 0;
		part += bytes;
		self->progress = (float) ((double) part / (double) total);
		if(XML_ParseBuffer(self->parser, bytes, done) == 0)
		{
			// make sure str is null terminated
			char* str = (char*) buf;
			str[(bytes > 0) ? (bytes - 1) : 0] = '\0';

			enum XML_Error e = XML_GetErrorCode(self->parser);
			int line = XML_GetCurrentLineNumber(self->parser);
			LOGE("XML_ParseBuffer err=%s, line=%i, bytes=%i, buf=%s",
			     XML_ErrorString(e), line, bytes, str);
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
