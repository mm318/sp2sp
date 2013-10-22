
/*
 * ssintern.h: internal definitions for spicestream library
 *
 * Copyright (C) 1998-2003  Stephen G. Tell
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
 */

/* try to arrange to be able ue fopen64 and off64_t inside the spicestream
 * library only.
 * None of spicestream's API is sensitive to whether or not we
 * have large file support.
 *
 * In particular, gwave (and guile) are ignorant of all this.  No telling if
 * guile on a particular system has large file issues.
 * this is why we don't use autoconf's AC_SYS_LARGEFILE.
 */

#define _LARGEFILE64_SOURCE 1
#include <unistd.h>

#if !defined(_LFS64_STDIO)
// #define fopen64 fopen
// #define ftello64 ftello
// #define off64_t off_t
#endif
/* wish there was a way to portably printf either a 64-bit or 32-bit off_t
 * without cluttering the rest of the source with #ifdefs.
 */
