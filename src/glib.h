#ifndef _GLIB_H_
#define _GLIB_H_

#define g_new(type, size)			malloc((size)*sizeof(type))
#define g_new0(type, size)			calloc((size), sizeof(type))
#define g_free(pointer)				free(pointer)
#define g_realloc(pointer, size)	realloc((pointer), (size))
#define g_strdup(pointer)			strdup((pointer))

typedef int gint32;
typedef char gchar;

#endif
