/* predefined array limits */
#define PATHLEN		1024	/* path length */
#define NFONTS		32	/* number of fonts */
#define FNLEN		64	/* font name length */
#define GNLEN		32	/* glyph name length */
#define ILNLEN		1000	/* line limit of input files */
#define LNLEN		4000	/* line buffer length (ren.c/out.c) */

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) < (b) ? (b) : (a))
#define LEN(a)		(sizeof(a) / sizeof((a)[0]))

/* device related variables */
extern int dev_res;
extern int dev_uwid;
extern int dev_hor;
extern int dev_ver;

struct glyph {
	char id[GNLEN];		/* device-dependent glyph identifier */
	char name[GNLEN];	/* the first character mapped to this glyph */
	struct font *font;	/* glyph font */
	int wid;		/* character width */
	int type;		/* character type; ascender/descender */
	int pos;		/* glyph code */
};

/* output device functions */
int dev_open(char *dir, char *dev);
void dev_close(void);
int dev_mnt(int pos, char *id, char *name);
struct font *dev_font(int fn);
int dev_fontid(struct font *fn);
struct glyph *dev_glyph(char *c, int fn);

/* font-related functions */
struct font *font_open(char *path);
void font_close(struct font *fn);
struct glyph *font_glyph(struct font *fn, char *id);
struct glyph *font_find(struct font *fn, char *name);
int font_wid(struct font *fn, int sz, int w);
int font_swid(struct font *fn, int sz);
char *font_name(struct font *fn);

/* output functions */
void out(char *s, ...);
void outc(char *s);
void outh(int h);
void outv(int v);
void outrel(int h, int v);
void outfont(int f);
void outsize(int s);
void outcolor(int c);
void outrotate(int deg);
void outeps(char *eps);
void outlink(char *spec);
void outpage(void);
void outmnt(int f);
void outgname(int g);
extern char o_fonts[];

void drawbeg(void);
void drawend(int close, int fill);
void drawmbeg(char *s);
void drawmend(char *s);
void drawl(int h, int v);
void drawc(int c);
void drawe(int h, int v);
void drawa(int h1, int v1, int h2, int v2);
void draws(int h1, int v1, int h2, int v2);

/* postscript functions */
void ps_header(int pagewidth, int pageheight, int linewidth);
void ps_trailer(int pages, char *fonts);
void ps_pagebeg(int n);
void ps_pageend(int n);

/* colors */
#define CLR_R(c)		(((c) >> 16) & 0xff)
#define CLR_G(c)		(((c) >> 8) & 0xff)
#define CLR_B(c)		((c) & 0xff)
#define CLR_RGB(r, g, b)	(((r) << 16) | ((g) << 8) | (b))

char *clr_str(int c);
int clr_get(char *s);

/* mapping integers to sets */
struct iset *iset_make(void);
void iset_free(struct iset *iset);
int *iset_get(struct iset *iset, int key);
void iset_put(struct iset *iset, int key, int ent);
int iset_len(struct iset *iset, int key);

/* mapping strings to longs */
struct dict *dict_make(int notfound, int dupkeys);
void dict_free(struct dict *d);
void dict_put(struct dict *d, char *key, int val);
int dict_get(struct dict *d, char *key);
int dict_idx(struct dict *d, char *key);
char *dict_key(struct dict *d, int idx);
int dict_val(struct dict *d, int idx);
int dict_prefix(struct dict *d, char *key, int *idx);

/* memory allocation */
void *xmalloc(long len);
void *mextend(void *old, long oldsz, long newsz, int memsz);
