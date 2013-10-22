
/*
 * wavefile.h - definitions for WaveFile, routines and data structures
 * for reading and working with entire datasets of waveform data.
 *
 * Copyright 1999,2005 Stephen G. Tell.
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

#ifndef WAVEFILE_H
#define WAVEFILE_H

#include "spicestream.h"
#include "glib.h"

typedef struct _WaveFile WaveFile;
typedef struct _WaveVar WaveVar;
typedef struct _WDataSet WDataSet;
typedef struct _WvTable WvTable;

/* Wave Data Set - 
 * an array of double-precision floating-point values,  used to store a
 * column of values.  Organized as a block structure because we don't know
 * how many entries there will be without reading the file, and we don't
 * want to read the whole thing twice.
 * 
 * Depending on what the memory allocator does, this might even 
 * end up being relatively cache-friendly.  TODO: think more about this.
 */ 

#define DS_DBLKSIZE	8192
#define DS_INBLKS	1024
#define ds_blockno(n) ((n) / DS_DBLKSIZE)
#define ds_offset(n) ((n) % DS_DBLKSIZE)

struct _WDataSet {
	double min;
	double max;
	
	/* remaining stuff is an array storage structure 
	 * that could be abstracted out and/or replaced with somthing else */
	/* pointer to array of pointers to blocks of doubles */
	double **bptr;
	int bpsize; /* size of array of pointers */
	int bpused; /* number of blocks actually allocated */
	int nreallocs;
};

/* Wave Variable - used for independent or dependent variable.
 */
struct _WaveVar {
	SpiceVar *sv;
	WvTable *wtable;  /* backpointer to file */
	WDataSet *wds;	/* data for one or more columns */
	void *udata;
};

#define wv_name		sv->name
#define wv_type		sv->type
#define wv_ncols	sv->ncols
#define wv_nvalues	wtable->nvalues
#define wv_iv		wtable->iv
#define wv_file		wtable->wf

#define wv_is_multisweep(WV) ((WV)->wtable->wf->wf_ntables>1)

/*
 * Wave Table - association of one or more dependent variables with
 *	a contiguous, nondecreasing independent variable.
 */
struct _WvTable {
	WaveFile *wf;
	int swindex;	/* index of the sweep, 0-based */
	char *name;	/* name of the sweep, if any, else NULL */
	double swval;	/* value at which the sweep was taken */
	int nvalues;	/* number of rows */
	WaveVar *iv;	/* pointer to single independent variable */
	WaveVar *dv;	/* pointer to array of dependent var info */
};

#define wt_ndv	wf->ss->ndv

/*
 * WaveFile - data struture containing all of the data from a file.
 */
struct _WaveFile {
	SpiceStream *ss;
	GPtrArray *tables;  /* array of WvTable* */
	void *udata;
};

#define wf_filename	ss->filename
#define wf_ndv		ss->ndv
#define wf_ncols	ss->ncols
#define wf_ntables	tables->len
#define wf_wtable(WF,I)	(WvTable*)g_ptr_array_index((WF)->tables, (I))


/* defined in wavefile.c */
extern WaveFile *wf_read(char *name, char *format);
extern double wv_interp_value(WaveVar *dv, double ival);
extern int wf_find_point(WaveVar *iv, double ival);
extern double wds_get_point(WDataSet *ds, int n);
extern void wf_free(WaveFile *df);
extern WaveVar *wf_find_variable(WaveFile *wf, char *varname, int swpno);
extern void wf_foreach_wavevar(WaveFile *wf, GFunc func, gpointer *p);

#endif /* WAVEFILE_H */
