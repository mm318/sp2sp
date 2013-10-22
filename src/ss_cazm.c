/*
 * ss_cazm.c: CAZM- and ASCII- format routines for SpiceStream
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
 * You should have received a copy of the GNU General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * CAzM and "ascii" format are closely related, so they are both handled
 * in this file.
 *
 * CAzM format is intended to handles files written by MCNC's CAzM simulator,
 * used by a number of universities, and its commercial decendant, 
 * the TSPICE product from Tanner Research.
 *
 * CAzM-format files contain a multiline header.  The second to last line
 * of the header identifies the analysis type, for example TRANSIENT or AC.
 * The last line of the header contains the names of the variables, seperated
 * by whitespace.
 *
 * Ascii-format files have a one-line header, containing a space- or
 * tab-speperated list of the variable names.  To avoid treating a file
 * containing random binary garbage as an ascii-format file, we require the
 * header line to contain space, tab, and USASCII printable characters only.
 * 
 */
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

// #include <config.h>
#include "glib.h"
#include "spicestream.h"

static int sf_readrow_ascii(SpiceStream *sf, double *ivar, double *dvars);
static SpiceStream *ascii_process_header(char *line, VarType ivtype,
				  char *fname, int lineno);

/* Read spice-type file header - cazm format */
SpiceStream *
sf_rdhdr_cazm(char *name, FILE *fp)
{
	SpiceStream *sf;
	char *line = NULL;
	int lineno = 0;
	int linesize = 1024;
	int done = 0;
	VarType ivtype;
	
	while(!done) {
		if((fread_line(fp, &line, &linesize) == EOF) || lineno > 30) {
			g_free(line);
			return NULL;
		}
		lineno++;
		/* "section header" line */
		if(strncmp(line, "TRANSIENT", 9) == 0) {
			ivtype = TIME;
			done = 1;
		} else if(strncmp(line, "AC ANALYSIS", 11) == 0) {
			ivtype = FREQUENCY;
			done = 1;
		} else if(strncmp(line, "TRANSFER", 8) == 0) {
			/* DC transfer function - ivar might also be current,
			 * but we can't tell */
			ivtype = VOLTAGE;
			done = 1;
		}
	}

	/* line after header contains signal names
	 * first one is assumed to be the independent variable.
	 */
	if(fread_line(fp, &line, &linesize) == EOF) {
		g_free(line);
		return NULL;
	}
	lineno++;

	sf = ascii_process_header(line, ivtype, name, lineno);
	if(!sf)
		goto fail;

	sf->fp = fp;
	sf->lineno = lineno;
	sf->linebuf = line;
	sf->lbufsize = linesize;
	return sf;

 fail:
	if(line)
		g_free(line);
	return NULL;
}


/* Read spice-type file header - ascii format */
SpiceStream *
sf_rdhdr_ascii(char *name, FILE *fp)
{
	SpiceStream *sf;
	char *line = NULL;
	int lineno = 0;
	int linesize = 1024;
	char *cp;
	
	/* 
	 * first line is expected to contain space-seperated
	 * variable names.
	 * first one is assumed to be the independent variable.
	 */
	if(fread_line(fp, &line, &linesize) == EOF) {
		goto fail;
	}
	lineno++;
	
	/* Check for non-ascii characters in header, to reject 
	 * binary files.
	 */
	for(cp = line; *cp; cp++) {
		if(!isgraph(*cp) && *cp != ' ' && *cp != '\t') {
			goto fail;
		}
	}

	sf = ascii_process_header(line, UNKNOWN, name, lineno);
	if(!sf)
		goto fail;

	sf->fp = fp;
	sf->lineno = lineno;
	sf->linebuf = line;
	sf->lbufsize = linesize;
	return sf;

 fail:
	if(line)
		g_free(line);
	return NULL;
}


/*
 * Process a header line from an ascii or cazm format file.
 * Returns a filled-in SpiceStream* with variable information.
 */
static
SpiceStream *ascii_process_header(char *line, VarType ivtype,
				  char *fname, int lineno)
{
	SpiceStream *sf;
	char *signam;
	int dvsize = 64;

	signam = strtok(line, " \t\n");
	if(!signam) {
		ss_msg(ERR, "ascii_process_header", "%s:%d: syntax error in header", fname, lineno);
		return NULL;
	}

	/* a bit of a hack: get ss_new to allocate additional
	 * dvars, then only use the entries we need or allocate more
	 */
	sf = ss_new(NULL, fname, dvsize, 0);

	if(ivtype == UNKNOWN) {
		if(strcasecmp(signam, "time") == 0)
                       sf->ivar->type = TIME;
	} else {
               sf->ivar->type = ivtype;
	}
	sf->ivar->name = g_strdup(signam);
	sf->ivar->col = 0;
	sf->ivar->ncols = 1;

	sf->ndv = 0;
	sf->ncols = 1;
	sf->ntables = 1;
	while((signam = strtok(NULL, " \t\n")) != NULL) {
		if(sf->ndv >= dvsize) {
			dvsize *= 2;
			sf->dvar = g_realloc(sf->dvar, dvsize * sizeof(SpiceVar));
		}
		sf->dvar[sf->ndv].name = g_strdup(signam);
		sf->dvar[sf->ndv].type = UNKNOWN;
		sf->dvar[sf->ndv].col = sf->ncols;
		sf->dvar[sf->ndv].ncols = 1;
		sf->ndv++;
		sf->ncols++;
	}
	sf->readrow = sf_readrow_ascii;
	
	return sf;
}



/* Read row of values from ascii- or cazm- format file.  
 * Possibly reusable for other future formats with lines of
 * whitespace-seperated values.
 * Returns:
 *	1 on success.  also fills in *ivar scalar and *dvars vector
 *	0 on EOF
 *	-1 on error  (may change some ivar/dvar values)
 */
static int 
sf_readrow_ascii(SpiceStream *sf, double *ivar, double *dvars)
{
	int i;
	char *tok;

	if(fread_line(sf->fp, &sf->linebuf, &sf->lbufsize) == EOF) {
		return 0;
	}
	sf->lineno++;

	tok = strtok(sf->linebuf, " \t\n");
	if(!tok) {
		return 0;  /* blank line can indicate end of data */
	}

	/* check to see if it is numeric: ascii format is so loosly defined
	 * that we might read a load of garbage otherwise. */
	    
	if(strspn(tok, "0123456789eE+-.") != strlen(tok)) {
		ss_msg(ERR, "sf_readrow_ascii", "%s:%d: expected number; maybe this isn't an ascii data file at all?", sf->filename, sf->lineno, i);
		return -1;
	}

	*ivar = atof(tok);
	
	for(i = 0; i < sf->ncols-1; i++) {
		tok = strtok(NULL, " \t\n");
		if(!tok) {
			ss_msg(ERR, "sf_readrow_ascii", "%s:%d: data field %d missing", sf->filename, sf->lineno, i);
			return -1;
		}
		dvars[i] = atof(tok);
	}
	return 1;
}
