/*
 * wavefile.c - stuff for working with entire datasets of waveform data.
 *
 * Copyright 1999, Stephen G. Tell.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
// #include <config.h>
#include "glib.h"
#include "wavefile.h"


#ifdef HAVE_POSIX_REGEXP
#include <regex.h>
#define REGEXP_T regex_t
#define regexp_test(c,s) (regexec((c), (s), 0, NULL, 0) == 0)
static regex_t *
regexp_compile(char *str)
{
	int err;
	char ebuf[128];
	regex_t *creg;

	creg = g_new(regex_t, 1);
	err = regcomp(creg, str, REG_NOSUB|REG_EXTENDED);
	if(err)
	{
		regerror(err, creg, ebuf, sizeof(ebuf));
		fprintf(stderr, "internal error (in regexp %s):\n", str);
		fprintf(stderr, "  %s\n", ebuf);
		exit(1);
	}
	return creg;
}

#else
#include "regexp.h"	/* Henry Spencer's V8 regexp */
#define REGEXP_T regexp
#define regexp_test(c,s) regexec((c), (s))
#define regexp_compile(s) regcomp(s)
#endif

WaveFile *wf_finish_read(SpiceStream *ss);
WvTable *wf_read_table(SpiceStream *ss, WaveFile *wf, int *statep, double *ivalp, double *dvals);
void wf_init_dataset(WDataSet *ds);
inline void wf_set_point(WDataSet *ds, int n, double val);
void wf_free_dataset(WDataSet *ds);
WvTable *wvtable_new(WaveFile *wf);
void wt_free(WvTable *wt);

typedef struct
{
	char *name;
	char *fnrexp;
	REGEXP_T *creg;/* compiled form of regexp */
} DFormat;

/* table associating file typenames with filename regexps.
 * Typenames should be those supported by spicefile.c.
 *
 * Filename patterns are full egrep-style
 * regular expressions, NOT shell-style globs.
 */
static DFormat format_tab[] =
{
	{"hspice", "\\.(tr|sw|ac)[0-9]$" },
	{"cazm", "\\.[BNW]$" },
	{"spice3raw", "\\.raw$" },
	{"spice2raw", "\\.rawspice$" },
	{"nsout", "\\.out$" },
	{"ascii", "\\.(asc|acs|ascii)$" }, /* ascii / ACS format */
};
static const int NFormats = sizeof(format_tab)/sizeof(DFormat);

/*
 * Read a waveform data file.
 *  If the format name is non-NULL, only tries reading in specified format.
 *  If format not specified, tries to guess based on filename, and if
 *  that fails, tries all of the readers until one sucedes.
 *  Returns NULL on failure after printing an error message.
 *
 * TODO: use some kind of callback or exception so that client
 * can put the error messages in a GUI or somthing.
 */
WaveFile *wf_read(char *name, char *format)
{
	FILE *fp;
	SpiceStream *ss;
	int i;

	unsigned int tried = 0; /* bitmask of formats. */

	g_assert(NFormats <= 8*sizeof(tried));
	fp = fopen64(name, "r");
	if(fp == NULL)
	{
		perror(name);
		return NULL;
	}

	if(format == NULL)
	{
		for(i = 0; i < NFormats; i++)
		{
			if(!format_tab[i].creg)
			{
				format_tab[i].creg = regexp_compile(format_tab[i].fnrexp);
			}
			if(regexp_test(format_tab[i].creg, name))
			{
				tried |= 1<<i;
				ss = ss_open_internal(fp, name, format_tab[i].name);
				if(ss)
				{
					ss_msg(INFO, "wf_read", "%s: read with format \"%s\"", name, format_tab[i].name);
					return wf_finish_read(ss);
				}

				if(fseek(fp, 0L, SEEK_SET) < 0)
				{
					perror(name);
					return NULL;
				}

			}
		}
		if(tried == 0)
			ss_msg(INFO, "wf_read", "%s: couldn't guess a format from filename suffix.", name);
		/* no success with formats whose regexp matched filename,
		* try the others.
		*/
		for(i = 0; i < NFormats; i++)
		{
			if((tried & (1<<i)) == 0)
			{
				ss = ss_open_internal(fp, name, format_tab[i].name);
				if(ss)
					return wf_finish_read(ss);
				tried |= 1<<i;
				if(fseek(fp, 0L, SEEK_SET) < 0)
				{
					perror(name);
					return NULL;
				}
			}
		}
		ss_msg(ERR, "wf_read", "%s: couldn't read with any format\n", name);
		return NULL;
	}
	else     /* use specified format only */
	{
		ss = ss_open_internal(fp, name, format);
		if(ss)
			return wf_finish_read(ss);
		else
			return NULL;
	}
}

