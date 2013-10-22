/*
 * ss_hspice.c: HSPICE routines for SpiceStream
 *
 * Copyright (C) 1998-2002  Stephen G. Tell
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "ssintern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <sys/types.h>
#include <sys/stat.h>

// #include <config.h>
#include "glib.h"
#include "spicestream.h"

SpiceStream *sf_rdhdr_hspice(char *name, FILE *fp);
SpiceStream *sf_rdhdr_hsascii(char *name, FILE *fp);
SpiceStream *sf_rdhdr_hsbin(char *name, FILE *fp);

static int sf_readrow_hsascii(SpiceStream *sf, double *ivar, double *dvars);
static int sf_readrow_hsbin(SpiceStream *sf, double *ivar, double *dvars);
static SpiceStream *hs_process_header(int nauto, int nprobe, 
				      int nsweepparam, char *line, char *name);
static int sf_readsweep_hsascii(SpiceStream *sf, double *svar);
static int sf_readsweep_hsbin(SpiceStream *sf, double *svar);
static int sf_readblock_hsbin(FILE *fp, char **bufp, int *bufsize, int offset);

struct hsblock_header {  /* structure of binary tr0 block headers */
	gint32 h1;
	gint32 h2;
	gint32 h3;
	gint32 block_nbytes;
};

union gint32bytes {
	gint32 i;
	gchar b[4];
};

static void swap_gint32(gint32 *pi, size_t n);

/* Read spice-type file header - autosense hspice binary or ascii */
SpiceStream *
sf_rdhdr_hspice(char *name, FILE *fp)
{
	int c;
	if((c = getc(fp)) == EOF)
		return NULL;
	ungetc(c, fp);

	if((c & 0xff) < ' ')
		return sf_rdhdr_hsbin(name, fp);
	else
		return sf_rdhdr_hsascii(name, fp);
      
	return NULL;
}

/* Read spice-type file header - hspice ascii */
SpiceStream *
sf_rdhdr_hsascii(char *name, FILE *fp)
{
	SpiceStream *sf = NULL;
	char *line = NULL;
	int nauto, nprobe, nsweepparam, ntables;
	int lineno = 0;
	int linesize = 1024;
	int lineused;
	char lbuf[256];
	char nbuf[16];
	char *cp;
	int maxlines;

	if(fgets(lbuf, sizeof(lbuf), fp) == NULL)
		return NULL;
	lineno++;
	
	/* version of post format */
	if(strncmp(&lbuf[16], "9007", 4) != 0 
	   && strncmp(&lbuf[16], "9601", 4) != 0)
		return NULL;
	strncpy(nbuf, &lbuf[0], 4);
	nbuf[4] = 0;
	nauto = atoi(nbuf);

	strncpy(nbuf, &lbuf[4], 4);
	nbuf[4] = 0;
	nprobe = atoi(nbuf);
	
	strncpy(nbuf, &lbuf[8], 4);
	nbuf[4] = 0;
	nsweepparam = atoi(nbuf);
	
	if(fgets(lbuf, sizeof(lbuf), fp) == NULL) /* date, time etc. */
		return NULL;
	lineno++;
	/* number of sweeps, possibly with cruft at the start of the line */
	if(fgets(lbuf, sizeof(lbuf), fp) == NULL)
		return NULL;
	cp = strchr(lbuf, ' ');
	if(!cp)
		cp = lbuf;
	ntables = atoi(cp);
	if(ntables == 0)
		ntables = 1;
	lineno++;

	maxlines = nauto + nprobe + nsweepparam + 100;
	/* lines making up a fixed-field structure with variable-types and
	 * variable names.
	 * variable names can get split across lines! so we remove newlines,
	 * paste all of the lines together, and then deal with the
	 * whole header at once.
	 * A variable name of "$&%#" indicates the end!
	 */
	line = g_new0(char, linesize);
	lineused = 0;
	do {
		int len;
		if(fgets(lbuf, sizeof(lbuf), fp) == NULL)
			return NULL;
		lineno++;
		if((cp = strchr(lbuf, '\n')) != NULL)
			*cp = 0;
		len = strlen(lbuf);
		if(lineused + len + 1 > linesize) {
			linesize *= 2;
			if(linesize > 1050000) {
				ss_msg(ERR, "rdhdr_ascii", "internal error - failed to find end of header\n; linesize=%d line=\n%.200s\n", linesize, line);
				exit(4);
			}
				
			line = g_realloc(line, linesize);
		}
		strcat(line, lbuf);
		lineused += len;

	} while(!strstr(line, "$&%#") && lineno < maxlines);
	if(lineno == maxlines) {
		ss_msg(DBG, "rdhdr_hsascii", "%s:%d: end of hspice header not found", name,lineno);
		goto fail;
	}

	sf = hs_process_header(nauto, nprobe, nsweepparam, line, name);
	if(!sf)
		goto fail;
	sf->fp = fp;
	sf->readrow = sf_readrow_hsascii;
	sf->linebuf = line;
	sf->linep = NULL;
	sf->lbufsize = linesize;
	sf->ntables = ntables;
	sf->read_tables = 0;
	sf->read_rows = 0;
	sf->read_sweepparam = 0;
	sf->readsweep = sf_readsweep_hsascii;
	sf->lineno = lineno;

	ss_msg(DBG, "rdhdr_hsascii", "ntables=%d; expect %d columns", 
	       sf->ntables, sf->ncols);

	return sf;

 fail:
	if(line)
		g_free(line);
	return NULL;

}

