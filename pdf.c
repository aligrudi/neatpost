#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "post.h"

static char *pdf_title;		/* document title */
static int pdf_width;		/* page width */
static int pdf_height;		/* page height */
static int pdf_pages;		/* pages object id */
static int pdf_root;		/* root object id */
static int pdf_pos;		/* current pdf file offset */
static int *obj_off;		/* object offsets */
static int obj_sz, obj_n;	/* number of pdf objects */
static int *page_id;		/* page object ids */
static int page_sz, page_n;	/* number of pages */
static char **font_ps;		/* document font names */
static int *font_id;		/* font object */
static int *font_ct;		/* font content stream object */
static int *font_ix;		/* font index */
static int font_sz, font_n;	/* number of fonts */

static struct sbuf *pg;		/* current page contents */
static int o_f, o_s, o_m;	/* font and size */
static int o_h, o_v;		/* current user position */
static int p_h, p_v;		/* current output position */
static int o_pf, p_pf;		/* output and pdf fonts; indices into o_fs[] */
static int p_f, p_s, p_m;	/* output font */
static int o_queued;		/* queued character type */
static int *o_fs;		/* page fonts */
static int o_fsn, o_fssz;	/* page fonts */

/* print pdf output */
static void pdfout(char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	pdf_pos += vprintf(s, ap);
	va_end(ap);
}

/* allocate an object number */
static int obj_map(void)
{
	if (obj_n == obj_sz) {
		obj_sz += 1024;
		obj_off = mextend(obj_off, obj_n, obj_sz, sizeof(obj_off[0]));
	}
	return obj_n++;
}

/* start the definition of an object */
static int obj_beg(int id)
{
	if (id <= 0)
		id = obj_map();
	obj_off[id] = pdf_pos;
	pdfout("%d 0 obj\n", id);
	return id;
}

/* end an object definition */
static void obj_end(void)
{
	pdfout("endobj\n\n");
}

/* embed font; return stream object identifier */
static int font_outdat(char *path, char *name, int ix)
{
	struct sbuf *sb;
	FILE *fp;
	int c, i, id;
	for (i = 0; i < font_n; i++)
		if (!strcmp(name, font_ps[i]) && font_ct[i] >= 0)
			return font_ct[i];
	fp = fopen(path, "r");
	if (!fp)
		return -1;
	sb = sbuf_make();
	c = fgetc(fp);
	for (i = 0; c != EOF; i++) {
		sbuf_printf(sb, "%02x", c);
		c = fgetc(fp);
		if (i % 40 == 39 && c != EOF)
			sbuf_chr(sb, '\n');
	}
	sbuf_str(sb, ">\n");
	fclose(fp);
	id = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Filter /ASCIIHexDecode\n");
	pdfout("  /Length %d\n", sbuf_len(sb));
	pdfout("  /Length1 %d\n", i);
	pdfout(">>\n");
	pdfout("stream\n");
	pdfout("%s", sbuf_buf(sb));
	pdfout("endstream\n");
	obj_end();
	sbuf_free(sb);
	return id;
}

/* write the object corresponding to font font_id[f] */
static void font_out(struct font *fn, int f)
{
	int i;
	int enc_obj, des_obj;
	char *path = font_path(fn);
	char *ext = path ? strrchr(path, '.') : NULL;
	/* the encoding object */
	enc_obj = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /Encoding\n");
	pdfout("  /Differences [ 0");
	for (i = 0; i < 256; i++) {
		struct glyph *g = font_glget(fn, font_ix[f] * 256 + i);
		pdfout(" /%s", g ? g->id : ".notdef");
	}
	pdfout(" ]\n");
	pdfout(">>\n");
	obj_end();
	/* embedding the font */
	if (ext && !strcmp(".ttf", ext))
		font_ct[f] = font_outdat(path, font_ps[f], font_ix[f]);
	/* the font descriptor */
	des_obj = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /FontDescriptor\n");
	pdfout("  /FontName /%s\n", font_ps[f]);
	pdfout("  /Flags 0\n");
	pdfout("  /MissingWidth 255\n");
	pdfout("  /StemV 100\n");
	pdfout("  /StemH 100\n");
	pdfout("  /CapHeight 100\n");
	pdfout("  /Ascent 100\n");
	pdfout("  /Descent 100\n");
	if (font_ct[f] >= 0)
		pdfout("  /FontFile2 %d 0 R\n", font_ct[f]);
	pdfout(">>\n");
	obj_end();
	/* the font object */
	obj_beg(font_id[f]);
	pdfout("<<\n");
	pdfout("  /Type /Font\n");
	pdfout("  /Subtype /%s\n",
		ext && !strcmp(".ttf", ext) ? "TrueType" : "Type1");
	pdfout("  /BaseFont /%s\n", font_ps[f]);
	pdfout("  /FirstChar 0\n");
	pdfout("  /LastChar 255\n");
	pdfout("  /Widths [");
	for (i = 0; i < 256; i++) {
		struct glyph *g = font_glget(fn, font_ix[f] * 256 + i);
		pdfout(" %d", (g ? g->wid : 0) * dev_res / 72);
	}
	pdfout(" ]\n");
	pdfout("  /FontDescriptor %d 0 R\n", des_obj);
	pdfout("  /Encoding %d 0 R\n", enc_obj);
	pdfout(">>\n");
	obj_end();
}

