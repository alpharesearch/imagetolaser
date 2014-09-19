#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>


static int verbose_flag = 0;	// Set by verbose flag.

double tool_radius = 0.1;	// mm. radius of laser.
double stepover = 0.2;
double max_cut = 255;		// mm. Max plunge depth.
double zscale = 255;              // mm. Depth of image.
double max_error = 0.001;	// mm
double clear = 2;		// clearance Z-height. Top of work is assumed to be zero.
double res = 0.2;		// Sampling size in mm.
int issues = 0;
int nclines = 4;
double z_last_good = 0;

struct image
{
  int xsize;
  int ysize;
  double res;
  double data[];
};

typedef struct image I;

//=========================================================
double mx, my, mz;
double mdx, mdy, mdz;
double msteps;

double
projection_error(double x, double y, double z)
{
  double px = mx + mdx / msteps * (msteps + 1);
  double py = my + mdy / msteps * (msteps + 1);
  double pz = mz + mdz / msteps * (msteps + 1);

  px -= x;
  py -= y;
  pz -= z;

  return sqrt(px * px + py * py + pz * pz);
}

void
do_route_to(double x, double y, double z)
{
  printf("G1 X%.2f Y%.2f S%.0f\n", x, y, z*-1);
  mx = x;
  my = y;
  mz = z;
  mdx = mdy = mdz = 0;
}

void
route_to(double x, double y, double z)
{
  if (msteps > 0) {
    double err = projection_error(x, y, z);
    if (err < max_error) {
      mdx = mdx / msteps * (msteps + 1);
      mdy = mdy / msteps * (msteps + 1);
      mdz = mdz / msteps * (msteps + 1);
      msteps++;
      return;
    }
    do_route_to(mx + mdx, my + mdy, mz + mdz);
    msteps = 0;
  }
  if (msteps < 0) {
    do_route_to(x, y, z);
    msteps = 0;
    return;
  }
  mdx = x - mx;
  mdy = y - my;
  mdz = z - mz;
  msteps++;
}

void
finish(void)
{
  if (mdx > 0 || mdy > 0 || mdz > 0) {
    do_route_to(mx + mdx, my + mdy, mz + mdz);
  }
  msteps = -1;
}

void
move_to(double x, double y, double z)
{
  finish();
  printf("G0 X%.2f Y%.2f\nM3 S0\nF1000\n", x, y);
  mx = x;
  my = y;
  mz = z;
}

void
retract(void)
{
  finish();
  printf("M6\n");
  mz = 5;
}


double
min_height(I * img, double tx, double ty, double tradius)
{
  double z = -9999;

  int dx, dy;
  int sx, sy, ex, ey;
  int x, y, radius;

  x = tx / img->res;
  y = ty / img->res;
  radius = tradius / img->res + 1;

  sx = -radius;
  sy = -radius;
  ex = +radius;
  ey = +radius;
  if ((sx + x) < 0)
    sx = -x;
  if ((sy + y) < 0)
    sy = -y;
  if ((ex + x) >= img->xsize)
    ex = img->xsize - x - 1;
  if ((ey + y) >= img->ysize)
    ey = img->ysize - y - 1;

  radius *= radius;
  for (dx = sx; dx <= ex; ++dx) {
    for (dy = sy; dy <= ey; ++dy) {
      if ((dx * dx + dy * dy) > radius) continue;
      double h = img->data[img->xsize * (y + dy) + (x + dx)];
      double tdx = (x + dx) * img->res - tx;
      double tdy = (y + dy) * img->res - ty;
      double d = (tdx * tdx + tdy * tdy);
      if (d > tradius * tradius) continue;
      h += sqrt(tradius * tradius - d);

      z = (z > h && z > -255) ? z : h;	// max(z,h)
    }
  }
  if(z == -9999) {
	  issues++;
	  z = z_last_good;
  }
  else z_last_good = z;
  nclines++;
  return z;
}

void
render_image(I * img, double xsize, double ysize, double step)
{
  double x = 0, y = 0;
  move_to(x, 0, clear + tool_radius);
  for (x = 0; x < xsize; x += step) {
    for (y = 0; y < ysize; y += step) {
      double h = min_height(img, x, y, tool_radius);
      route_to(x, y, h);
    }
    x += step;
    for (y -= step; y >= 0; y -= step) {
      double h = min_height(img, x, y, tool_radius);
      route_to(x, y, h);
    }
  }
  retract();
}

