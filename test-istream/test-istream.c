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

#define LOG_TAG "xml-istream-test"
#include "libcc/cc_log.h"
#include "libxmlstream/xml_istream.h"

/***********************************************************
* private                                                  *
***********************************************************/

static int start_fn(void* priv, int line, float progress,
                    const char* name,
                    const char** atts)
{
	LOGI("line=%i, progress=%f, name=%s",
	     line, progress, name);

	int i = 0;
	while(atts[i] && atts[i + 1])
	{
		LOGI("atts[%i]=%s, atts[%i]=%s",
		     i, atts[i], i + 1, atts[i + 1]);
		i += 2;
	}

	return 1;
}

static int end_fn(void* priv, int line, float progress,
                  const char* name,
                  const char* content)
{
	LOGI("line=%i, progress=%f, name=%s",
	     line, progress, name);

	if(content)
	{
		LOGI("content=%s", content);
	}

	return 1;
}

/***********************************************************
* public                                                   *
***********************************************************/

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		LOGE("usage: %s <test.xml>", argv[0]);
		return EXIT_FAILURE;
	}

	if(xml_istream_parse(NULL, start_fn, end_fn, argv[1]) == 0)
	{
		LOGE("xml_istream_parse failed");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