static int font_put(struct font *fn, int ix)
{
	int i;
	char *name = font_name(fn);
	for (i = 0; i < font_n; i++)
		if (!strcmp(font_ps[i], font_name(fn)) && font_ix[i] == ix)
			return i;
	if (font_n == font_sz) {
		font_sz += 128;
		font_id = mextend(font_id, font_n, font_sz, sizeof(font_id[0]));
		font_ps = mextend(font_ps, font_n, font_sz, sizeof(font_ps[0]));
		font_ix = mextend(font_ix, font_n, font_sz, sizeof(font_ix[0]));
		font_ct = mextend(font_ct, font_n, font_sz, sizeof(font_ct[0]));
	}
	font_id[font_n] = obj_map();
	font_ix[font_n] = ix;
	font_ps[font_n] = malloc(strlen(name) + 1);
	font_ct[font_n] = -1;
	strcpy(font_ps[font_n], name);
	font_n++;
	font_out(fn, font_n - 1);
	return font_n - 1;
}

void out(char *s, ...)
{
}

static void o_flush(void)
{
	if (o_queued == 1)
		sbuf_printf(pg, ") Tj\n");
	o_queued = 0;
}

static int o_loadfont(struct glyph *g)
{
	struct font *fn = g ? g->font : dev_font(o_f);
	int ix = font_glnum(fn, g) / 256;
	char *name = font_name(fn);
	int i;
	int id;
	for (i = 0; i < o_fsn; i++)
		if (!strcmp(name, font_ps[o_fs[i]]) && font_ix[o_fs[i]] == ix)
			return i;
	id = font_put(fn, ix);
	if (o_fsn == o_fssz) {
		o_fssz += 128;
		o_fs = mextend(o_fs, o_fsn, o_fssz, sizeof(o_fs[0]));
	}
	o_fs[o_fsn++] = id;
	return o_fsn - 1;
}

#define PREC		1000
#define PRECN		"3"

/* convert troff position to pdf position; returns a static buffer */
static char *pdfpos(int uh, int uv)
{
	static char buf[64];
	int h = uh * PREC * 72 / dev_res;
	int v = pdf_height * PREC - (uv * PREC * 72 / dev_res);
	sprintf(buf, "%d.%0" PRECN "d %d.%0" PRECN "d",
		h / PREC, h % PREC, v / PREC, v % PREC);
	return buf;
}

/* convert troff color to pdf color; returns a static buffer */
static char *pdfcolor(int m)
{
	static char buf[64];
	int r = CLR_R(m) * 1000 / 255;
	int g = CLR_G(m) * 1000 / 255;
	int b = CLR_B(m) * 1000 / 255;
	sbuf_printf(pg, "%d.%03d %d.%03d %d.%03d",
		r / 1000, r % 1000, g / 1000, g % 1000, b / 1000, b % 1000);
	return buf;
}

static void o_queue(struct glyph *g)
{
	int pos;
	if (o_h != p_h || o_v != p_v) {
		o_flush();
		sbuf_printf(pg, "1 0 0 1 %s Tm\n", pdfpos(o_h, o_v));
		p_h = o_h;
		p_v = o_v;
	}
	if (!o_queued)
		sbuf_printf(pg, "(");
	o_queued = 1;
	pos = font_glnum(g->font, g) % 256;
	sbuf_printf(pg, "\\%d%d%d", (pos >> 6) & 7, (pos >> 3) & 7, pos & 7);
	p_h += font_wid(g->font, o_s, g->wid);
}

