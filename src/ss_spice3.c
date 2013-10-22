/*
 * ss_spice3.c: routines for SpiceStream that handle the file formats
 * 	known as Berkeley Spice3 Rawfile
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
 * You should have received a copy of the GNU Library General Public
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

// #include <config.h>
#include "glib.h"
#include "spicestream.h"

static int sf_readrow_s3raw(SpiceStream *sf, double *ivar, double *dvars);
char *msgid = "s3raw";
static int sf_readrow_s3bin(SpiceStream *sf, double *ivar, double *dvars);

/* convert variable type string from spice3 raw file to
 * our type numbers
 */
static VarType
sf_str2type_s3raw(char *s)
{
	if(strcasecmp(s, "voltage") == 0)
		return VOLTAGE;
	else if(strcasecmp(s, "current") == 0)
		return CURRENT;
	else if(strcasecmp(s, "frequency") == 0)
		return FREQUENCY;
	else if(strcasecmp(s, "time") == 0)
		return TIME;
	else return UNKNOWN;
}


/* Read spice-type file header - Berkeley Spice3 "raw" format */
SpiceStream* sf_rdhdr_s3raw(char *name, FILE *fp)
{
	SpiceStream *sf = NULL;
	char *line = NULL;
	int lineno = 0;
	int linesize = 1024;
	char *key, *val;
	int nvars = 0, npoints = 0;
	int got_nvars = 0;
	int got_values = 0;
	int dtype_complex = 0;
	int binary = 0;
	char *vnum, *vname, *vtypestr;
	int i = 0;

	while(fread_line(fp, &line, &linesize) != EOF)
	{
		lineno++;
		if(lineno == 1 && strncmp(line, "Title: ", 7))
		{
			/* not a spice3raw file; bail out */
			ss_msg(DBG, msgid, "%s:%d: Doesn't look like a spice3raw file; \"Title:\" expected\n", name, lineno);

			return NULL;
		}

		key = strtok(line, ":");
		if(!key)
		{
			ss_msg(ERR, msgid, "%s:%d: syntax error, expected \"keyword:\"", name, lineno);
			g_free(line);
			return NULL;
		}
		if(strcmp(key, "Flags") == 0)
		{
			while((val = strtok(NULL, " ,\t\n")))
			{
				if(strcmp(val, "real") == 0)
				{
					dtype_complex = 0;
				}
				if(strcmp(val, "complex") == 0)
				{
					dtype_complex = 1;
				}
			}
		}
		else if(strcmp(key, "No. Variables") == 0)
		{
			val = strtok(NULL, " \t\n");
			if(!val)
			{
				ss_msg(ERR, msgid, "%s:%d: syntax error, expected integer", name, lineno);
				g_free(line);
				return NULL;
			}
			nvars = atoi(val);
			got_nvars = 1;
		}
		else if(strcmp(key, "No. Points") == 0)
		{
			val = strtok(NULL, " \t\n");
			if(!val)
			{
				ss_msg(ERR, msgid, "%s:%d: syntax error, expected integer", name, lineno);
				g_free(line);
				return NULL;
			}
			npoints = atoi(val);
		}
		else if(strcmp(key, "Variables") == 0)
		{
			if(!got_nvars)
			{
				ss_msg(ERR, msgid, "%s:%d: \"Variables:\" before \"No. Variables:\"", name, lineno, i);
				goto err;

			}
			sf = ss_new(fp, name, nvars-1, 0);
			sf->ncols = 1;
			sf->ntables = 1;
			/* first variable may be described on the same line
			 * as "Variables:" keyword
			 */
			vnum = strtok(NULL, " \t\n");

			for(i = 0; i < nvars; i++)
			{
				if(i || !vnum)
				{
					if(fread_line(fp, &line, &linesize) == EOF)
					{
						ss_msg(ERR, msgid, "%s:%d: Unexpected EOF in \"Variables:\" at var %d", name, lineno, i);
						goto err;
					}
					lineno++;
					vnum = strtok(line, " \t\n");
				}
				vname = strtok(NULL, " \t\n");
				vtypestr = strtok(NULL, " \t\n");
				if(!vnum || !vname || !vtypestr)
				{
					ss_msg(ERR, msgid, "%s:%d: expected number name type", name, lineno);
					goto err;
				}
				if(i == 0)   /* assume Ind.Var. first */
				{
					sf->ivar->name = g_strdup(vname);
					sf->ivar->type = sf_str2type_s3raw(vtypestr);
					sf->ivar->col = 0;
					/* ivar can't really be two-column,
					   this is a flag that says to
					   discard 2nd point */
					if(dtype_complex)
						sf->ivar->ncols = 2;
					else
						sf->ivar->ncols = 1;

				}
				else
				{
					sf->dvar[i-1].name = g_strdup(vname);
					sf->dvar[i-1].type = sf_str2type_s3raw(vtypestr);
					sf->dvar[i-1].col = sf->ncols;
					if(dtype_complex)
						sf->dvar[i-1].ncols = 2;
					else
						sf->dvar[i-1].ncols = 1;

					sf->ncols += sf->dvar[i-1].ncols;
				}
			}
		}
		else if(strcmp(key, "Values") == 0)
		{
			got_values = 1;
			break;
		}
		else if(strcmp(key, "Binary") == 0)
		{
			binary = 1;
			got_values = 1;
			break;
		}
		if(got_values)
			break;
	}
	if(!sf)
	{
		ss_msg(ERR, msgid, "%s:%d: no \"Variables:\" section in header", name, lineno);
		goto err;
	}
	if(!got_values)
	{
		ss_msg(ERR, msgid, "%s:%d: EOF without \"Values:\" in header", name, lineno);
		goto err;
	}

	if(binary)
	{
		sf->readrow = sf_readrow_s3bin;
	}
	else
	{
		sf->readrow = sf_readrow_s3raw;
	}
	sf->read_rows = 0;
	sf->expected_vals = npoints * (sf->ncols + (dtype_complex ? 1 : 0));
	ss_msg(DBG, msgid, "expecting %d values\n", sf->expected_vals);
	sf->lineno = lineno;
	sf->linebuf = line;
	sf->lbufsize = linesize;
	ss_msg(DBG, msgid, "Done with header at offset 0x%lx\n", (long) ftello64(sf->fp));

	return sf;
err:
	if(line)
		g_free(line);
	if(sf)
	{
		sf->fp = NULL;
		/* prevent ss_delete from cleaning up FILE*; ss_open callers
		   may rewind and try another format on failure. */
		ss_delete(sf);
	}
	return NULL;
}


