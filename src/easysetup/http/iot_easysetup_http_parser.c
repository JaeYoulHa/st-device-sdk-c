/******************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2020 Samsung Electronics
 * Copyright (c) 2019 Aleksey Kurepin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * http message parser has come from Pico HTTP Server (https://github.com/foxweb/pico)
 *
 ******************************************************************/

#include <string.h>
#include <ctype.h>
#include "easysetup_http.h"
#include "iot_debug.h"
#include "iot_easysetup.h"

typedef struct { char *name, *value; } header_t;

#define MAX_HEADER_SUPPORT (16)
static header_t reqhdr[MAX_HEADER_SUPPORT + 1] = {{"\0", "\0"}};

static bool is_header_content_length(char *header_key)
{
	int i;
	size_t len;

	const char content_length_lower[] = "content-length";
	len = strlen(content_length_lower);

	if (!header_key) {
		return false;
	}

	if (strlen(header_key) != len) {
		return false;
	}

	for (i = 0; i < len; i++) {
		if (content_length_lower[i] != (char)tolower((int)header_key[i])) {
			return false;
		}
	}

	return true;
}

iot_error_t es_msg_parser(char *rx_buffer, size_t rx_buffer_len, char **payload, int *cmd, int *type, size_t *content_len)
{
	// Client request
	char *method = NULL; // "GET" or "POST"
	char *uri = NULL; // "/index.html" things before '?'
	char *prot = NULL; // "HTTP/1.1"

	if ((rx_buffer == NULL) || (cmd == NULL) || (type == NULL))
	{
		IOT_ERROR("invalid data format!!");
		return IOT_ERROR_INVALID_ARGS;
	}

	method = strtok(rx_buffer, " \t\r\n");
	uri = strtok(NULL, " \t");
	prot = strtok(NULL, " \t\r\n");

	if ((method == NULL) || (uri == NULL) || (prot == NULL))
	{
		IOT_ERROR("invalid data reported");
		return IOT_ERROR_INVALID_ARGS;
	}

	if (!strcmp(method,  "GET")) {
		*type = D2D_GET;
		if (!strcmp(uri, IOT_ES_URI_GET_DEVICEINFO)) {
			*cmd = IOT_EASYSETUP_STEP_DEVICEINFO;
		} else if (!strcmp(uri, IOT_ES_URI_GET_WIFISCANINFO)) {
			*cmd = IOT_EASYSETUP_STEP_WIFISCANINFO;
		} else if (!strcmp(uri, IOT_ES_URI_GET_LOGS_SYSTEMINFO)) {
			*cmd = IOT_EASYSETUP_STEP_LOG_SYSTEMINFO;
		} else if (!strcmp(uri, IOT_ES_URI_GET_LOGS_DUMP)) {
			*cmd = IOT_EASYSETUP_STEP_LOG_GET_DUMP;
		} else {
			IOT_ERROR("[GET] invalid step : %s", uri);
			*cmd = IOT_EASYSETUP_INVALID_STEP;
		}
	} else if (!strcmp(method,  "POST")) {
        size_t post_content_len = 0;
        header_t *header = reqhdr;
        char *body = NULL;

        while (header < reqhdr + MAX_HEADER_SUPPORT) {
            char *key, *value;

            key = strtok(NULL, "\r\n: \t");
            if (!key)
                break;

            value = strtok(NULL, "\r\n");
            if (!value)
                break;

            while (*value && *value == ' ')
                value++;

            header->name = key;
            header->value = value;

            if (is_header_content_length(header->name)) {
                post_content_len = atoi(header->value);
            }

            header++;

            body = value + strlen(value);

            if (body[1] == '\n' && body[2] == '\r' && body[3] == '\n') {
                // end of header
                body += 3;
                break;
            }
        }

        if (body == NULL) {
            body = prot + strlen(prot);
            while (*body && *body == ' ')
                body++;
            body += strlen("\r\n\r\n") - 1;
        }

        body++;

		if ((body) < (rx_buffer + rx_buffer_len)) {
			*payload = body;
		} else {
		    IOT_ERROR("[POST] out-of-range");
            return IOT_ERROR_INVALID_ARGS;
		}

        if (post_content_len > 0) {
            *content_len = post_content_len;
        } else {
            IOT_ERROR("[POST] missing content length");
        }
		*type = D2D_POST;
		if (!strcmp(uri, IOT_ES_URI_POST_KEYINFO)) {
			*cmd = IOT_EASYSETUP_STEP_KEYINFO;
		} else if (!strcmp(uri, IOT_ES_URI_POST_CONFIRMINFO)) {
			*cmd = IOT_EASYSETUP_STEP_CONFIRMINFO;
		} else if (!strcmp(uri, IOT_ES_URI_POST_CONFIRM)) {
			*cmd = IOT_EASYSETUP_STEP_CONFIRM;
		} else if (!strcmp(uri, IOT_ES_URI_POST_WIFIPROVISIONINGINFO)) {
			*cmd = IOT_EASYSETUP_STEP_WIFIPROVIONINGINFO;
		} else if (!strcmp(uri, IOT_ES_URI_POST_SETUPCOMPLETE)) {
			*cmd = IOT_EASYSETUP_STEP_SETUPCOMPLETE;
		} else if (!strcmp(uri, IOT_ES_URI_POST_LOGS)) {
			*cmd = IOT_EASYSETUP_STEP_LOG_CREATE_DUMP;
		} else {
			IOT_ERROR("[POST] invalid step : %s", uri);
			*cmd = IOT_EASYSETUP_INVALID_STEP;
		}
		IOT_DEBUG("payload : %s", payload);
	} else {
		IOT_ERROR("[%s] not support type : %s", prot, method);
		*type = D2D_ERROR;
	}
	return IOT_ERROR_NONE;
}