/*
 * read all of the data from a SpiceStream and store it in the WaveFile
 * structure.
 */
WaveFile *wf_finish_read(SpiceStream *ss)
{
	WaveFile *wf;
	double ival;
	double *dvals;
	WvTable *wt;
	int state;
	double *spar = NULL;

	wf = g_new0(WaveFile, 1);
	wf->ss = ss;
	wf->tables = g_ptr_array_new();
	dvals = g_new(double, ss->ncols);

	state = 0;
	do
	{
		wt = wf_read_table(ss, wf, &state, &ival, dvals);
		if(wt)
		{
			ss_msg(DBG, "wf_finish_read", "table with %d rows; state=%d", wt->nvalues, state);
			wt->swindex = wf->wf_ntables;
			g_ptr_array_add(wf->tables, wt);
			if(!wt->name)
			{
				char tmp[128];
				sprintf(tmp, "tbl%d", wf->wf_ntables);
				wt->name = g_strdup(tmp);
			}
		}
		else
		{
			ss_msg(DBG, "wf_finish_read", "NULL table; state=%d", state);
		}
	}
	while(state > 0);

	g_free(dvals);
	g_free(spar);
	ss_close(ss);

	if(state < 0)
	{
		wf_free(wf);
		return NULL;
	}
	else
	{
		return wf;
	}
}

/*
 * read data for a single table (sweep or segment) from spicestream.
 * on entry:
 *	state=0: no previous data; dvals is allocated but garbage
 *	state=1: first row of data is in *ivalp, and vals[].
 * on exit:
 *	return NULL: fatal error, *statep=-1
 *	return non-NULL: valid wvtable*
 *
 *	state=-1 fatal error
 *	state=0: successful completion of reading whole file
 * 	state=1:  finished table but more tables remain,
 *			none of the next table has yet been read
 * 	state=2:  finished table but more tables remain and
 *		*ivalp,dvals[] contain first row of next table.
 */
WvTable *
wf_read_table(SpiceStream *ss, WaveFile *wf,
              int *statep, double *ivalp, double *dvals)
{
	WvTable *wt;
	int row;
	WaveVar *dv;
	double last_ival;
	double spar;
	int rc, i, j;

	if(ss->nsweepparam > 0)
	{
		if(ss->nsweepparam == 1)
		{
			if(ss_readsweep(ss, &spar) <= 0)
			{
				*statep = -1;
				return NULL;
			}
		}
		else
		{
			ss_msg(ERR, "wf_read_table", "nsweepparam=%d; multidimentional sweeps not supported\n", ss->nsweepparam);
			*statep = -1;
			return NULL;
		}
	}
	wt = wvtable_new(wf);
	if(ss->nsweepparam == 1)
	{
		wt->swval = spar;
		wt->name = g_strdup(ss->spar[0].name);
	}
	else
	{
		wt->swval = 0;
	}

	if(*statep == 2)
	{
		wf_set_point(wt->iv->wds, row, *ivalp);
		for(i = 0; i < wt->wt_ndv; i++)
		{
			dv = &wt->dv[i];
			for(j = 0; j < dv->wv_ncols; j++)
				wf_set_point(&dv->wds[j], row,
				             dvals[dv->sv->col - 1 + j ]);
		}
		row = 1;
		wt->nvalues = 1;
		last_ival = *ivalp;
	}
	else
	{
		row = 0;
		wt->nvalues = 0;
		last_ival = -1.0e29;
	}

	while((rc = ss_readrow(ss, ivalp, dvals)) > 0)
	{
		if(row > 0 && *ivalp < last_ival)
		{
			if(row == 1)
			{
				ss_msg(ERR, "wavefile_read", "independent variable is not nondecreasing at row %d; ival=%g last_ival=%g\n", row, *ivalp, last_ival);
				wt_free(wt);
				*statep = -1;
				return NULL;

			}
			else
			{
				*statep = 2;
				return wt;
			}
		}
		last_ival = *ivalp;
		wf_set_point(wt->iv->wds, row, *ivalp);
		for(i = 0; i < wt->wt_ndv; i++)
		{
			dv = &wt->dv[i];
			for(j = 0; j < dv->wv_ncols; j++)
				wf_set_point(&dv->wds[j], row,
				             dvals[dv->sv->col - 1 + j ]);
		}
		row++;
		wt->nvalues++;
	}
	if(rc == -2)
		*statep = 1;
	else if(rc < 0)
	{
		wt_free(wt);
		*statep = -1;
		return NULL;
	}
	else
	{
		*statep = 0;
	}
	return wt;
}


