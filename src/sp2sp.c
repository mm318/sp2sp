/*
 * sp2sp - test program for spicestream library and
 * rudimentary spicefile format converter.
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
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <errno.h>
#include <unistd.h>

#include "glib.h"
#include "spicestream.h"

#define SWEEP_NONE 0
#define SWEEP_PREPEND 1
#define SWEEP_HEAD 2

int g_verbose = 0;
int sweep_mode = SWEEP_PREPEND;
char *progname = "sp2sp";

static void ascii_header_output(SpiceStream *sf, int *enab, int nidx);
static void ascii_data_output(SpiceStream *sf, int *enab, int nidx,
			      double begin_val, double end_val, int ndigits);
static int parse_field_numbers(int **index, int *idxsize, int *nsel,
			       char *list, int nfields);
static int parse_field_names(int **index, int *idxsize, int *nsel,
			     char *list, SpiceStream *sf);
static VarType get_vartype_code(char *vartype);

static void usage()
{
	int i;
	char *s;

	fprintf(stderr, "usage: %s [options] file\n", progname);
	fprintf(stderr, " options:\n");
	fprintf(stderr, "  -b V          begin output after independent-variable value V is reached\n");
	fprintf(stderr, "                instead of start of input\n");
	fprintf(stderr, "  -c T          Convert output to type T\n");
	fprintf(stderr, "  -d N          use N significant digits in output\n");
	fprintf(stderr, "  -e V          stop after independent-variable value V is reached\n");
	fprintf(stderr, "                instead of end of input.\n");
  
	fprintf(stderr, "  -f f1,f2,...  Output only fields named f1, f2, etc.\n");
	fprintf(stderr, "  -n n1,n2,...  Output only fields n1, n2, etc;\n");
	fprintf(stderr, "                independent variable is field number 0\n");
	fprintf(stderr, "  -u U          Output only variables with units of type; U\n");
	fprintf(stderr, "                U = volts, amps, etc.\n");
	fprintf(stderr, "  -s S          Handle sweep parameters as S:\n");
	fprintf(stderr, "  -s head         add header-like comment line\n");
	fprintf(stderr, "  -s prepend      prepend columns to all output lines\n");
	fprintf(stderr, "  -s none         ignore sweep info\n");
	fprintf(stderr, "  -t T          Assume that input is of type T\n");
	fprintf(stderr, "  -v            Verbose - print detailed signal information\n");
	fprintf(stderr, " output format types:\n");
	fprintf(stderr, "   none - no data output\n");
	fprintf(stderr, "   ascii - lines of space-seperated numbers, with header\n");
	fprintf(stderr, "   nohead - lines of space-seperated numbers, no headers\n");
	fprintf(stderr, "   cazm - CAzM format\n");
	fprintf(stderr, " input format types:\n");
	
	i = 0;
	while((s = ss_filetype_name(i++))) {
		fprintf(stderr, "    %s\n", s);
	}
}

int
main(int argc, char **argv)
{
	SpiceStream *sf;

	int i;
	int idx;
	extern int optind;
	extern char *optarg;
	// int x_flag = 0;
	int errflg = 0;
	char *infiletype = "hspice";
	char *outfiletype = "ascii";
	char *fieldnamelist = NULL;
	char *fieldnumlist = NULL;
	int *out_indices = NULL;
	int outi_size = 0;
	int nsel = 0;
	VarType vartype = UNKNOWN;
	int c;
	int ndigits = 7;
	double begin_val = -DBL_MAX;
	double end_val = DBL_MAX;

	while ((c = getopt (argc, argv, "b:c:d:e:f:n:s:t:u:vx")) != EOF) {
		switch(c) {
		case 'v':
			spicestream_msg_level = DBG;
			g_verbose = 1;
			break;
		case 'b':
			begin_val = atof(optarg);
			break;
		case 'c':
			outfiletype = optarg;
			break;
		case 'd':
			ndigits = atoi(optarg);
			if(ndigits < 5)
				ndigits = 5;
			break;
		case 'e':
			end_val = atof(optarg);
			break;
		case 'f':
			fieldnamelist = optarg;
			break;
		case 'n':
			fieldnumlist = optarg;
			break;
		case 's':
			if(strcmp(optarg, "none") == 0)
				sweep_mode = SWEEP_NONE;
			else if(strcmp(optarg, "prepend") == 0)
				sweep_mode = SWEEP_PREPEND;
			else if(strcmp(optarg, "head") == 0)
				sweep_mode = SWEEP_HEAD;
			else {
				fprintf(stderr, "unknown sweep-data style %s\n", optarg);
				exit(1);
			}
			break;
		case 't':
			infiletype = optarg;
			break;
		case 'u':
			vartype = get_vartype_code(optarg);
			break;
		case 'x':
			spicestream_msg_level = DBG;
			// x_flag = 1;
			break;
		default:
			errflg = 1;
			break;
		}
	}

	if(errflg || optind >= argc)  {
		usage();
		exit(1);
	}

	sf = ss_open(argv[optind], infiletype);
	if(!sf) {
		if(errno)
			perror(argv[optind]);
		fprintf(stderr, "unable to read file\n");
		exit(1);
	}
	if(g_verbose) {
		printf("filename: \"%s\"\n", sf->filename);
		printf("  columns: %d\n", sf->ncols);
		printf("  tables: %d\n", sf->ntables);
		printf("independent variable:\n");
		printf("  name: \"%s\"\n", sf->ivar->name);
		printf("  type: %s\n", vartype_name_str(sf->ivar->type));
		printf("  col: %d\n", sf->ivar->col);
		printf("  ncols: %d\n", sf->ivar->ncols);
		printf("sweep parameters: %d\n", sf->nsweepparam);
		for(i = 0; i < sf->nsweepparam; i++) {
			printf("  name: \"%s\"\n", sf->spar[i].name);
			printf("  type: %s\n", vartype_name_str(sf->spar[i].type));
		}
		printf("dependent variables: %d\n", sf->ndv);
		for(i = 0; i < sf->ndv; i++) {
			printf(" dv[%d] \"%s\" ", i, sf->dvar[i].name);
			printf(" (type=%s col=%d ncols=%d)\n", 
			       vartype_name_str(sf->dvar[i].type),
			       sf->dvar[i].col,
			       sf->dvar[i].ncols);
		}
	}

	if(fieldnamelist == NULL && fieldnumlist == NULL) {
		out_indices = g_new0(int, sf->ndv+1);
		nsel = 0;
		idx = 0;
		for(i = 0; i < sf->ndv+1; i++) {
			if(i == 0 || 
			   (vartype == UNKNOWN 
			    || sf->dvar[i-1].type == vartype)) {
				out_indices[idx++] = i;
				nsel++;
			}
		}
	}
	if(fieldnumlist)
		if(parse_field_numbers(&out_indices, &outi_size, &nsel, 
				    fieldnumlist, sf->ndv+1) < 0)
			exit(1);
	if(fieldnamelist)
		if(parse_field_names(&out_indices, &outi_size, &nsel,
				  fieldnamelist, sf) < 0)
			exit(1);
	if(nsel == 0) {
		fprintf(stderr, "No fields selected for output\n");
		exit(0);
	}

	if(strcmp(outfiletype, "cazm") == 0) {
		printf("* CAZM-format output converted with sp2sp\n");
		printf("\n");
		printf("TRANSIENT ANALYSIS\n");
		ascii_header_output(sf, out_indices, nsel);
		ascii_data_output(sf, out_indices, nsel, begin_val, end_val, ndigits);
	} else if(strcmp(outfiletype, "ascii") == 0) {
		ascii_header_output(sf, out_indices, nsel);
		ascii_data_output(sf, out_indices, nsel, begin_val, end_val, ndigits);
	} else if(strcmp(outfiletype, "nohead") == 0) {
		ascii_data_output(sf, out_indices, nsel, begin_val, end_val, ndigits);
	} else if(strcmp(outfiletype, "none") == 0) {
		/* do nothing */
	} else {
		fprintf(stderr, "%s: invalid output type name: %s\n",
			progname, outfiletype);
	}

	ss_close(sf);

	exit(0);
}

