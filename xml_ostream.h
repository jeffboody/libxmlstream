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

#ifndef xml_ostream_H
#define xml_ostream_H

#include <stdio.h>
#include <zlib.h>

typedef struct
{
	FILE* f;
	int   close;
	char  fname[256];
} xml_ostreamFile_t;

typedef struct
{
	gzFile f;
	int    close;
	char   gzname[256];
} xml_ostreamGzFile_t;

typedef struct
{
	char* buffer;
	int   len;
} xml_ostreamBuffer_t;

typedef struct xml_ostreamElem_s
{
	char name[256];
	struct xml_ostreamElem_s* next;
} xml_ostreamElem_t;

typedef struct
{
	int mode;
	int state;
	int error;
	int depth;
	xml_ostreamElem_t* elem;
	union
	{
		xml_ostreamFile_t   of;
		xml_ostreamGzFile_t oz;
		xml_ostreamBuffer_t ob;
	};
} xml_ostream_t;

xml_ostream_t* xml_ostream_new(const char* fname);
xml_ostream_t* xml_ostream_newGz(const char* gzname);
xml_ostream_t* xml_ostream_newFile(FILE* f);
xml_ostream_t* xml_ostream_newBuffer(void);
void           xml_ostream_delete(xml_ostream_t** _self);
int            xml_ostream_begin(xml_ostream_t* self,
                                 const char* name);
int            xml_ostream_end(xml_ostream_t* self);
int            xml_ostream_attr(xml_ostream_t* self,
                                const char* name,
                                const char* val);
int            xml_ostream_attrf(xml_ostream_t* self,
                                 const char* name,
                                 const char* fmt, ...);
int            xml_ostream_content(xml_ostream_t* self,
                                   const char* content);
int            xml_ostream_contentf(xml_ostream_t* self,
                                    const char* fmt, ...);
const char*    xml_ostream_buffer(xml_ostream_t* self,
                                  int acquire,
                                  int* len);
int            xml_ostream_complete(xml_ostream_t* self);

#endif
