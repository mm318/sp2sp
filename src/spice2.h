/* header file for spice2g6 raw file structures */

typedef struct {
  char title[80];
  char date[8];
  char time[8];
  short mode:16;
  short nvars:16;
  short const4:16;
} spice_hdr_t;

typedef struct {
  char name[8];
} spice_var_name_t;

typedef short spice_var_type_t;

typedef short spice_var_loc_t;

typedef struct {
  char title[24];
} spice_plot_title_t;

#define SPICE_MAGIC "rawfile1"

typedef union {
  double val;
  struct {
    float r;
    float j;
  } cval;
  char magic[8];
} spice_var_t;