/*
 * print all column headers.  
 * For multicolumn variables, ss_var_name will generate a column name
 * consisting of the variable name plus a suffix.
 */
static void
ascii_header_output(SpiceStream *sf, int *indices, int nidx)
{
	int i, j;
	char buf[1024];

	if((sf->nsweepparam > 0) && (sweep_mode == SWEEP_PREPEND)) {
		for(i = 0; i < sf->nsweepparam; i++) {
			printf("%s ", sf->spar[i].name);	
		}
	}
	for(i = 0; i < nidx; i++) {
		if(i > 0)
			putchar(' ');
		if(indices[i] == 0) {
			ss_var_name(sf->ivar, 0, buf, 1024);
			printf("%s", buf);
		} else {
			int varno = indices[i]-1;
			for(j = 0; j < sf->dvar[varno].ncols; j++) {
				if(j > 0)
					putchar(' ');
				ss_var_name(&sf->dvar[varno], j, buf, 1024);
				printf("%s", buf);
			}
		}
	}
	putchar('\n');
}

/*
 * print data as space-seperated columns.
 */
static void
ascii_data_output(SpiceStream *sf, int *indices, int nidx, 
		  double begin_val, double end_val, int ndigits)
{
	int i, j, tab;
	int rc;
	double ival;
	double *dvals;
	double *spar = NULL;
	int done;

	dvals = g_new(double, sf->ncols);
	if(sf->nsweepparam > 0)
		spar = g_new(double, sf->nsweepparam);
	
	done = 0;
	tab = 0;
	while(!done) {
		if(sf->nsweepparam > 0) {
			if(ss_readsweep(sf, spar) <= 0)
				break;
		}
		if(tab > 0 && sweep_mode == SWEEP_HEAD) {
			printf("# sweep %d;", tab);
			for(i = 0; i < sf->nsweepparam; i++) {
				printf(" %s=%g", sf->spar[i].name, spar[i]);
			}
			putchar('\n');
		}
		while((rc = ss_readrow(sf, &ival, dvals)) > 0) {
			if(ival < begin_val)
				continue;
			if(ival > end_val) {
				/* past end_val, but can only stop reading
				   early if if there is only one sweep-table
				   in the file. */ 
				if(sf->ntables == 1)
					break;
				else
					continue;
			}

			if((sf->nsweepparam > 0) && (sweep_mode == SWEEP_PREPEND)) {
				for(i = 0; i < sf->nsweepparam; i++) {
					printf("%.*g ", ndigits, spar[i]);
				}
			}
			for(i = 0; i < nidx; i++) {
				if(i > 0)
					putchar(' ');
				if(indices[i] == 0)
					printf("%.*g", ndigits, ival);
				else {
					int varno = indices[i]-1;
					int dcolno = sf->dvar[varno].col - 1;
					for(j = 0; j < sf->dvar[varno].ncols; j++) {
						if(j > 0)
							putchar(' ');
						printf("%.*g", ndigits,
						       dvals[dcolno+j]);
					}
				}
			}
			putchar('\n');
		}
		if(rc == -2) {  /* end of sweep, more follow */
			if(sf->nsweepparam == 0)
				sweep_mode = SWEEP_HEAD;
			tab++;
		} else {  	/* EOF or error */
			done = 1;
		}
	}
	g_free(dvals);
	if(spar)
		g_free(spar);
}