/* Read spice-type file header - hspice binary */
SpiceStream *
sf_rdhdr_hsbin(char *name, FILE *fp)
{

	SpiceStream *sf = NULL;
	char *ahdr = NULL;
	int ahdrsize = 0;
	int ahdrend = 0;
	int n;
	int datasize;
	int nauto, nprobe, nsweepparam, ntables;
	char nbuf[16];
	struct hsblock_header hh;
	
	do {
		n = sf_readblock_hsbin(fp, &ahdr, &ahdrsize, ahdrend);
		if(n <= 0)
			goto fail;
		ahdrend += n;
		ahdr[ahdrend] = '\0';

	} while(!strstr(ahdr, "$&%#"));

	/* ahdr is an ascii header that describes the variables in
	 * much the same way that the first lines of the ascii format do,
	 * except that there are no newlines
	 */

	if(strncmp(&ahdr[16], "9007", 4) != 0 	/* version of post format */
	   && strncmp(&ahdr[16], "9601", 4) != 0)
		goto fail;
	strncpy(nbuf, &ahdr[0], 4);
	nbuf[4] = 0;
	nauto = atoi(nbuf);	/* number of automaticly-included variables,
				   first one is independent variable */
	strncpy(nbuf, &ahdr[4], 4);
	nbuf[4] = 0;
	nprobe = atoi(nbuf);	/* number of user-requested columns */

	strncpy(nbuf, &ahdr[8], 4);
	nbuf[4] = 0;
	nsweepparam = atoi(nbuf);	/* number of sweep parameters */

	ntables = atoi(&ahdr[176]);
	if(ntables == 0)
		ntables = 1;

	sf = hs_process_header(nauto, nprobe, nsweepparam, &ahdr[256], name);
	if(!sf)
		goto fail;
	
	if(fread(&hh, sizeof(hh), 1, fp) != 1) {
		ss_msg(DBG, "sf_rdhdr_hsbin", "EOF reading block header");
		goto fail;
	}
	if(hh.h1 == 0x04000000 && hh.h3 == 0x04000000) {
		/* detected endian swap */
		sf->flags |= SSF_ESWAP;
		swap_gint32((gint32*)&hh, sizeof(hh)/sizeof(gint32));
	}
	if(hh.h1 != 4 || hh.h3 != 4) {
		ss_msg(DBG, "sf_rdhdr_hsbin", "unexepected values in data block header");
		goto fail;
	}

	datasize = hh.block_nbytes;
	sf->expected_vals = datasize / sizeof(float);
	sf->read_vals = 0;
	
	ss_msg(DBG, "sf_rdhdr_hsbin", "datasize=%d expect %d columns, %d values;\n  reading first data block at 0x%lx", datasize, sf->ncols, sf->expected_vals, (long)ftello64(fp));


	sf->fp = fp;
	sf->readrow = sf_readrow_hsbin;
	sf->readsweep = sf_readsweep_hsbin;

	sf->ntables = ntables;
	sf->read_tables = 0;
	sf->read_rows = 0;
	sf->read_sweepparam = 0;

	return sf;
 fail:
	if(ahdr)
		g_free(ahdr);
	if(sf) {
		if(sf->dvar)
			g_free(sf->dvar);
		g_free(sf);
	}

	return NULL;
}