/* return pointer to the next whitespace-seperated token in the file
 * advances to the next lines of the file as needed.
 * pointer points into the line buffer linebuf.
 * token will not be nul-terminated; whole line remains available.
 *
 * upon return, sf->linep points to the char after the end of the token,
 * which might be the trailing nul or might be whitespace.
 */
static char *sf_nexttoken(SpiceStream *sf)
{
	char *cp;
	char *tok = NULL;

	if(sf->linep)
		cp = sf->linep;
	else
	{
		if(fread_line(sf->fp, &sf->linebuf, &sf->lbufsize) == EOF)
		{
			return 0;  /* normal EOF */
		}
		sf->lineno++;
		cp = sf->linebuf;
	}

	// search for start of token
	while(!tok)
	{
		if(*cp == 0)
		{
			do
			{
				if(fread_line(sf->fp, &sf->linebuf, &sf->lbufsize) == EOF)
				{
					return 0;  /* normal EOF */
				}
				sf->lineno++;
				cp = sf->linebuf;
			}
			while (*cp == 0);   // skip multiple blank lines
		}
		if(!isspace(*cp))
			tok = cp;
		else
			cp++;
	}
	// tok now points to start of the token; search for the end
	while(*cp && !isspace(*cp))
		cp++;
	sf->linep = cp;
	return tok;
}


/*
 * Read row of values from an ascii spice3 raw file
 */
