/*
 * request_header.c
 * Copyright (C) 2014 c9s <c9s@c9smba.local>
 *
 * Distributed under terms of the MIT license.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include "ldlog.h"
#include "h3.h"

RequestHeader* h3_request_header_new() {
    RequestHeader *h = malloc(sizeof(RequestHeader));
    h->RequestLineStart = NULL;
    h->RequestLineEnd = NULL;
    h->RequestMethod = NULL;
    h->RequestMethodLen = 0;
    h->RequestURI = NULL;
    h->RequestURILen = 0;
    h->HTTPVersion = NULL;;
    h->HTTPVersionLen = 0;
    h->HeaderSize = 0;
    memset( &h->Fields[0], 0, sizeof( h->Fields ) );
    return h;
}

void h3_request_header_free(RequestHeader *header) {
    free(header);
}


/**
 * This function returns a char pointer, which is the end of the request line.
 *
 * Return NULL if parse failed.
 */
const char * h3_request_line_parse(RequestHeader *header, const char *body, int bodyLength) {
    // Parse the request-line
    // http://tools.ietf.org/html/rfc2616#section-5.1
    // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
    const char * p = body;
    header->RequestLineStart = body;

    while (notend(p,body,bodyLength) && ! isspace(*p) ) p++;

    if ( end(p,body,bodyLength) || iscrlf(p) ) {
        // set error
        return NULL;
    }

    header->RequestMethod = body;
    header->RequestMethodLen = p - body;

    log_debug("RequestMethod: %.*s\n",  header->RequestMethodLen, header->RequestMethod);

    // Skip space
    // parse RequestURI
    while (isspace(*p) && notcrlf(p) && notend(p,body,bodyLength)) p++;
    
    header->RequestURI = p; 
    while (!isspace(*p) && notcrlf(p) && notend(p,body,bodyLength)) p++;
    header->RequestURILen = p - header->RequestURI; 

    log_debug("RequestURI: %.*s\n",  header->RequestURILen, header->RequestURI);

    // Skip space and parse HTTP-Version
    if ( iscrlf(p) || end(p,body,bodyLength)) {
        header->HTTPVersion = H3_DEFAULT_HTTP_VERSION;
    } else {
        while (isspace(*p) && notcrlf(p)) p++;

        header->HTTPVersion = p; 
        while (!isspace(*p) && notcrlf(p)) p++;
        header->HTTPVersionLen = p - header->HTTPVersion;
    }
    log_debug("HTTPVersionLen: %.*s\n",  header->HTTPVersionLen, header->HTTPVersion);
    return p;
}



/**
 * Parse header body
 */
int h3_request_header_parse(RequestHeader *header, const char *body, int bodyLength) {
    const char *p = h3_request_line_parse(header, body, bodyLength);

    if (p == NULL) {
        return H3_ERR_REQUEST_LINE_PARSE_FAIL;
    }

    // should be ended with CR-LF
    if (end(p,body,bodyLength)) return -1;

    // skip CR-LF
    if(iscrlf(p)) p+=2;
    if (end(p,body,bodyLength)) return 0;



    header->HeaderSize = 0;

    // Parse Header Fields Here
    do {
        HeaderField *field = & header->Fields[ header->HeaderSize++ ];
        field->FieldName = p; // start of a header field name

        while(notend(p,body,bodyLength) && *p != ':' ) p++;
        field->FieldNameLen = p - field->FieldName;
        p++; // skip ':'

        // CRLF is not allowed here
        if (end(p,body,bodyLength) || iscrlf(p)) return -1;


        while(notend(p,body,bodyLength) && isspace(*p)) p++; // skip space

        // CRLF is not allowed here
        if (end(p,body,bodyLength) || iscrlf(p)) return -1;

        field->Value = p;
        while(notend(p,body,bodyLength) && notcrlf(p) ) p++;
        field->ValueLen = p - field->Value;

        if(iscrlf(p)) p+=2;

        log_debug("==> %.*s ==> %.*s\n", field->FieldNameLen, field->FieldName, field->ValueLen, field->Value );

        // end of header
        if (iscrlf(p)) return 0;
    } while(notend(p,body,bodyLength) && (header->HeaderSize < MAX_HEADER_SIZE));

    return 0;
}