/* common code for reading ascii or binary hspice headers.
 * Given a string of ascii header information, set up the
 * SpiceStream structure appropriately.
 * Returns NULL on failure.
 */
static SpiceStream *
hs_process_header(int nauto, int nprobe, int nsweepparam, char *line, char *name)
{
	char *cp;
	char *signam;
	SpiceStream *sf;
	int i;
	int hstype;

/* type of independent variable */
	cp = strtok(line, " \t\n");
	if(!cp) {
		ss_msg(DBG, "hs_process_header", "%s: initial vartype not found on header line.", name);
		return NULL;
	}
	sf = ss_new(NULL, name, nauto-1 + nprobe, nsweepparam);
	hstype = atoi(cp);
	switch(hstype) {
	case 1:
		sf->ivar->type = TIME;
		break;
	case 2:
		sf->ivar->type = FREQUENCY;
		break;
	case 3:
		sf->ivar->type = VOLTAGE;
		break;
	default:
		sf->ivar->type = UNKNOWN;
		break;
	}
	sf->ivar->col = 0;
	sf->ivar->ncols = 1;
	sf->ncols = 1;

/* dependent variable types */
	for(i = 0; i < sf->ndv; i++) {
		cp = strtok(NULL, " \t\n");
		if(!cp) {
			ss_msg(DBG, "hs_process_header", "%s: not enough vartypes on header line", name);
			return NULL;
		}
		if(!isdigit(cp[0])) {
			ss_msg(DBG, "hs_process_header", "%s: bad vartype %d [%s] on header line", name, i, cp);
			return NULL;
		}
		hstype = atoi(cp);
		switch(hstype) {
		case 1:
		case 2:
			sf->dvar[i].type = VOLTAGE;
			break;
		case 8:
		case 15:
		case 22:
			sf->dvar[i].type = CURRENT;
			break;
		default:
			sf->dvar[i].type = UNKNOWN;
			break;
		}

		/* how many columns comprise this variable? */
		sf->dvar[i].col = sf->ncols;
		if(i < nauto-1 && sf->ivar->type == FREQUENCY) {
			sf->dvar[i].ncols = 2;
		} else {
			sf->dvar[i].ncols = 1;
		}
		sf->ncols += sf->dvar[i].ncols;
	}

/* independent variable name */
	signam = strtok(NULL, " \t\n"); 
	if(!signam) {
		ss_msg(DBG, "hs_process_header", "%s: no IV name found on header line", name);
		goto fail;
	}
	sf->ivar->name = g_strdup(signam);
	
 /* dependent variable names */
	for(i = 0; i < sf->ndv; i++) {
		if((signam = strtok(NULL, " \t\n")) == NULL) {
			ss_msg(DBG, "hs_process_header", "%s: not enough DV names found on header line", name);
			goto fail;
		}
		sf->dvar[i].name = g_strdup(signam);
	}
/* sweep parameter names */
	for(i = 0; i < sf->nsweepparam; i++) {
		if((signam = strtok(NULL, " \t\n")) == NULL) {
			ss_msg(DBG, "hs_process_header", "%s: not enough sweep parameter names found on header line", name);
			goto fail;
		}
		sf->spar[i].name = g_strdup(signam);
	}

	return sf;

 fail:
	ss_delete(sf);
	return NULL;
}

/*
 * Read a "block" from an HSPICE binary file.
 * Returns number of bytes read, 0 for EOF, negative for error.
 * The body of the block is copied into the buffer pointed to by the
 * buffer-pointer pointed to by bufp, at offset offset.
 * The buffer is expanded with g_realloc if necessary. 
 * If bufp is NULL, a new buffer  is allocated.   The buffer
 * size is maintained in the int pointed to by bufsize. 
 *
 */
