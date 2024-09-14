/* predefined array limits */
#define PATHLEN		1024	/* path length */
#define NFONTS		1024	/* number of fonts */
#define FNLEN		64	/* font name length */
#define GNLEN		32	/* glyph name length */
#define GNFMT		"%31s"	/* glyph name scanf format */
#define ILNLEN		1000	/* line limit of input files */

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
struct font *dev_fontopen(char *name);

/* font-related functions */
struct font *font_open(char *path);
void font_close(struct font *fn);
struct glyph *font_glyph(struct font *fn, char *id);
struct glyph *font_find(struct font *fn, char *name);
int font_wid(struct font *fn, int sz, int w);
int font_swid(struct font *fn, int sz);
char *font_name(struct font *fn);
char *font_path(struct font *fn);
int font_glnum(struct font *fn, struct glyph *g);
struct glyph *font_glget(struct font *fn, int id);
char *font_desc(struct font *fn);

/* output functions */
void out(char *s, ...);
void outc(char *s);
void outh(int h);
void outv(int v);
void outrel(int h, int v);
void outfont(int f);
void outsize(int s);
void outcolor(int c);
void outrotate(double deg);
void outeps(char *eps, int hwid, int vwid);
void outpdf(char *pdf, int hwid, int vwid);
void outlink(char *dst, int hwid, int vwid);
void outmark(int n, char (*desc)[256], int *page, int *off, int *level);
void outname(int n, char (*desc)[64], int *page, int *off);
void outinfo(char *kwd, char *val);
void outset(char *var, char *val);
void outpage(void);
void outmnt(int f);
void outgname(int g);

void drawbeg(void);
void drawend(int close, int fill);
void drawmbeg(char *s);
void drawmend(char *s);
void drawl(int h, int v);
void drawc(int c);
void drawe(int h, int v);
void drawa(int h1, int v1, int h2, int v2);
void draws(int h1, int v1, int h2, int v2);

void docheader(char *title, int pagewidth, int pageheight, int linewidth);
void doctrailer(int pages);
void docpagebeg(int n);
void docpageend(int n);

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
struct dict *dict_make(int notfound, int dupkeys, int hashlen);
void dict_free(struct dict *d);
void dict_put(struct dict *d, char *key, int val);
int dict_get(struct dict *d, char *key);
int dict_idx(struct dict *d, char *key);
char *dict_key(struct dict *d, int idx);
int dict_val(struct dict *d, int idx);
int dict_prefix(struct dict *d, char *key, int *idx);

/* memory allocation */
void *mextend(void *old, long oldsz, long newsz, int memsz);
/* helper functions */
char *pdftext_static(char *s);

/* string buffers */
struct sbuf *sbuf_make(void);
char *sbuf_buf(struct sbuf *sb);
char *sbuf_done(struct sbuf *sb);
void sbuf_free(struct sbuf *sb);
int sbuf_len(struct sbuf *sbuf);
void sbuf_str(struct sbuf *sbuf, char *s);
void sbuf_printf(struct sbuf *sbuf, char *s, ...);
void sbuf_chr(struct sbuf *sbuf, int c);
void sbuf_mem(struct sbuf *sbuf, char *s, int len);
void sbuf_cut(struct sbuf *sb, int len);

/* reading PDF files */
int pdf_ws(char *pdf, int len, int pos);
int pdf_len(char *pdf, int len, int pos);
int pdf_type(char *pdf, int len, int pos);
int pdf_dval(char *pdf, int len, int pos, char *key);
int pdf_dkey(char *pdf, int len, int pos, int key);
int pdf_lval(char *pdf, int len, int pos, int idx);
int pdf_trailer(char *pdf, int len);
int pdf_obj(char *pdf, int len, int pos, int *obj, int *rev);
int pdf_find(char *pdf, int len, int obj, int rev);
int pdf_ref(char *pdf, int len, int pos);
int pdf_dval_val(char *pdf, int len, int pos, char *key);
int pdf_dval_obj(char *pdf, int len, int pos, char *key);