/*
 * Free all memory used by a WaveFile
 */
void
wf_free(WaveFile *wf)
{
	int i;
	WvTable *wt;
	for(i = 0; i < wf->tables->len; i++)
	{
		wt = wf_wtable(wf, i);
		wt_free(wt);
	}
	g_ptr_array_free(wf->tables, 0);
	ss_delete(wf->ss);
	g_free(wf);
}

void wt_free(WvTable *wt)
{
	int i;
	for(i = 0; i < wt->wt_ndv; i++)
		wf_free_dataset(wt->dv[i].wds);
	g_free(wt->dv);
	wf_free_dataset(wt->iv->wds);
	g_free(wt->iv);
	if(wt->name)
		g_free(wt->name);
	g_free(wt);
}

/*
 * create a new, empty WvTable for a WaveFile
 */
WvTable *
wvtable_new(WaveFile *wf)
{
	WvTable *wt;
	SpiceStream *ss = wf->ss;
	int i, j;

	wt = g_new0(WvTable, 1);
	wt->wf = wf;
	wt->iv = g_new0(WaveVar, 1);
	wt->iv->sv = ss->ivar;
	wt->iv->wtable = wt;
	wt->iv->wds = g_new0(WDataSet, 1);
	wf_init_dataset(wt->iv->wds);

	wt->dv = g_new0(WaveVar, wf->ss->ndv);
	for(i = 0; i < wf->wf_ndv; i++)
	{
		wt->dv[i].wtable = wt;
		wt->dv[i].sv = &ss->dvar[i];
		wt->dv[i].wds = g_new0(WDataSet, wt->dv[i].sv->ncols);
		for(j = 0; j < wt->dv[i].sv->ncols; j++)
			wf_init_dataset(&wt->dv[i].wds[j]);
	}
	return wt;
}


/*
 * initialize common elements of WDataSet structure
 */
void
wf_init_dataset(WDataSet *ds)
{
	ds->min = G_MAXDOUBLE;
	ds->max = -G_MAXDOUBLE;

	ds->bpsize = DS_INBLKS;
	ds->bptr = g_new0(double *, ds->bpsize);
	ds->bptr[0] = g_new(double, DS_DBLKSIZE);
	ds->bpused = 1;
	ds->nreallocs = 0;
}

/*
 * free up memory pointed to by a DataSet, but not the dataset itself.
 */
void
wf_free_dataset(WDataSet *ds)
{
	int i;
	for(i = 0; i < ds->bpused; i++)
		if(ds->bptr[i])
			g_free(ds->bptr[i]);
	g_free(ds->bptr);
	g_free(ds);
}

/*
 * Iterate over all WaveVars in all sweeps/segments in the WaveFile,
 * calling the function for each one.
 */
void
wf_foreach_wavevar(WaveFile *wf, GFunc func, gpointer *p)
{
	WvTable *wt;
	WaveVar *wv;
	int i, j;

	for(i = 0; i < wf->wf_ntables; i++)
	{
		wt = wf_wtable(wf, i);
		for(j = 0; j < wf->wf_ndv; j++)
		{
			wv = &wt->dv[j];
			(func)(wv, p);
		}
	}
}

/*
 * expand dataset's storage to add one more block.
 */