static int
sf_readblock_hsbin(FILE *fp, char **bufp, int *bufsize, int offset)
{
	struct hsblock_header hh;
	gint32 trailer;
	int eswap = 0;

	if(fread(&hh, sizeof(hh), 1, fp) != 1) {
		ss_msg(DBG, "sf_readblock_hsbin", "EOF reading block header");
		return 0;
	}
	if(hh.h1 == 0x04000000 && hh.h3 == 0x04000000) {
		/* detected endian swap */
		eswap = 1;
		swap_gint32((gint32*)&hh, sizeof(hh)/sizeof(gint32));
	}
	if(hh.h1 != 0x00000004 || hh.h3 != 0x00000004) {
		ss_msg(DBG, "sf_readblock_hsbin", "unexepected values in block header");
		return -1;
	}
	if(bufp == NULL) {   /* new buffer: exact fit */
		*bufsize = hh.block_nbytes;
		*bufp = g_new(char, *bufsize);
	}

	/* need to expand: double buffer size or make room for two blocks 
	 * this size, whichever is larger.  Better to realloc more now and
	 * cut down on the number of future reallocs.
	 */
	if(*bufsize < offset + hh.block_nbytes) {
		if(2 * *bufsize > (*bufsize + 2 * hh.block_nbytes))
			*bufsize *= 2;
		else
			*bufsize += 2 * hh.block_nbytes;
		*bufp = g_realloc(*bufp, *bufsize);
	}
	if(fread(*bufp + offset, sizeof(char), hh.block_nbytes, fp) != hh.block_nbytes) {
		ss_msg(DBG, "sf_readblock_hsbin", "EOF reading block body");
		return 0;
	}
	if(fread(&trailer, sizeof(gint32), 1, fp) != 1) {
		ss_msg(DBG, "sf_readblock_hsbin", "EOF reading block trailer");
		return 0;
	}
	if(eswap) {
		swap_gint32(&trailer, 1);
	}
	if(trailer != hh.block_nbytes) {
		ss_msg(DBG, "sf_readblock_hsbin", "block trailer mismatch");
		return -2;
	}
	return hh.block_nbytes;
}

/*
 * helper routine: get next floating-point value from data part of binary
 * hspice file.   Handles the block-structure of hspice files; all blocks
 * encountered are assumed to be data blocks.  We don't use readblock_hsbin because
 * some versions of hspice write very large blocks, which would require a 
 * very large buffer.
 * 
 * Returns 0 on EOF, 1 on success, negative on error.
 */
static int
sf_getval_hsbin(SpiceStream *sf, double *dval)
{
	off64_t pos;
	float val;
	struct hsblock_header hh;
	gint32 trailer;

	if(sf->read_vals >= sf->expected_vals) {
		pos = ftello64(sf->fp);
		if(fread(&trailer, sizeof(gint32), 1, sf->fp) != 1) {
			ss_msg(DBG, "sf_getval_hsbin", "EOF reading block trailer at offset 0x%lx", (long) pos);
			return 0;
		}
		if(sf->flags & SSF_ESWAP) {
			swap_gint32(&trailer, 1);
		}
		if(trailer != sf->expected_vals * sizeof(float)) {
			ss_msg(DBG, "sf_getval_hsbin", "block trailer mismatch at offset 0x%lx", (long) pos);
			return -2;
		}

		pos = ftello64(sf->fp);
		if(fread(&hh, sizeof(hh), 1, sf->fp) != 1) {
			ss_msg(DBG, "sf_getval_hsbin", "EOF reading block header at offset 0x%lx", (long) pos);
			return 0;
		}
		if(hh.h1 == 0x04000000 && hh.h3 == 0x04000000) {
			/* detected endian swap */
			sf->flags |= SSF_ESWAP;
			swap_gint32((gint32*)&hh, sizeof(hh)/sizeof(gint32));
		} else {
			sf->flags &= ~SSF_ESWAP;
		}
		if(hh.h1 != 0x00000004 || hh.h3 != 0x00000004) {
			ss_msg(ERR, "sf_getval_hsbin", "unexepected values in block header at offset 0x%lx", pos);
			return -1;
		}
		sf->expected_vals = hh.block_nbytes / sizeof(float);
		sf->read_vals = 0;
	}
	if(fread(&val, sizeof(float), 1, sf->fp) != 1) {
		pos = ftello64(sf->fp);
		ss_msg(ERR, "sf_getval_hsbin", "unexepected EOF in data at offset 0x%lx", (long) pos);
		return 0;
	}
	sf->read_vals++;

	if(sf->flags & SSF_ESWAP) {
		swap_gint32((gint32 *)&val, 1);
	}
	*dval = val;
	return 1;
}

