#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

static char ps_title[256];	/* document title */
static char ps_author[256];	/* document author */
static int ps_height;		/* document height in basic units */
static int o_f, o_s, o_m;	/* font and size */
static int o_h, o_v;		/* current user position */
static int p_f, p_s, p_m;	/* output postscript font */
static int o_qtype;		/* queued character type */
static int o_qv, o_qh, o_qend;	/* queued character position */
static int o_rh, o_rv;
static double o_rdeg;		/* previous rotation position and degree */
static int o_gname;		/* use glyphshow for all glyphs */

static char o_fonts[FNLEN * NFONTS] = " ";

static void outvf(char *s, va_list ap)
{
	vfprintf(stdout, s, ap);
}

static void outf(char *s, ...)
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

void outgname(int g)
{
	o_gname = g;
}

void outpage(void)
{
	o_flush();
	o_v = 0;
	o_h = 0;
	p_s = 0;
	p_f = 0;
	p_m = 0;
	o_rdeg = 0;
}

static void o_queue(struct glyph *g)
{
	int type = 1 + (g->pos <= 0 || o_gname);
	if (o_qtype != type || o_qend != o_h || o_qv != o_v) {
		o_flush();
		o_qh = o_h;
		o_qv = o_v;
		o_qtype = type;
		outf(type == 1 ? "(" : "[");
	}
	if (o_qtype == 1) {
		if (g->pos >= ' ' && g->pos <= '~')
			outf("%s%c", strchr("()\\", g->pos) ? "\\" : "", g->pos);
		else
			outf("\\%d%d%d", (g->pos >> 6) & 7,
					(g->pos >> 3) & 7, g->pos & 7);
	} else {
		outf("/%s", g->id);
	}
	o_qend = o_h + font_wid(g->font, o_s, g->wid);
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
		out("%d /%s f\n", o_s, font_name(fn));
		p_f = fid;
		p_s = o_s;
		sprintf(fnname, " %s ", font_name(fn));
		if (!strstr(o_fonts, fnname))
			sprintf(strchr(o_fonts, '\0'), "%s ", font_name(fn));
	}
}

void outc(char *c)
{
	struct glyph *g;
	struct font *fn;
	g = dev_glyph(c, o_f);
	fn = g ? g->font : dev_font(o_f);
	if (!g) {
		outrel(*c == ' ' && fn ? font_swid(fn, o_s) : 1, 0);
		return;
	}
	out_fontup(dev_fontid(fn));
	o_queue(g);
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
	if (dev_font(f))
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
	if (s > 0)
		o_s = s;
}

void outcolor(int c)
{
	o_m = c;
}