static int
sf_readrow_s3raw(SpiceStream *sf, double *ivar, double *dvars)
{
	int i;
	// int frownum;
	char *tok;
	double v;

	if((sf->flags & SSF_PUSHBACK) == 0)
	{
		tok = sf_nexttoken(sf);
		if(!tok)
		{
			return 0;
			// ss_msg(ERR, msgid, "%s:%d: expected row number",
			//        sf->filename, sf->lineno);
			// return -1;
		}
		if(!isdigit(*tok))
		{
			ss_msg(WARN, msgid, "%s:%d: expected row number, got \"%s\". Note: only one dataset per file is supported, extra garbage ignored",
			       sf->filename, sf->lineno, tok);
			return 0;
		}
		// frownum = atoi(tok);
		/* todo: check for expected and maximum row number */

		tok = sf_nexttoken(sf);
		if(!tok)
		{
			ss_msg(WARN, msgid, "%s:%d: expected ivar value",
			       sf->filename, sf->lineno);
			return -1;
		}
		v = atof(tok);
		if(v < sf->ivval)
		{
			/* independent-variable value decreased, this must
			 * be the start of another sweep.  hold the value and
			 * return flag to caller.
			 */
			sf->ivval = v;
			sf->flags |= SSF_PUSHBACK;
			return -2;
		}
		else
		{
			sf->ivval = v;
			*ivar = v;
		}
	}
	else
	{
		/* iv value for start of new sweep was read last time */
		sf->flags &= ~SSF_PUSHBACK;
		*ivar = sf->ivval;
	}

	for(i = 0; i < sf->ndv; i++)
	{
		SpiceVar *dv;
		dv = &sf->dvar[i];

		tok = sf_nexttoken(sf);
		if(!tok)
		{
			ss_msg(ERR, msgid, "%s:%d: expected value",
			       sf->filename, sf->lineno);
			return -1;
		}
		dvars[dv->col-1] = atof(tok);

		if(dv->ncols > 1)
		{
			tok = strchr(tok, ',');
			if(!tok || !*(tok+1))
			{
				ss_msg(ERR, msgid, "%s:%d: expected second value",
				       sf->filename, sf->lineno);
				return -1;
			}
			tok++;
			dvars[dv->col] = atof(tok);
		}
	}
	sf->read_rows++;
	return 1;
}

/*
 * Read a single value from binary spice3 rawfile, and do
 * the related error-checking.
 */

static int
sf_getval_s3bin(SpiceStream *sf, double *dval)
{
	off64_t pos;
	double val;

	if(sf->read_vals >= sf->expected_vals)
	{
		pos = ftello64(sf->fp);
		ss_msg(DBG, "sf_getval_s3bin", "past last expected value offset 0x%lx", (long) pos);
		return 0;
	}
	if(fread(&val, sizeof(double), 1, sf->fp) != 1)
	{
		pos = ftello64(sf->fp);
		ss_msg(ERR, "sf_getval_s3bin", "unexepected EOF in data at offset 0x%lx", (long) pos);
		return -1;
	}
	sf->read_vals++;

	*dval = val;
	return 1;
}


/*
 * Read row of values from a binay spice3 raw file
 */
static int
sf_readrow_s3bin(SpiceStream *sf, double *ivar, double *dvars)
{
	int i, rc;
	double v;
	double dummy;

	if((sf->flags & SSF_PUSHBACK) == 0)
	{
		rc = sf_getval_s3bin(sf, &v);
		if(rc == 0)		/* file EOF */
			return 0;
		if(rc < 0)
			return -1;
		if(sf->ivar->ncols == 2)
		{
			rc = sf_getval_s3bin(sf, &dummy);
			if(rc == 0)		/* file EOF */
				return 0;
			if(rc < 0)
				return -1;
		}
		if(v < sf->ivval)
		{
			/* independent-variable value decreased, this must
			 * be the start of another sweep.  hold the value and
			 * return flag to caller.
			 */
			sf->ivval = v;
			sf->flags |= SSF_PUSHBACK;
			return -2;
		}
		else
		{
			sf->ivval = v;
			*ivar = v;
		}
	}
	else
	{
		/* iv value for start of new sweep was read last time */
		sf->flags &= ~SSF_PUSHBACK;
		*ivar = sf->ivval;
	}

	for(i = 0; i < sf->ncols-1; i++)
	{
		if(sf_getval_s3bin(sf, &dvars[i]) != 1)
		{
			ss_msg(WARN, "sf_readrow_s3bin", "%s: EOF or error reading data field %d in row %d; file is incomplete.", sf->filename, i, sf->read_rows);
			return 0;
		}
	}

	sf->read_rows++;
	return 1;
}