static void out_fontup(void)
{
	if (o_m != p_m) {
		o_flush();
		sbuf_printf(pg, "%s rg\n", pdfcolor(o_m));
		p_m = o_m;
	}
	if (o_pf != p_pf || o_s != p_s) {
		int f = o_fs[o_pf];
		o_flush();
		sbuf_printf(pg, "/%s.%d %d Tf\n", font_ps[f], font_ix[f], o_s);
		p_pf = o_pf;
		p_s = o_s;
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
	o_pf = o_loadfont(g);
	out_fontup();
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

void outsize(int s)
{
	if (s > 0)
		o_s = s;
}

void outcolor(int c)
{
	o_m = c;
}

void outrotate(int deg)
{
}

void outeps(char *eps)
{
}

void outlink(char *spec)
{
}

void outpage(void)
{
	o_v = 0;
	o_h = 0;
	p_v = 0;
	p_h = 0;
	p_s = 0;
	p_f = 0;
	p_m = 0;
}

void outmnt(int f)
{
	if (p_f == f)
		p_f = -1;
}

void outgname(int g)
{
}

static int draw_path;	/* number of path segments */
static int draw_point;	/* point was set for postscript newpath */

static void drawmv(void)
{
	if (!draw_point)
		sbuf_printf(pg, "%d %d m ", o_h, o_v);
	draw_point = 1;
}

void drawbeg(void)
{
	o_flush();
	out_fontup();
	if (draw_path)
		return;
	sbuf_printf(pg, "%s m\n", pdfpos(o_h, o_v));
}

void drawend(int close, int fill)
{
	if (draw_path)
		return;
	draw_point = 0;
	if (!fill)		/* stroking color */
		sbuf_printf(pg, "%s RG\n", pdfcolor(o_m));
	if (fill)
		sbuf_printf(pg, "f\n");
	else
		sbuf_printf(pg, close ? "s\n" : "S\n");
}

void drawmbeg(char *s)
{
}

void drawmend(char *s)
{
}

void drawl(int h, int v)
{
	o_flush();
	outrel(h, v);
	sbuf_printf(pg, "%s l\n", pdfpos(o_h, o_v));
}

void drawc(int c)
{
	outrel(c, 0);
}

void drawe(int h, int v)
{
	outrel(h, 0);
}

void drawa(int h1, int v1, int h2, int v2)
{
	outrel(h1 + h2, v1 + v2);
}

void draws(int h1, int v1, int h2, int v2)
{
	outrel(h1, v1);
}

void ps_header(char *title, int pagewidth, int pageheight, int linewidth)
{
	pdf_title = title;
	obj_map();
	pdf_root = obj_map();
	pdf_pages = obj_map();
	pdf_title = title;
	pdfout("%%PDF-1.6\n");
	pdf_width = (pagewidth * 72 + 127) / 254;
	pdf_height = (pageheight * 72 + 127) / 254;
}

void ps_trailer(int pages)
{
	int i;
	int xref_off;
	int info_id;
	/* pdf pages object */
	obj_beg(pdf_pages);
	pdfout("<<\n");
	pdfout("  /Type /Pages\n");
	pdfout("  /MediaBox [ 0 0 %d %d ]\n", pdf_width, pdf_height);
	pdfout("  /Count %d\n", page_n);
	pdfout("  /Kids [");
	for (i = 0; i < page_n; i++)
		pdfout(" %d 0 R", page_id[i]);
	pdfout(" ]\n");
	pdfout(">>\n");
	obj_end();
	/* pdf root object */
	obj_beg(pdf_root);
	pdfout("<<\n");
	pdfout("  /Type /Catalog\n");
	pdfout("  /Pages %d 0 R\n", pdf_pages);
	pdfout(">>\n");
	obj_end();
	/* info object */
	info_id = obj_beg(0);
	pdfout("<<\n");
	if (pdf_title)
		pdfout("  /Title (%s)\n", pdf_title);
	pdfout("  /Creator (Neatroff)\n");
	pdfout("  /Producer (Neatpost)\n");
	pdfout(">>\n");
	obj_end();
	/* the xref */
	xref_off = pdf_pos;
	pdfout("xref\n");
	pdfout("0 %d\n", obj_n);
	pdfout("0000000000 65535 f \n");
	for (i = 1; i < obj_n; i++)
		pdfout("%010d 00000 n \n", obj_off[i]);
	/* the trailer */
	pdfout("trailer\n");
	pdfout("<<\n");
	pdfout("  /Size %d\n", obj_n);
	pdfout("  /Root %d 0 R\n", pdf_root);
	pdfout("  /Info %d 0 R\n", info_id);
	pdfout(">>\n");
	pdfout("startxref\n");
	pdfout("%d\n", xref_off);
	pdfout("%%%%EOF\n");
	free(page_id);
	free(obj_off);
	for (i = 0; i < font_n; i++)
		free(font_ps[i]);
	free(font_ps);
	free(font_ct);
	free(font_id);
	free(font_ix);
}

void ps_pagebeg(int n)
{
	pg = sbuf_make();
	sbuf_printf(pg, "BT\n");
}

void ps_pageend(int n)
{
	int cont_id;
	int i;
	o_flush();
	sbuf_printf(pg, "ET\n");
	/* page contents */
	cont_id = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Length %d\n", sbuf_len(pg));
	pdfout(">>\n");
	pdfout("stream\n");
	pdfout("%s", sbuf_buf(pg));
	pdfout("endstream\n");
	obj_end();
	/* the page object */
	if (page_n == page_sz) {
		page_sz += 1024;
		page_id = mextend(page_id, page_n, page_sz, sizeof(page_id[0]));
	}
	page_id[page_n++] = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /Page\n");
	pdfout("  /Parent %d 0 R\n", pdf_pages);
	pdfout("  /Resources <<\n");
	pdfout("    /Font <<");
	for (i = 0; i < o_fsn; i++)
		pdfout(" /%s.%d %d 0 R",
			font_ps[o_fs[i]], font_ix[o_fs[i]], font_id[o_fs[i]]);
	pdfout(" >>\n");
	pdfout("  >>\n");
	pdfout("  /Contents %d 0 R\n", cont_id);
	pdfout(">>\n");
	obj_end();
	sbuf_free(pg);
	free(o_fs);
	o_fs = NULL;
	o_fsn = 0;
	o_fssz = 0;
}
