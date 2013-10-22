/*
 * SpiceStream - simple, incremental reader for analog data files,
 * such as those produced by spice-type simulators.
 *
 * Copyright (C) 1998,1999  Stephen G. Tell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "ssintern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <stdarg.h>
#include <errno.h>
// #include <config.h>
#include "glib.h"

#include "spicestream.h"

extern SpiceStream *sf_rdhdr_hspice(char *name, FILE *fp);
extern SpiceStream *sf_rdhdr_hsascii(char *name, FILE *fp);
extern SpiceStream *sf_rdhdr_hsbin(char *name, FILE *fp);
extern SpiceStream *sf_rdhdr_cazm(char *name, FILE *fp);
extern SpiceStream *sf_rdhdr_s3raw(char *name, FILE *fp);
extern SpiceStream *sf_rdhdr_s2raw(char *name, FILE *fp);
extern SpiceStream *sf_rdhdr_ascii(char *name, FILE *fp);
// extern SpiceStream *sf_rdhdr_nsout(char *name, FILE *fp);
static int ss_readrow_none(SpiceStream *, double *ivar, double *dvars);

SSMsgLevel spicestream_msg_level = WARN;

typedef SpiceStream* (*PFD)(char *name, FILE *fp);

typedef struct {
	char *name;
	PFD rdfunc;
} DFormat;

static DFormat format_tab[] = {
	{"hspice", sf_rdhdr_hspice },
	{"hsascii", sf_rdhdr_hsascii },
	{"hsbinary", sf_rdhdr_hsbin },
	{"cazm", sf_rdhdr_cazm },
	{"spice3raw", sf_rdhdr_s3raw },
	{"spice2raw", sf_rdhdr_s2raw },
	{"ascii", sf_rdhdr_ascii },
	// {"nsout", sf_rdhdr_nsout },
};
static const int NFormats = sizeof(format_tab)/sizeof(DFormat);

/*
 * Open spice waveform file for reading.  
 * Reads in header with signal names (and sometimes signal types).
 * TODO: simple strategies for trying to deduce file type from
 * name or contents.
 */

SpiceStream *
ss_open_internal(FILE *fp, char *filename, char *format)
{
	SpiceStream *ss;
	int i;

	for(i = 0; i < NFormats; i++) {
		if(0==strcmp(format, format_tab[i].name)) {
			ss = (format_tab[i].rdfunc)(filename, fp);
			if(ss) {
				ss->filetype = i;
				return ss;
			} else {
				ss_msg(DBG, "ss_open", "failed to open %s using format %s", filename, format_tab[i].name);
				return NULL;
			}
		}
	}
	ss_msg(ERR, "ss_open", "Format \"%s\" unknown", format);
	return NULL;
}

SpiceStream *
ss_open(char *filename, char *format)
{
	FILE *fp;

	fp = fopen64(filename, "r");
	if(fp == NULL) {
		fprintf(stderr, "fopen(\"%s\"): %s\n", filename, strerror(errno));
		return NULL;
	}

	return ss_open_internal(fp, filename, format);
}

SpiceStream *
ss_open_fp(FILE *fp, char *format)
{
	return ss_open_internal(fp, "<spicestream>", format);
}

/*
 * Allocate SpiceStream structure and fill in some portions.
 * To be called only from format-specific header-reading functions,
 * usually after they read and verify the header.
 * Caller must still set types and names of ivar and dvars,
 * and must set readrow and linebuf items.
 */
SpiceStream *
ss_new(FILE *fp, char *filename, int ndv, int nspar)
{
	SpiceStream *ss;

	ss = g_new0(SpiceStream, 1);
	ss->filename = g_strdup(filename);
	ss->fp = fp;
	ss->ivar = g_new0(SpiceVar, 1);
	ss->ndv = ndv;
	if(ndv)
		ss->dvar = g_new0(SpiceVar, ndv);
	ss->nsweepparam = nspar;
	if(nspar)
		ss->spar = g_new0(SpiceVar, nspar);

	return ss;
}

/*
 * Close the file assocated with a SpiceStream.
 * No more data can be read, but the header information can still
 * be accessed.
 */
void ss_close(SpiceStream *ss)
{
	fclose(ss->fp);
	ss->fp = NULL;
	ss->readrow = ss_readrow_none;
}

/*
 * Free all resources associated with a SpiceStream.
 */
void ss_delete(SpiceStream *ss)
{
	if(ss->fp)
		fclose(ss->fp);
	if(ss->filename)
		g_free(ss->filename);
	if(ss->ivar)
		g_free(ss->ivar);
	if(ss->dvar)
		g_free(ss->dvar);
	if(ss->linebuf)
		g_free(ss->linebuf);
	g_free(ss);
}

