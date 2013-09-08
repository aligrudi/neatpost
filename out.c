#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

static int o_f, o_s, o_m;	/* font and size */
static int o_h, o_v;		/* current user position */
static int p_f, p_s, p_m;	/* output postscript font */
static int o_qtype;		/* queued character type */
static int o_qv, o_qh, o_qend;	/* queued character position */
char o_fonts[FNLEN * NFONTS] = " ";

static void outvf(char *s, va_list ap)
{
	vfprintf(stdout, s, ap);
}

void outf(char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	outvf(s, ap);
	va_end(ap);
}

static void o_flush(void)
{
	if (o_qtype == 1)
		outf(") %d %d w\n", o_qh, o_qv);
	if (o_qtype == 2)
		outf("] %d %d g\n", o_qh, o_qv);
	o_qtype = 0;
}

void outpage(void)
{
	o_flush();
	o_v = 0;
	o_h = 0;
	p_s = 0;
	p_f = 0;
	p_m = 0;
}

static void o_queue(struct glyph *g)
{
	int type = 1 + !isdigit((g)->id[0]);
	int num = atoi(g->id);
	if (o_qtype != type || o_qend != o_h || o_qv != o_v) {
		o_flush();
		o_qh = o_h;
		o_qv = o_v;
		o_qtype = type;
		outf(type == 1 ? "(" : "[");
	}
	if (o_qtype == 1) {
		if (num >= ' ' && num <= '~')
			outf("%s%c", strchr("()\\", num) ? "\\" : "", num);
		else
			outf("\\%d%d%d", (num >> 6) & 7, (num >> 3) & 7, num & 7);
	} else {
		outf("/%s", g->id);
	}
	o_qend = o_h + charwid(g->wid, o_s);
}

/* calls o_flush() if necessary */
void out(char *s, ...)
{
	va_list ap;
	o_flush();
	va_start(ap, s);
	outvf(s, ap);
	va_end(ap);
}

/* glyph placement adjustments */
static struct fixlist {
	char *name;
	int dh, dv;
} fixlist[] = {
	{"", -5, 4},
	{"", 20, 0},
	{"", 20, 0},
	{"", -11, 0},
	{"", -11, 0},
	{"", -50, 0},
	{"br", -5, 4},
	{"lc", 20, 0},
	{"lf", 20, 0},
	{"rc", -11, 0},
	{"rf", -11, 0},
	{"rn", -50, 0},
};

static void fixpos(struct glyph *g, int *dh, int *dv)
{
	struct font *fn = g->font;
	int i;
	*dh = 0;
	*dv = 0;
	if (!strcmp("S", fn->name) && !strcmp("Symbol", fn->fontname)) {
		for (i = 0; i < LEN(fixlist); i++) {
			if (!strcmp(fixlist[i].name, g->name)) {
				*dh = charwid(fixlist[i].dh, o_s);
				*dv = charwid(fixlist[i].dv, o_s);
				return;
			}
		}
	}
}

static void out_fontup(int fid)
{
	char fnname[FNLEN];
	struct font *fn;
	if (o_m != p_m) {
		out("%d %d %d rgb\n", CLR_R(o_m), CLR_G(o_m), CLR_B(o_m));
		p_m = o_m;
	}
	if (fid != p_f || o_s != p_s) {
		fn = dev_font(fid);
		out("%d /%s f\n", o_s, fn->fontname);
		p_f = fid;
		p_s = o_s;
		sprintf(fnname, " %s ", fn->fontname);
		if (!strstr(o_fonts, fnname))
			sprintf(strchr(o_fonts, '\0'), "%s ", fn->fontname);
	}
}

void outc(char *c)
{
	struct glyph *g;
	struct font *fn;
	int dh, dv;
	g = dev_glyph(c, o_f);
	fn = g ? g->font : dev_font(o_f);
	if (!g) {
		outrel(*c == ' ' && fn ? charwid(fn->spacewid, o_s) : 1, 0);
		return;
	}
	out_fontup(dev_fontid(fn));
	fixpos(g, &dh, &dv);
	o_h += dh;
	o_v += dv;
	o_queue(g);
	o_h -= dh;
	o_v -= dv;
}

void outh(int h)
{
	o_h = h;
}

void outv(int v)
{
	o_v = v;
}

void outrel(int h, int v)
{
	o_h += h;
	o_v += v;
}

void outfont(int f)
{
	o_f = f;
}

/* a font was mounted at pos f */
void outmnt(int f)
{
	if (p_f == f)
		p_f = -1;
}

void outsize(int s)
{
	o_s = s;
}

void outcolor(int c)
{
	o_m = c;
}

static int draw_path;	/* number of path segments */
static int draw_point;	/* point was set for postscript newpath */

static void drawmv(void)
{
	if (!draw_point)
		outf("%d %d m ", o_h, o_v);
	draw_point = 1;
}

/* start a multi-segment path */
void drawmbeg(char *s)
{
	o_flush();
	out_fontup(o_f);
	draw_path = 1;
	outf("gsave newpath %s\n", s);
}

/* end a multi-segment path */
void drawmend(char *s)
{
	draw_path = 0;
	draw_point = 0;
	outf("%s grestore\n", s);
}

void drawbeg(void)
{
	o_flush();
	out_fontup(o_f);
	if (draw_path)
		return;
	outf("newpath ");
}

void drawend(int close, int fill)
{
	if (draw_path)
		return;
	draw_point = 0;
	if (close)
		outf("closepath ");
	if (fill)
		outf("fill\n");
	else
		outf("stroke\n");
}

void drawl(int h, int v)
{
	drawmv();
	outrel(h, v);
	outf("%d %d drawl ", o_h, o_v);
}

void drawc(int c)
{
	drawmv();
	outrel(c, 0);
	outf("%d %d drawe ", c, c);
}

void drawe(int h, int v)
{
	drawmv();
	outrel(h, 0);
	outf("%d %d drawe ", h, v);
}

void drawa(int h1, int v1, int h2, int v2)
{
	drawmv();
	outf("%d %d %d %d drawa ", h1, v1, h2, v2);
	outrel(h1 + h2, v1 + v2);
}

void draws(int h1, int v1, int h2, int v2)
{
	drawmv();
	outf("%d %d %d %d %d %d draws ", o_h, o_v, o_h + h1, o_v + v1,
		o_h + h1 + h2, o_v + v1 + v2);
	outrel(h1, v1);
}