/*
 * helper routine: get next value from ascii hspice file.
 * the file is line-oriented, with fixed-width fields on each line.
 * Lines may look like either of these two examples:
0.66687E-090.21426E+010.00000E+000.00000E+000.25000E+010.71063E-090.17877E+01
 .00000E+00 .30000E+01 .30000E+01 .30000E+01 .30000E+01 .30000E+01 .30092E-05
 * There may be whitespace at the end of the line before the newline.
 *
 * Returns 0 on EOF, 1 on success.
 */
static int
sf_getval_hsascii(SpiceStream *sf, double *val)
{
	char vbuf[16];
	char *vp;
	char *cp;
	int l;

	if(!sf->linep || (*sf->linep==0) || *sf->linep == '\n') {
		if(fgets(sf->linebuf, sf->lbufsize, sf->fp) == NULL)
			return 0;
		
		l = strlen(sf->linebuf);
		if(l) {  /* delete whitespace at end of line */
			cp = sf->linebuf + l - 1;
			while(cp > sf->linebuf && *cp && isspace(*cp))
				*cp-- = '\0';
		}
		sf->linep = sf->linebuf;
		sf->line_length = strlen(sf->linep);
		/* fprintf(stderr, "#line: \"%s\"\n", sf->linebuf); */
	}
	if(sf->linep > sf->linebuf + sf->line_length) {
		ss_msg(WARN, "sf_getval_hsascii", 
		       "%s: internal error or bad line in file", sf->filename);
		return 0;
	}

	strncpy(vbuf, sf->linep, 11);
	sf->linep += 11;
	vbuf[11] = 0;
	if(strlen(vbuf) != 11) {
		/* incomplete float value - probably truncated or
		   partialy-written file */
		return 0;
	}
	vp = vbuf;
	while(isspace(*vp)) /* atof doesn't like spaces */
		vp++;
	*val = atof(vp);
	/* fprintf(stderr, "#vp=\"%s\" val=%f\n", vp, *val); */
	return 1;
}

/* Read row of values from ascii hspice-format file.
 * Returns:
 *	1 on success.  also fills in *ivar scalar and *dvars vector
 *	0 on EOF
 *	-1 on error  (may change some ivar/dvar values)
 *	-2 on end of table, with more tables supposedly still to be read.
 */

static int 
sf_readrow_hsascii(SpiceStream *sf, double *ivar, double *dvars)
{
	int i;

	if(!sf->read_sweepparam) { /* first row of table */
		if(sf_readsweep_hsascii(sf, NULL) <= 0) /* discard sweep parameters, if any */
			return -1;  
	}
	if(sf_getval_hsascii(sf, ivar) == 0)
		return 0;
	if(*ivar >= 1.0e29) { /* "infinity" at end of data table */
		sf->read_tables++;
		if(sf->read_tables == sf->ntables)
			return 0; /* EOF */
		else
			sf->read_sweepparam = 0;
			sf->read_rows = 0;
			return -2;  /* end of table, more tables follow */
	}

	sf->read_rows++;
	for(i = 0; i < sf->ncols-1; i++) {
		if(sf_getval_hsascii(sf, &dvars[i]) == 0) {
			ss_msg(WARN, "sf_readrow_hsascii", "%s: EOF or error reading data field %d in row %d of table %d; file is incomplete.", sf->filename, i, sf->read_rows, sf->read_tables);
			return 0;
		}
	}
	return 1;
}

