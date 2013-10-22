/*
 * ss_spice2.c: routines for SpiceStream that handle the output
 * 	format from Berkeley Spice2G6
 *
 * Copyright 1998,1999  Stephen G. Tell
 * Copyright 1998 D. Jeff Dionne
 *
 * Based on rd_spice2.c that Jeff Dione contributed to gwave-0.0.4,
 * this was largely rewritten by Steve Tell for the spicestream library.
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
#include "spice2.h"
#include "spicestream.h"

static int sf_readrow_s2raw(SpiceStream *sf, double *ivar, double *dvars);
static char *msgid = "s2raw";

/* Read spice-type file header - Berkeley Spice2G6 "raw" format */
SpiceStream *
sf_rdhdr_s2raw(char *name, FILE *fp)
{
	SpiceStream *sf = NULL;
	int ndv;
	int i;
	char *cp;
	spice_hdr_t s2hdr;
	spice_var_name_t s2vname;
	spice_var_type_t s2vtype;
	spice_var_loc_t s2vloc;
	spice_plot_title_t s2title;
	spice_var_t s2var;

	if(fread (&s2var,sizeof(s2var),1,fp) != 1)
		return NULL;
        if (memcmp(&s2var,SPICE_MAGIC,8)) {
		ss_msg(DBG, msgid, "%s: not a spice2 rawfile (bad magic number)", name);
                return NULL;
        }
	if(fread (&s2hdr,sizeof(s2hdr),1,fp) != 1)
		return NULL;
	ss_msg(DBG, msgid, "%s: nvars=%d const=%d analysis mode %d",
	       name, s2hdr.nvars, s2hdr.const4, s2hdr.mode);

	/* independent variable name */
	if(fread (&s2vname,sizeof(s2vname),1,fp) != 1)
		return NULL;
	s2vname.name[7] = 0;
	if(cp = strchr(s2vname.name, ' '))
		*cp = 0;

	ndv = s2hdr.nvars - 1;
	sf = ss_new(fp, name, ndv, 0);
	sf->ncols = ndv;
	sf->ivar->name = g_strdup(s2vname.name);
	sf->ivar->type = TIME;
	sf->ivar->col = 0;
	sf->ivar->ncols = 1;

        for (i = 0; i < ndv; i++) {
		if(fread (&s2vname, sizeof(s2vname), 1, fp) != 1)
			goto err;
		s2vname.name[7] = 0;
		if(cp = strchr(s2vname.name, ' '))
			*cp = 0;

		sf->dvar[i].name = g_strdup(s2vname.name);
		sf->dvar[i].type = VOLTAGE;  /* FIXME:sgt: get correct type */
		sf->dvar[i].col = i; /* FIXME:sgt: handle complex */
		sf->dvar[i].ncols = 1;
	}

	if(fread (&s2vtype, sizeof(s2vtype), 1, fp) != 1)
		goto err;
	for (i = 0; i < ndv; i++) {
		if(fread (&s2vtype, sizeof(s2vtype), 1, fp) != 1)
			goto err;
	}

	if(fread (&s2vloc, sizeof(s2vloc), 1, fp) != 1)
		goto err;
	for (i = 0; i < ndv; i++) {
		if(fread (&s2vloc, sizeof(s2vloc), 1, fp) != 1)
			goto err;
	}
	if(fread (&s2title, sizeof(s2title), 1, fp) != 1)
		goto err;
	s2title.title[23] = 0;
	ss_msg(DBG, msgid, "title=\"%s\"", s2title.title);
	ss_msg(DBG, msgid, "done with header at offset=0x%lx", (long) ftello64(fp));

	sf->readrow = sf_readrow_s2raw;
	return sf;
err:
	if(sf) {
		ss_delete(sf);
	}
	return NULL;
}


/*
 * Read row of values from a spice2 rawfile
 */
static int
sf_readrow_s2raw(SpiceStream *sf, double *ivar, double *dvars)
{
	int i, rc;
	spice_var_t val;

	/* independent var */
	if ((rc = fread (&val,sizeof(val),1, sf->fp)) != 1) {
		if(rc == 0)
			return 0;
		else
			return -1;
	}
	if (memcmp(&val,SPICE_MAGIC,8) == 0) /* another analysis */
		return 0;
	*ivar = val.val;
	
	/* dependent vars */
	for(i = 0; i < sf->ndv; i++) {
		if(fread(&val, sizeof(val), 1, sf->fp) != 1) {
			ss_msg(ERR, msgid, "unexpected EOF at dvar %d", i);
			return -1;
		}
		dvars[i] = val.val;
	}
	return 1;
}
