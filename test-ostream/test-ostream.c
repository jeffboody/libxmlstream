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

#define LOG_TAG "xml-ostream-test"
#include "libcc/cc_log.h"
#include "libxmlstream/xml_ostream.h"

/***********************************************************
* public                                                   *
***********************************************************/

int main(int argc, char** argv)
{
	#ifdef USE_BUFFER
		xml_ostream_t* os = xml_ostream_newBuffer();
	#else
		xml_ostream_t* os = xml_ostream_new("test.xml");
	#endif
	if(os == NULL)
	{
		return EXIT_FAILURE;
	}

	xml_ostream_begin(os, "test");
	{
		xml_ostream_begin(os, "tag");
		xml_ostream_attr(os, "k", "elev");
		xml_ostream_attrf(os, "v", "%f", 5280.0f);
		xml_ostream_end(os);

		xml_ostream_begin(os, "content");
		xml_ostream_attr(os, "test", "content");
		xml_ostream_content(os, "hello");
		xml_ostream_content(os, " ... ");
		xml_ostream_content(os, "goodbye");
		xml_ostream_end(os);

		xml_ostream_begin(os, "a");
		xml_ostream_begin(os, "b");
		xml_ostream_begin(os, "c");
		xml_ostream_attr(os, "test", "recursion");
		xml_ostream_end(os);
		xml_ostream_end(os);
		xml_ostream_end(os);

		xml_ostream_begin(os, "special");
		xml_ostream_attr(os, "lt", "a < b");
		xml_ostream_attr(os, "gt", "a > b");
		xml_ostream_attr(os, "amp", "a & b");
		xml_ostream_attr(os, "quot", "a \" b");
		xml_ostream_attr(os, "apos", "a ' b");
		xml_ostream_attr(os, "inval", "a \n\r\t b");
		xml_ostream_end(os);
	}
	xml_ostream_end(os);

	// check for errors
	#ifdef USE_BUFFER
		int len = 0;
		const char* buf = xml_ostream_buffer(os, 1, &len);
		if(buf)
		{
			LOGI("len=%i, strlen=%i", len, (int) strlen(buf));
			printf("%s\n", buf);
			LOGI("COMPLETE");

			// free buffer when acquired
			free((void*) buf);
		}
	#else
		if(xml_ostream_complete(os))
		{
			LOGI("COMPLETE");
		}
	#endif
	else
	{
		LOGE("INCOMPLETE");
	}

	xml_ostream_delete(&os);

	return EXIT_SUCCESS;
}
