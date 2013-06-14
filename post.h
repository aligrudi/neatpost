/* predefined array limits */
#define PATHLEN		1024	/* path length */
#define NFONTS		32	/* number of fonts */
#define FNLEN		32	/* font name length */
#define NGLYPHS		512	/* glyphs in fonts */
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
	char name[FNLEN];	/* name of the glyph */
	char id[FNLEN];		/* device-dependent glyph identifier */
	struct font *font;	/* glyph font */
	int wid;		/* character width */
	int type;		/* character type; ascender/descender */
};

struct font {
	char name[FNLEN];
	char psname[FNLEN];
	struct glyph glyphs[NGLYPHS];
	int nglyphs;
	int spacewid;
	int special;
	char c[NGLYPHS][FNLEN];		/* character names in charset */
	struct glyph *g[NGLYPHS];	/* character glyphs in charset */
	int n;				/* number of characters in charset */
};

/* output device functions */
int dev_open(char *path);
void dev_close(void);
int dev_mnt(int pos, char *id, char *name);
struct font *dev_font(int fn);
int dev_fontid(struct font *fn);
int charwid(int wid, int sz);
struct glyph *dev_glyph(char *c, int fn);
struct glyph *dev_glyph_byid(char *id, int fn);

/* font-related functions */
struct font *font_open(char *path);
void font_close(struct font *fn);
struct glyph *font_glyph(struct font *fn, char *id);
struct glyph *font_find(struct font *fn, char *name);

/* output functions */
void out(char *s, ...);
void outc(char *s);
void outh(int h);
void outv(int v);
void outrel(int h, int v);
void outfont(int f);
void outsize(int s);
void outpage(void);
extern char o_fonts[];

void drawbeg(char *s);
void drawend(char *s);
void drawl(int h, int v);
void drawc(int c);
void drawe(int h, int v);
void drawa(int h1, int v1, int h2, int v2);
void draws(int h1, int v1, int h2, int v2);

/* postscript functions */
void ps_header(void);
void ps_trailer(int pages, char *fonts);
void ps_pagebeg(int n);
void ps_pageend(int n);