void outrotate(double deg)
{
	o_flush();
	out_fontup(o_f);
	if (o_rdeg)
		outf("%f %d %d rot\n", -o_rdeg, o_rh, o_rv);
	o_rdeg = deg;
	o_rh = o_h;
	o_rv = o_v;
	outf("%f %d %d rot\n", deg, o_h, o_v);
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

void outeps(char *eps, int hwid, int vwid)
{
	char buf[1 << 12];
	int llx, lly, urx, ury;
	FILE *filp;
	int nbb, ver;
	if (!(filp = fopen(eps, "r")))
		return;
	if (!fgets(buf, sizeof(buf), filp)) {
		fprintf(stderr, "warning: file %s is empty\n", eps);
		fclose(filp);
		return;
	}
	if (sscanf(buf, "%%!PS-Adobe-%d.%d EPSF-%d.%d", &ver, &ver, &ver, &ver) != 4) {
		fprintf(stderr, "warning: unsupported EPSF header in %s\n", eps);
		fclose(filp);
		return;
	}
	nbb = 0;
	while (fgets(buf, sizeof(buf), filp))
		if (!strncmp(buf, "%%BoundingBox: ", 15))
			if ((nbb = sscanf(buf + 15, "%d %d %d %d",
					&llx, &lly, &urx, &ury)) == 4)
				break;
	fclose(filp);
	if (nbb < 4)		/* no BoundingBox comment */
		return;
	if (hwid <= 0 && vwid <= 0)
		hwid = (urx - llx) * dev_res / 72;
	if (vwid <= 0)
		vwid = (ury - lly) * hwid / (urx - llx);
	if (hwid <= 0)
		hwid = (urx - llx) * vwid / (ury - lly);
	/* output the EPS file */
	o_flush();
	out_fontup(o_f);
	outf("%d %d %d %d %d %d %d %d EPSFBEG\n",
		llx, lly, hwid, urx - llx, vwid, ury - lly, o_h, o_v);
	outf("%%%%BeginDocument: %s\n", eps);
	filp = fopen(eps, "r");
	while (fgets(buf, sizeof(buf), filp))
		out("%s", buf);
	fclose(filp);
	outf("%%%%EndDocument\n");
	outf("EPSFEND\n");
}

void outpdf(char *pdf, int hwid, int vwid)
{
}

void outlink(char *lnk, int hwid, int vwid)
{
	o_flush();
	if (lnk[0] == '#' || isdigit((unsigned char) lnk[0])) {
		outf("[ /Rect [ %d %d t %d %d t ] %s%s "
			"/Subtype /Link /LNK pdfmark\n",
			o_h, o_v, o_h + hwid, o_v + vwid,
			lnk[0] == '#' ? "/Dest /" : "/Page ",
			lnk[0] == '#' ? lnk + 1 : lnk);
	} else {
		outf("[ /Rect [ %d %d t %d %d t ] "
			"/Action << /Subtype /URI /URI %s >> /Open true "
			"/Subtype /Link /LNK pdfmark\n",
			o_h, o_v, o_h + hwid, o_v + vwid, pdftext_static(lnk));
	}
}

void outname(int n, char (*desc)[64], int *page, int *off)
{
	int i;
	o_flush();
	for (i = 0; i < n; i++) {
		outf("[ /Dest /%s", desc[i]);
		outf(" /Page %d", page[i]);
		if (off[i] > 0)
			outf(" /View [/XYZ null %d null]",
				(ps_height - off[i]) * 72 / dev_res);
		outf(" /DEST pdfmark\n");
	}
}

void outmark(int n, char (*desc)[256], int *page, int *off, int *level)
{
	int i, j;
	o_flush();
	for (i = 0; i < n; i++) {
		int cnt = 0;
		for (j = i + 1; j < n && level[j] > level[i]; j++)
			if (level[j] == level[i] + 1)
				cnt++;
		outf("[ /Title %s", pdftext_static(desc[i]));
		if (page[i] > 0)
			outf(" /Page %d", page[i]);
		if (cnt > 0)
			outf(" /Count %d", cnt);
		if (off[i] > 0)
			outf(" /View [/XYZ null %d null]",
				(ps_height - off[i]) * 72 / dev_res);
		outf(" /OUT pdfmark\n");
	}
}

void outinfo(char *kwd, char *val)
{
	if (!strcmp("Author", kwd))
		snprintf(ps_author, sizeof(ps_author), "%s", val);
	if (!strcmp("Title", kwd))
		snprintf(ps_title, sizeof(ps_title), "%s", val);
}

void outset(char *var, char *val)
{
}

void docpagebeg(int n)
{
	out("%%%%Page: %d %d\n", n, n);
	out("/saveobj save def\n");
	out("mark\n");
	out("%d pagesetup\n", n);
}

void docpageend(int n)
{
	out("cleartomark\n");
	out("showpage\n");
	out("saveobj restore\n");
}

void doctrailer(int pages)
{
	out("[");
	if (ps_title[0])
		out(" /Title %s", pdftext_static(ps_title));
	if (ps_author[0])
		out(" /Author %s", pdftext_static(ps_author));
	out(" /Creator (Neatroff) /DOCINFO pdfmark\n");
	out("%%%%Trailer\n");
	out("done\n");
	out("%%%%DocumentFonts: %s\n", o_fonts);
	out("%%%%Pages: %d\n", pages);
	out("%%%%EOF\n");
}

static char *prolog =
	"/setup {\n"
	"	counttomark 2 idiv {def} repeat pop\n"
	"	/scaling 72 resolution div def\n"
	"	linewidth setlinewidth\n"
	"	1 setlinecap\n"
	"	0 pagesize 1 get translate\n"
	"	scaling scaling scale\n"
	"	0 0 moveto\n"
	"} def\n"
	"\n"
	"/pagesetup {\n"
	"	/page exch def\n"
	"	currentdict /pagedict known currentdict page known and {\n"
	"		page load pagedict exch get cvx exec\n"
	"	} if\n"
	"} def\n"
	"\n"
	"/pdfmark where\n"
	"	{ pop globaldict /?pdfmark /exec load put }\n"
	"	{ globaldict begin\n"
	"		/?pdfmark /pop load def\n"
	"		/pdfmark /cleartomark load def\n"
	"		end }\n"
	"	ifelse\n"
	"\n"
	"/t {neg} bind def\n"
	"/w {neg moveto show} bind def\n"
	"/m {neg moveto} bind def\n"
	"/g {neg moveto {glyphshow} forall} bind def\n"
	"/rgb {255 div 3 1 roll 255 div 3 1 roll 255 div 3 1 roll setrgbcolor} bind def\n"
	"/rot {/y exch def /x exch def x y neg translate rotate x neg y translate} bind def\n"
	"/done {/lastpage where {pop lastpage} if} def\n"
	"\n"
	"% caching fonts, as selectfont is supposed to be doing\n"
	"/fncache 16 dict def\n"
	"/selectfont_append { fncache exch dup findfont put } bind def\n"
	"/selectfont_cached {\n"
	"	exch dup fncache exch known not { dup selectfont_append } if\n"
	"	fncache exch get exch scalefont setfont\n"
	"} bind def\n"
	"/f {\n"
	"	exch dup 3 1 roll scaling div selectfont_cached\n"
	"	linewidth mul scaling 10 mul div setlinewidth\n"
	"} bind def\n"
	"\n"
	"/savedmatrix matrix def\n"
	"/drawl {\n"
	"	neg lineto\n"
	"} bind def\n"
	"/drawe {\n"
	"	savedmatrix currentmatrix pop scale\n"
	"	.5 0 rmoveto currentpoint .5 0 rmoveto .5 0 360 arc\n"
	"	savedmatrix setmatrix\n"
	"} bind def\n"
	"/drawa {\n"
	"	/dy2 exch def\n"
	"	/dx2 exch def\n"
	"	/dy1 exch def\n"
	"	/dx1 exch def\n"
	"	currentpoint dy1 neg add exch dx1 add exch\n"
	"	dx1 dx1 mul dy1 dy1 mul add sqrt\n"
	"	dy1 dx1 neg atan\n"
	"	dy2 neg dx2 atan\n"
	"	arc\n"
	"} bind def\n"
	"/draws {\n"
	"	/y2 exch def\n"
	"	/x2 exch def\n"
	"	/y1 exch def\n"
	"	/x1 exch def\n"
	"	/y0 exch def\n"
	"	/x0 exch def\n"
	"	x0 5 x1 mul add 6 div\n"
	"	y0 5 y1 mul add -6 div\n"
	"	x2 5 x1 mul add 6 div\n"
	"	y2 5 y1 mul add -6 div\n"
	"	x1 x2 add 2 div\n"
	"	y1 y2 add -2 div\n"
	"	curveto\n"
	"} bind def\n"
	"% including EPS files\n"
	"/EPSFBEG {\n"
	"	/epsf_state save def\n"
	"	neg translate\n"
	"	div 3 1 roll div exch scale\n"
	"	neg exch neg exch translate\n"
	"	/dict_count countdictstack def\n"
	"	/op_count count 1 sub def\n"
	"	userdict begin\n"
	"	/showpage { } def\n"
	"	0 setgray 0 setlinecap 1 setlinewidth 0 setlinejoin\n"
	"	10 setmiterlimit [ ] 0 setdash newpath\n"
	"} bind def\n"
	"/EPSFEND {\n"
	"	count op_count sub {pop} repeat\n"
	"	countdictstack dict_count sub {end} repeat\n"
	"	epsf_state restore\n"
	"} bind def\n";

/* pagewidth and pageheight are in tenths of a millimetre */
void docheader(char *title, int pagewidth, int pageheight, int linewidth)
{
	ps_height = pageheight * dev_res / 254;
	out("%%!PS-Adobe-2.0\n");
	out("%%%%Version: 1.0\n");
	if (title)
		out("%%%%Title: %s\n", title);
	out("%%%%Creator: Neatroff\n");
	out("%%%%DocumentFonts: (atend)\n");
	out("%%%%Pages: (atend)\n");
	out("%%%%EndComments\n");

	out("%%%%BeginProlog\n");
	out("/resolution %d def\n", dev_res);
	out("/pagesize [%d %d] def\n", (pagewidth * 72 + 127) / 254,
					(pageheight * 72 + 127) / 254);
	out("/linewidth %d.%02d def\n\n", linewidth / 100, linewidth % 100);
	out("%s", prolog);
	out("%%%%EndProlog\n");
	out("%%%%BeginSetup\n");
	out("<< /PageSize pagesize /ImagingBBox null >> setpagedevice\n");
	out("mark\n");
	out("setup\n");
	out("%%%%EndSetup\n");
}