void
wf_expand_dset(WDataSet *ds)
{
	if(ds->bpused >= ds->bpsize)
	{
		ds->bpsize *= 2;
		ds->bptr = g_realloc(ds->bptr, ds->bpsize * sizeof(double*));
		ds->nreallocs++;
	}
	ds->bptr[ds->bpused++] = g_new(double, DS_DBLKSIZE);
}

/*
 * set single value in dataset.   Probably can be inlined.
 */
void
wf_set_point(WDataSet *ds, int n, double val)
{
	int blk, off;
	blk = ds_blockno(n);
	off = ds_offset(n);
	while(blk >= ds->bpused)
		wf_expand_dset(ds);

	ds->bptr[blk][off] = val;
	if(val < ds->min)
		ds->min = val;
	if(val > ds->max)
		ds->max = val;
}

/*
 * get single point from dataset.   Probably can be inlined.
 */
double
wds_get_point(WDataSet *ds, int n)
{
	int blk, off;
	blk = ds_blockno(n);
	off = ds_offset(n);
	g_assert(blk <= ds->bpused);
	g_assert(off < DS_DBLKSIZE);

	return ds->bptr[blk][off];
}

/*
 * Use a binary search to return the index of the point
 * whose value is the largest not greater than ival.
 * if ival is equal or greater than the max value of the
 * independent variable, return the index of the last point.
 *
 * Only works on independent-variables, which we require to
 * be nondecreasing and have only a single column.
 *
 * Further, if there are duplicate values, returns the highest index
 * that has the same value.
 */
int
wf_find_point(WaveVar *iv, double ival)
{
	WDataSet *ds = iv->wds;
	double cval;
	int a, b;
	int n = 0;

	a = 0;
	b = iv->wv_nvalues - 1;
	if(ival >= ds->max)
		return b;
	while(a+1 < b)
	{
		cval = wds_get_point(ds, (a+b)/2);
		/*		printf(" a=%d b=%d ival=%g cval=%g\n", a,b,ival,cval); */
		if(ival < cval)
			b = (a+b)/2;
		else
			a = (a+b)/2;


		g_assert(n++ < 32);  /* > 2 ** 32 points?  must be a bug! */
	}
	return a;
}

/*
 * return the value of the dependent variable dv at the point where
 * its associated independent variable has the value ival.
 *
 * FIXME:tell
 * make this fill in an array of dependent values,
 * one for each column in the specified dependent variable.
 * This will be better than making the client call us once for each column,
 * because we'll only have to search for the independent value once.
 * (quick hack until we need support for complex and other multicolumn vars:
 * just return first column's value.)
 */
double
wv_interp_value(WaveVar *dv, double ival)
{
	int li, ri;   /* index of points to left and right of desired value */
	double lx, rx;  /* independent variable's value at li and ri */
	double ly, ry;  /* dependent variable's value at li and ri */
	WaveVar *iv;

	iv = dv->wv_iv;

	li = wf_find_point(iv, ival);
	ri = li + 1;
	if(ri >= dv->wv_nvalues)
		return wds_get_point(dv->wds, dv->wv_nvalues-1);

	lx = wds_get_point(&iv->wds[0], li);
	rx = wds_get_point(&iv->wds[0], ri);
	/*	g_assert(lx <= ival); */
	if(li > 0 && lx > ival)
	{
		fprintf(stderr, "wv_interp_value: assertion failed: lx <= ival for %s: ival=%g li=%d lx=%g\n", dv->wv_name, ival, li, lx);
	}

	ly = wds_get_point(&dv->wds[0], li);
	ry = wds_get_point(&dv->wds[0], ri);

	if(ival > rx)   /* no extrapolation allowed! */
	{
		return ry;
	}

	return ly + (ry - ly) * ((ival - lx)/(rx - lx));
}

/*
 * Find a named variable, return pointer to WaveVar
 */
WaveVar *
wf_find_variable(WaveFile *wf, char *varname, int swpno)
{
	int i;
	WvTable *wt;
	if(swpno >= wf->wf_ntables)
		return NULL;

	for(i = 0; i < wf->wf_ndv; i++)
	{
		wt = wf_wtable(wf, swpno);
		if(0==strcmp(wt->dv[i].wv_name, varname))
			return &wt->dv[i];
	}
	return NULL;
}