/*
 * row-reading function that always returns EOF.
 */
static int 
ss_readrow_none(SpiceStream *ss, double *ivar, double *dvars)
{
	return 0;
}


static char *vartype_names[] = {
	"Unknown", "Time", "Voltage", "Current", "Frequency"
};
const int nvartype_names = sizeof(vartype_names)/sizeof(char *);

/*
 * return a string corresponding to a SpiceStream VarType.
 * the pointer returned is in static or readonly storage,
 * and is overwritten with each call.
 */
char *vartype_name_str(VarType type)
{
	static char buf[32];
	if(type < nvartype_names)
		return vartype_names[type];
	else {
		sprintf(buf, "type-%d", type);
		return buf;
	}
}

/*
 * return pointer to string with printable name for a variable
 * or one of the columns of a variable.
 * buf is a pointer to a buffer to use.  If NULL, one will be allocated.
 * n is the maximum number of characters to put in the buffer.
 */
char *ss_var_name(SpiceVar *sv, int col, char *buf, int n)
{
	int idx;

	if(buf == NULL) {
		int l;
		l = strlen(sv->name + 3);
		buf = g_new(char, l);
		n = l;
	}
	strncpy(buf, sv->name, n-1);
	n -= strlen(buf)+1;
	if(sv->ncols == 1 || col < 0)
		return buf;
	if(n>1) {
		idx = strlen(buf);
		buf[idx++] = '.';
		buf[idx++] = '0'+col;
		buf[idx] = 0;
	}
	
	return(buf);
}

/*
 * given a filetype number, return a pointer to a string containing the 
 * name of the Spicestream file format.
 * Valid file type numbers start at 0. 
 */
char *ss_filetype_name(int n)
{
	if(n >= 0 && n < NFormats)
		return format_tab[n].name;
	else
		return NULL;
}

/*
 * utility function to read whole line into buffer, expanding buffer if needed.
 * line buffer must be allocated with g_malloc/g_new, or NULL in which case
 * we allocate an initial, buffer.
 * returns 0 or EOF.
 */
int
fread_line(FILE *fp, char **bufp, int *bufsize)
{
	int c;
	int n = 0;
	if(*bufp == NULL) {
		if(*bufsize == 0)
			*bufsize = 1024;
		*bufp = g_new(char, *bufsize);
	}
	while(((c = getc(fp)) != EOF) && c != '\n') {
		(*bufp)[n++] = c;
		if(n >= *bufsize) {
			*bufsize *= 2;
			*bufp = g_realloc(*bufp, *bufsize);
		}
	}
	(*bufp)[n] = 0;
	if(c == EOF)
		return EOF;
	else
		return 0;
}

FILE *ss_error_file;
SSMsgHook ss_error_hook;

/* 
 * ss_msg: emit an error message from anything in the spicestream subsystem,
 * or anything else that wants to use our routines.
 *
 * If ss_error_hook is non-NULL, it is a pointer to a function that
 * will be called with the error string.
 * if ss_error_file is non-NULL, it is a FILE* to write the message to.
 * If neither of these are non-null, the message is written to stderr.
 *
 * args: 
 *   type is one of:
 *	 DBG - Debug, ERR - ERROR, INFO - infomration, WARN - warning
 *       id is the name of the function, or other identifier
 * 	 remaining arguments are printf-like.
 */
void
ss_msg(SSMsgLevel type, const char *id, const char *msg, ...)
{
	char *typestr;
	va_list args;
	int blen = 1024;
	char buf[1024];

	if(type < spicestream_msg_level)
		return;

	switch (type) {
	case DBG:
		typestr = "<<DEBUG>>";
		break;
	case ERR:
		typestr = "<<ERROR>>";
		break;
	case WARN:
		typestr = "<<WARNING>>";
		break;
	case INFO:
	default:
		typestr = "";
		break;
	}

	va_start(args, msg);

#ifdef HAVE_SNPRINTF
	blen = snprintf(buf, 1024, "[%s]: %s ", id, typestr);
	if(blen>0)
		blen += vsnprintf(&buf[blen-1], 1024-blen, msg, args);
	if(blen>0)
		blen += snprintf(&buf[blen-1], 1024-blen, "\n");
#else
	sprintf(buf, "[%s]: %s ", id, typestr);
	blen = strlen(buf);
	vsprintf(&buf[blen], msg, args);
	strcat(buf, "\n");
#endif

	if(ss_error_hook)
		(ss_error_hook)(buf);
	if(ss_error_file)
		fputs(buf, ss_error_file);
	if(ss_error_hook == NULL && ss_error_file == NULL)
		fputs(buf, stderr);
	
	va_end(args);
}