static int parse_field_numbers(int **indices, int *idxsize, int *nidx, 
			       char *list, int nfields)
{
	int n, i;
	char *fnum;
	int err = 0;
	int *idx;
	if(!*indices || idxsize == 0) {
		*idxsize = nfields*2;
		idx = g_new0(int, *idxsize);
		*indices = idx;
		*nidx = 0;
	}

	fnum = strtok(list, ", \t");
	i = 0;
	while(fnum) {
		if(*nidx >= *idxsize) {
			*idxsize *= 2;
			idx = g_realloc(idx, (*idxsize) * sizeof(int));
			*indices = idx;
		}
		n = atoi(fnum);
		if(n < 0 || n >= nfields) {
			fprintf(stderr, "bad field number in -n option: %s\n", fnum);
			err = -1;
		} else {
			idx[i++] = n;
			(*nidx)++;
		}
		fnum = strtok(NULL, ", \t");
	}
	return err;
}


/*
 * Try looking for named dependent variable.  Try twice, 
 * first as-is, then with "v(" prepended the way hspice mangles things.
 */
static int find_dv_by_name(char *name, SpiceStream *sf)
{
	int i;
	for(i = 0; i < sf->ndv; i++) {
		if(strcasecmp(name, sf->dvar[i].name) == 0)
			return i;
	}
	for(i = 0; i < sf->ndv; i++) {
		if(strncasecmp("v(", sf->dvar[i].name, 2) == 0
		   && strcasecmp(name, &sf->dvar[i].name[2]) == 0)
			return i;
	}
	return -1;
}

/*
 * parse comma-seperated list of field names.  Turn on the output-enables
 * for the listed fields.
 */
static int parse_field_names(int **indices, int *idxsize, int *nidx,
			     char *list, SpiceStream *sf)
{
	int err = 0;
	int n;
	char *fld;
	int i;
	int *idx = 0;

	if(!*indices || idxsize == 0) {
		*idxsize = (sf->ndv+1)*2;
		idx = g_new0(int, *idxsize);
		*indices = idx;
		*nidx = 0;
	}

	fld = strtok(list, ", \t");
	i = 0;
	while(fld) {
		if(*nidx >= *idxsize) {
			*idxsize *= 2;
			idx = g_realloc(idx, (*idxsize) * sizeof(int));
			*indices = idx;
		}
		if(strcasecmp(fld, sf->ivar->name)==0) {
			idx[i++] = 0;
			(*nidx)++;
		} else if((n = find_dv_by_name(fld, sf)) >= 0) {
			idx[i++] = n+1;
			(*nidx)++;
		} else {
			fprintf(stderr, "field name in -f option not found in file: %s\n", fld);
			err = -1;
		}
		fld = strtok(NULL, ", \t");
	}
	return err;
}


struct vtlistel {
	VarType t;
	char *s;
};
static struct vtlistel vtlist[] = {
	{TIME, "time"},
	{VOLTAGE, "volt"},
	{VOLTAGE, "volts"},
	{VOLTAGE, "voltage"},
	{CURRENT, "current"},
	{CURRENT, "amps"},
	{FREQUENCY, "freq"},
	{FREQUENCY, "frequency"},
	{FREQUENCY, "hertz"},
	{UNKNOWN, NULL},
};

/*
 * Given a variable type name, return a numeric VarType.
 * Returns 0 (UNKNOWN) if no match.
 */
static VarType get_vartype_code(char *vartype)
{
	int i;
	for(i = 0; vtlist[i].s; i++) {
		if(strcasecmp(vartype, vtlist[i].s) == 0)
			return vtlist[i].t;
	}
	return UNKNOWN;
}
