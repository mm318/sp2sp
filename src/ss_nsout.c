/*
 * ss_nsout.c: routines for SpiceStream that handle the ".out" file format
 * 	from Synopsis' nanosim.
 *
 * Copyright (C) 2004  Stephen G. Tell
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

static int sf_readrow_nsout(SpiceStream *sf, double *ivar, double *dvars);
static char *msgid = "nsout";

struct nsvar {
	char *name;
	int index;
	VarType type;
};

/* convert variable type string from out-file to 
 * our type numbers
 */
static VarType
sf_str2type_nsout(char *s)
{
	if(strcasecmp(s, "v") == 0)
		return VOLTAGE;
	else if(strcasecmp(s, "i") == 0)
		return CURRENT;
	else return UNKNOWN;
}


/* Read spice-type file header - nanosim "out" format */
SpiceStream *
sf_rdhdr_nsout(char *name, FILE *fp)
{
	SpiceStream *sf = NULL;
	char *line = NULL;
	int lineno = 0;
	int linesize = 1024;
	char *key, *val;
	int got_ivline = 0;
	int ndvars;
	double voltage_resolution = 1.0;
	double current_resolution = 1.0;
	double time_resolution = 1.0;
	GList *vlist = NULL;
	struct nsvar *nsv;
	int i;
	int maxindex = 0;
	
	while(fread_line(fp, &line, &linesize) != EOF) {
		lineno++;
		if(lineno == 1 && strncmp(line, ";! output_format", 16)) {
			/* not an out file; bail out */
			ss_msg(DBG, msgid, "%s:%d: Doesn't look like an ns-out file; \"output_format\" expected\n", name, lineno);
			
			return NULL;
		}
		if(line[0] == ';')
			continue;

		if(line[0] == '.') {
			key = strtok(&line[1], " \t");
			if(!key) {
				ss_msg(ERR, msgid, "%s:%d: syntax error, expected \"keyword:\"", name, lineno);
				g_free(line);
				return NULL;
			}
			if(strcmp(key, "time_resolution") == 0) {
				val = strtok(NULL, " \t\n");
				if(!val) {
					ss_msg(ERR, msgid, "%s:%d: syntax error, expected number", name, lineno);
					g_free(line);
					return NULL;
				}
				time_resolution = atof(val);
			}
			if(strcmp(key, "current_resolution") == 0) {
				val = strtok(NULL, " \t\n");
				if(!val) {
					ss_msg(ERR, msgid, "%s:%d: syntax error, expected number", name, lineno);
					g_free(line);
					return NULL;
				}
				current_resolution = atof(val);
			}
			if(strcmp(key, "voltage_resolution") == 0) {
				val = strtok(NULL, " \t\n");
				if(!val) {
					ss_msg(ERR, msgid, "%s:%d: syntax error, expected number", name, lineno);
					g_free(line);
					return NULL;
				}
				voltage_resolution = atof(val);
			}
			if(strcmp(key, "index") == 0) {
				nsv = g_new0(struct nsvar, 1);

				val = strtok(NULL, " \t\n");
				if(!val) {
					ss_msg(ERR, msgid, "%s:%d: syntax error, expected varname", name, lineno);
					goto err;
				}
				nsv->name = g_strdup(val);

				val = strtok(NULL, " \t\n");
				if(!val) {
					ss_msg(ERR, msgid, "%s:%d: syntax error, expected var-index", name, lineno);
					goto err;
				}
				nsv->index = atoi(val);
				if(nsv->index > maxindex)
					maxindex = nsv->index;
				
				val = strtok(NULL, " \t\n");
				if(!val) {
					ss_msg(ERR, msgid, "%s:%d: syntax error, expected variable type", name, lineno);
					goto err;
				}
				nsv->type = sf_str2type_nsout(val);
				vlist = g_list_append(vlist, nsv);	
			}
		}

		if(isdigit(line[0])) {
			got_ivline = 1;
			break;
		}
	}
	if(!vlist) {
		ss_msg(ERR, msgid, "%s:%d: no variable indices found in header", name, lineno);
	}
	if(!got_ivline) {
		ss_msg(ERR, msgid, "%s:%d: EOF without data-line in header", name, lineno);
		goto err;
	}
	ndvars = g_list_length(vlist);
	
	sf = ss_new(fp, name, ndvars, 0);
	sf->time_resolution = time_resolution;
	sf->current_resolution = current_resolution;
	sf->voltage_resolution = voltage_resolution;
	sf->maxindex = maxindex;
	sf->datrow = g_new0(double, maxindex+1);
	sf->nsindexes = g_new0(int, ndvars);
	sf->ncols = 1;
	sf->ntables = 1;
	sf->ivar->name = g_strdup("TIME");
	sf->ivar->type = TIME;
	sf->ivar->col = 0;
	
	for(i = 0; i < ndvars; i++) {
		nsv = g_list_nth_data(vlist, i);

		sf->dvar[i].name = g_strdup(nsv->name);
		sf->dvar[i].type = nsv->type;
		sf->nsindexes[i] = nsv->index;
		sf->dvar[i].ncols = 1;
		sf->dvar[i].col = sf->ncols;
		sf->ncols += sf->dvar[i].ncols;

		ss_msg(DBG, msgid, "dv[%d] \"%s\" nsindex=%d",
		       i, sf->dvar[i].name, sf->nsindexes[i]);
	}

	sf->readrow = sf_readrow_nsout;
	sf->read_rows = 0;

	sf->lineno = lineno;
	sf->linebuf = line;
	sf->lbufsize = linesize;
	ss_msg(DBG, msgid, "Done with header at offset 0x%lx", (long) ftello64(sf->fp));
	
	return sf;
err:
	if(line)
		g_free(line);
	if(sf) {
		sf->fp = NULL;  
		/* prevent ss_delete from cleaning up FILE*; ss_open callers 
		   may rewind and try another format on failure. */
		ss_delete(sf);
	}
	return NULL;
}

/*
 * Read row of values from an out-format file
 * upon call, line buffer should always contain the 
 * independent-variable line that starts this set of values.
 */
static int
sf_readrow_nsout(SpiceStream *sf, double *ivar, double *dvars)
{
	int i;
	int idx;
	char *sidx;
	char *sval;
	double v;
	double scale;
	SpiceVar *dvp;

	if(feof(sf->fp)) {
		return 0;
	}

	// process iv line
	v = atof(sf->linebuf) * sf->time_resolution * 1e-9; /* ns */
	*ivar = v;
	
	// read and process dv lines until we see another iv line
	while(fread_line(sf->fp, &sf->linebuf, &sf->lbufsize) != EOF) {
		sf->lineno++;
		if(sf->linebuf[0] == ';')
			continue;
		
		sidx = strtok(sf->linebuf, " \t");
		if(!sidx) {
			ss_msg(ERR, msgid, "%s:%d: expected value", 
			       sf->filename, sf->lineno);
			return -1;
		}

		sval = strtok(NULL, " \t"); 
		if(!sval)
			/* no value token: this is the ivar line for the
			    next row */
			break;

		idx = atoi(sidx);
		if(idx <= sf->maxindex) {
			sf->datrow[idx] = atof(sval);
		}
	}

	for(i = 0; i < sf->ndv; i++) {
		dvp = &sf->dvar[i];
		scale = 1.0;
		switch(dvp->type) {
		case VOLTAGE:
			scale = sf->voltage_resolution;
			break;
		case CURRENT:
			scale = sf->current_resolution;
			break;
		}
		dvars[i] = sf->datrow[ sf->nsindexes[i] ] * scale;
	}

	return 1;
}