void
cross_render_image(I * img, double xsize, double ysize, double step)
{
  double x = 0, y = 0;
  move_to(0, 0, clear + tool_radius);
  for (y = 0; y <= ysize; y += step) {
    for (x = 0; x <= xsize; x += step) {
      double h = min_height(img, x, y, tool_radius);
      route_to(x, y, h);
    }
    y += step;
    for (x -= step; x >= 0; x -= step) {
      double h = min_height(img, x, y, tool_radius);
      route_to(x, y, h);
    }
  }
  retract();
}


//=============================================================
I *
read_pgm(FILE * f)
{
  I *img;
  int x, y, range;

  fscanf(f, "P5\n");
  fscanf(f, "%d", &x);
  fscanf(f, "%d", &y);
  fscanf(f, "%d", &range);
  if (range != 255) {
    fprintf(stderr, "Sorry, can only handle 8-bit images for now.\n");
    exit(1);
  }

  img = malloc(sizeof(*img) + sizeof(img->data[0]) * x * y);
  img->xsize = x;
  img->ysize = y;
  img->res = 1;
  int i;

  unsigned char c;
  fread(&c, sizeof(c), 1, f);
  for (i = 0; i < (x * y); ++i) {
    c = 0;
    fread(&c, sizeof(c), 1, f);
    img->data[i] = 1.0 * c / 256.0 - 1.0;
    img->data[i] *= zscale;
  }
  fclose(f);

  return img;
}

I *
img_zero(int x, int y)
{
  I *img = malloc(sizeof(*img) + sizeof(img->data[0]) * x * y);
  img->xsize = x;
  img->ysize = y;

  int i;
  for (i = 0; i < x * y; ++i)
    img->data[i] = 0;

  return img;
}

//=============================================================
int
main(int argc, char *argv[])
{
  static struct option long_options[] = {
    /* These options set a flag. */
    {"verbose", no_argument, &verbose_flag, 1},
    /* These options don't set a flag.
       We distinguish them by their indices. */
    {"toolsize", required_argument, 0, 't'},
    {"stepover", required_argument, 0, 'p'},
    {"zdepth", required_argument, 0, 'z'},
    {"scale", required_argument, 0, 's'},
    {"clear", required_argument, 0, 'c'},
    {0, 0, 0, 0}
  };

  while (1) {
    /* getopt_long stores the option index here. */
    int option_index = 0;

    char c =
      getopt_long(argc, argv, "t:p:z:s:c:", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 0:
      break;			/* This option set a flag, do nothing else now. */

    case 't':
      tool_radius = atof(optarg) / 2;
      break;

    case 'p':
      stepover = atof(optarg);
      break;

    case 'z':
      zscale = atof(optarg);
      break;

    case 's':
      res = atof(optarg);
      break;

    case 'c':
      clear = atof(optarg);

    case '?':
      /* getopt_long already printed an error message. */
      break;

    default:
      fprintf(stderr, "How odd: '%c' %d\n", c, c);
      abort();
    }
  }

  /* Instead of reporting `--verbose'
     and `--brief' as they are encountered,
     we report the final status resulting from them. */
  if (verbose_flag)
    fprintf(stderr, "verbose flag is set.\n");

  FILE *f = stdin;

  /* Remaining command line argument is filename if supplied. */
  if (optind < argc) {
    f = fopen(argv[optind], "r");
    if (!f) {
      fprintf(stderr, "Failed to open %s.\n", argv[optind]);
      exit(1);
    }
  }

  I *img = read_pgm(f);
  fprintf(stderr, "Image size: %d x %d\n", img->xsize, img->ysize);
  fprintf(stderr, "Output till be %.1fmm x %.1fmm\n", res * img->xsize,
	  res * img->ysize);
  img->res = res;
  cross_render_image(img, res * img->xsize, res * img->ysize, stepover);
  fprintf(stderr, "Issues: %i\nnc lines: %i", issues, nclines);	
  return 0;
}
