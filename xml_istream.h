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

#ifndef xml_istream_H
#define xml_istream_H

#include <stdio.h>
#include "../libexpat/expat/lib/expat.h"

typedef int (*xml_istream_start_fn)(void* priv,
                                    int line,
                                    float progress,
                                    const char* name,
                                    const char** atts);
typedef int (*xml_istream_end_fn)(void* priv,
                                  int line,
                                  float progress,
                                  const char* name,
                                  const char* content);

int xml_istream_parse(void* priv,
                      xml_istream_start_fn start_fn,
                      xml_istream_end_fn   end_fn,
                      const char* fname);
int xml_istream_parseGz(void* priv,
                        xml_istream_start_fn start_fn,
                        xml_istream_end_fn   end_fn,
                        const char* gzname);
int xml_istream_parseFile(void* priv,
                          xml_istream_start_fn start_fn,
                          xml_istream_end_fn   end_fn,
                          FILE* f, size_t len);
int xml_istream_parseBuffer(void* priv,
                            xml_istream_start_fn start_fn,
                            xml_istream_end_fn   end_fn,
                            const char* buffer,
                            size_t len);

#endif
