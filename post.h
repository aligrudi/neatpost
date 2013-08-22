/* predefined array limits */
#define PATHLEN		1024	/* path length */
#define NFONTS		32	/* number of fonts */
#define NGLYPHS		1024	/* glyphs in fonts */
#define NLIGS		32	/* number of font ligatures */
#define NKERNS		512	/* number of font pairwise kerning pairs */
#define FNLEN		64	/* font name length */
#define GNLEN		32	/* glyph name length */
#define ILNLEN		1000	/* line limit of input files */
#define LNLEN		4000	/* line buffer length (ren.c/out.c) */
#define LIGLEN		4	/* length of ligatures */

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) < (b) ? (b) : (a))
#define LEN(a)		(sizeof(a) / sizeof((a)[0]))

/* device related variables */
extern int dev_res;
extern int dev_uwid;
extern int dev_hor;
extern int dev_ver;

struct glyph {
	char name[GNLEN];	/* name of the glyph */
	char id[GNLEN];		/* device-dependent glyph identifier */
	struct font *font;	/* glyph font */
	int wid;		/* character width */
	int type;		/* character type; ascender/descender */
};

struct font {
	char name[FNLEN];
	char fontname[FNLEN];
	struct glyph glyphs[NGLYPHS];
	int nglyphs;
	int spacewid;
	int special;
	char c[NGLYPHS][FNLEN];		/* character names in charset */
	struct glyph *g[NGLYPHS];	/* character glyphs in charset */
	int n;				/* number of characters in charset */
	/* font ligatures */
	char lig[NLIGS][LIGLEN * GNLEN];
	int nlig;
	/* glyph list based on the first character of glyph names */
	int head[256];			/* glyph list head */
	int next[NGLYPHS];		/* next item in glyph list */
	/* kerning pair list per glyph */
	int knhead[NGLYPHS];		/* kerning pairs of glyphs[] */
	int knnext[NKERNS];		/* next item in knhead[] list */
	int knpair[NKERNS];		/* kerning pair 2nd glyphs */
	int knval[NKERNS];		/* font pairwise kerning value */
	int knn;			/* number of kerning pairs */
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
int font_lig(struct font *fn, char **c, int n);
int font_kern(struct font *fn, char *c1, char *c2);

/* output functions */
void out(char *s, ...);
void outc(char *s);
void outh(int h);
void outv(int v);
void outrel(int h, int v);
void outfont(int f);
void outsize(int s);
void outcolor(int c);
void outpage(void);
void outmnt(int f);
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
void ps_header(void);
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