/* Read row of values from binary hspice-format file.
 * Returns:
 *	1 on success.  also fills in *ivar scalar and *dvars vector
 *	0 on EOF
 *	-1 on error  (may change some ivar/dvar values)
 */
static int 
sf_readrow_hsbin(SpiceStream *sf, double *ivar, double *dvars)
{
	int i;
	int rc;

	if(!sf->read_sweepparam) { /* first row of table */
		if(sf_readsweep_hsbin(sf, NULL) <= 0) /* discard sweep parameters, if any */
			return -1;  
	}

	rc = sf_getval_hsbin(sf, ivar);
	if(rc == 0)		/* file EOF */
		return 0;
	if(rc < 0)
		return -1;
	if(*ivar >= 1.0e29) { /* "infinity" at end of data table */
		sf->read_tables++;
		if(sf->read_tables == sf->ntables)
			return 0; /* end of data, should also be EOF but we don't check */
		else {
			sf->read_sweepparam = 0;
			sf->read_rows = 0;
			return -2;  /* end of table, more tables follow */
		}
	}
	sf->read_rows++;
	for(i = 0; i < sf->ncols-1; i++) {
		if(sf_getval_hsbin(sf, &dvars[i]) != 1) {
			ss_msg(WARN, "sf_readrow_hsbin", "%s: EOF or error reading data field %d in row %d of table %d; file is incomplete.", sf->filename, i, sf->read_rows, sf->read_tables);
			return 0;
		}
	}
	return 1;
}

/*
 * Read the sweep parameters from an HSPICE ascii or binary file
 * This routine must be called before the first sf_readrow_hsascii call in each data
 * table.  If it has not been called before the first readrow call, it will be called
 * with a NULL svar pointer to read and discard the sweep data. 
 *
 * returns:
 *	1 on success
 * 	-1 on error
 */
static int
sf_readsweep_hsascii(SpiceStream *sf, double *svar)
{
	int i;
	double val;
	for(i = 0; i < sf->nsweepparam; i++) {
		if(sf_getval_hsascii(sf, &val) == 0) {
			ss_msg(ERR, "sf_readsweep_hsascii", "unexpected EOF reading sweep parameters\n");
			return -1;
		}
		if(svar)
			svar[i] = val;
	}
	
	sf->read_sweepparam = 1;
	return 1;
}

static int
sf_readsweep_hsbin(SpiceStream *sf, double *svar)
{
	int i;
	double val;
	for(i = 0; i < sf->nsweepparam; i++) {
		if(sf_getval_hsbin(sf, &val) != 1) {
			ss_msg(ERR, "sf_readsweep_hsbin", "EOF or error reading sweep parameter\n");
			return -1;
		}
		if(svar)
			svar[i] = val;
	}
	
	sf->read_sweepparam = 1;
	return 1;
}


/*
 * Estimate how many rows are in the file associated with sf.
 * We base our estimate on the size of the file.
 * This can be useful to aid in memory-use planning by programs planning to
 * read the entire file.
 * 
 * If the file descriptor is not associated with an ordinary file, we return 0
 * to indicate that the length cannot be estimated.
 * If an error occurs, -1 is returned. 
 */
static long
sf_guessrows_hsbin(SpiceStream *sf)
{
	int rc;
	struct stat st;

	rc = fstat(fileno(sf->fp), &st);
	if(rc < 0)
		return -1;
	if((st.st_mode & S_IFMT) != S_IFREG)
		return 0;
	
	return st.st_size / (sizeof(float)  * sf->ncols);
}


static void
swap_gint32(gint32 *pi, size_t n)
{
	union gint32bytes *p = (union gint32bytes *)pi;
	size_t i;
	gchar temp;
	for(i = 0; i < n; i++) {
		temp = p[i].b[3] ;
		p[i].b[3] = p[i].b[0];
		p[i].b[0] = temp;

		temp = p[i].b[2] ;
		p[i].b[2] = p[i].b[1];
		p[i].b[1] = temp;
	}
}
