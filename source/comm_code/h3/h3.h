/*
 * h3.h
 * Copyright (C) 2014 c9s <c9s@c9smba.local>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef H3_H
#define H3_H

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

// CRLF string for readibility
#define CRLF "\r\n"
#define MAX_HEADER_SIZE 20

enum H3_ERROR { 
    H3_ERR_REQUEST_LINE_PARSE_FAIL = -1,
    H3_ERR_INCOMPLETE_HEADER,
    H3_ERR_UNEXPECTED_CHAR,
};

#define iscrlf(p) ((*p == '\r') && (*(p + 1) == '\n'))
#define notcrlf(p) ((*p != '\r') && (*(p + 1) != '\n'))

#define notend(p,body,len) (p != body + len)
#define end(p,body,len) (p == body + len)


typedef struct _HeaderField HeaderField;

/*
    Host: github.com
    ^     ^
    |     |
    |     Value (ValueLen = 10)
    |
    | FieldName, FieldNameLen = 4
*/
struct _HeaderField {
    const char *FieldName;
    int         FieldNameLen;

    const char *Value;
    int  ValueLen;
};

typedef struct  {
    /**
     * Pointer to start of the request line.
     */
    const char * RequestLineStart;

    /**
     * Pointer to the end of the request line 
     */
    const char * RequestLineEnd;

    /**
     * Pointer to the start of the request method string
     */
    const char * RequestMethod;

    int    RequestMethodLen;

    const char * RequestURI;
    int RequestURILen;

    const char * HTTPVersion;
    int HTTPVersionLen;

    unsigned int HeaderSize;
    HeaderField Fields[MAX_HEADER_SIZE];

} RequestHeader;


const char * h3_request_line_parse(RequestHeader *header, const char *body, int bodyLength);


/**
 * Request Header function prototypes
 */
RequestHeader* h3_request_header_new();

void h3_request_header_free(RequestHeader *header);

int h3_request_header_parse(RequestHeader *header, const char *body, int bodyLength);


/**
 * Predefined string and constants
 */
#define H3_DEFAULT_HTTP_VERSION "HTTP/1.0"


#endif /* !H3_H */
