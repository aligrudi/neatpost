/* PDF post-processor functions */
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "post.h"

static char pdf_title[256];	/* document title */
static char pdf_author[256];	/* document author */
static int pdf_width;		/* page width */
static int pdf_height;		/* page height */
static int pdf_linewid;		/* line width in thousands of ems */
static int pdf_linecap = 1;	/* line cap style: 0 (butt), 1 (round), 2 (projecting square) */
static int pdf_linejoin = 1;	/* line join style: 0 (miter), 1 (round), 2 (bevel) */
static int pdf_pages;		/* pages object id */
static int pdf_root;		/* root object id */
static int pdf_pos;		/* current pdf file offset */
static int *obj_off;		/* object offsets */
static int obj_sz, obj_n;	/* number of pdf objects */
static int *page_id;		/* page object ids */
static int page_sz, page_n;	/* number of pages */
static int pdf_outline;		/* pdf outline hierarchiy */
static int pdf_dests;		/* named destinations */

static struct sbuf *pg;		/* current page contents */
static int o_f, o_s, o_m;	/* font and size */
static int o_h, o_v;		/* current user position */
static int p_h, p_v;		/* current output position */
static int o_i, p_i;		/* output and pdf fonts (indices into pfont[]) */
static int p_f, p_s, p_m;	/* output font */
static int o_queued;		/* queued character type */
static char o_iset[1024];	/* fonts accesssed in this page */
static int o_rh, o_rv;
static double o_rdeg;		/* previous rotation position and degree */
static int xobj[128];		/* page xobject object ids */
static int xobj_n;		/* number of xobjects in this page */
static int ann[128];		/* page annotations */
static int ann_n;		/* number of annotations in this page */

/* loaded PDF fonts */
struct pfont {
	char name[128];		/* font PostScript name */
	char path[1024];	/* font path */
	char desc[1024];	/* font descriptor path */
	int gbeg;		/* the first glyph */
	int gend;		/* the last glyph */
	int sub;		/* subfont number */
	int obj;		/* the font object */
	int des;		/* font descriptor */
	int cid;		/* CID-indexed */
};

static struct pfont *pfonts;
static int pfonts_n, pfonts_sz;

/* print formatted pdf output */
static void pdfout(char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	pdf_pos += vprintf(s, ap);
	va_end(ap);
}

/* print pdf output */
static void pdfmem(char *s, int len)
{
	fwrite(s, len, 1, stdout);
	pdf_pos += len;
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

void out(char *s, ...)
{
}

/* the length of the clear-text, encrypted, and fixed-content portions */
static int type1lengths(char *t1, int l, int *l1, int *l2, int *l3)
{
	int i;
	char *cleartext = t1;
	char *encrypted = NULL;
	char *fixedcont = NULL;
	for (i = 0; i < l - 5 && !encrypted; i++)
		if (t1[i] == 'e' && !memcmp("eexec", t1 + i, 5))
			encrypted = t1 + i;
	if (!encrypted)
		return 1;
	for (; i < l - 512 && !fixedcont; i++)
		if (t1[i] == '0' && !memcmp("00000", t1 + i, 5))
			fixedcont = t1 + i;
	*l1 = encrypted - cleartext;
	*l2 = fixedcont ? fixedcont - cleartext : 0;
	*l3 = fixedcont ? t1 + l - fixedcont : 0;
	return 0;
}

/* return font type: 't': TrueType, '1': Type 1, 'o': OpenType */
static int fonttype(char *path)
{
	char *ext = strrchr(path, '.');
	if (ext && !strcmp(".ttf", ext))
		return 't';
	if (ext && !strcmp(".otf", ext))
		return 't';
	if (ext && (!strcmp(".ttc", ext) || !strcmp(".otc", ext)))
		return 't';
	return '1';
}

/* write the object corresponding to the given font */
static void pfont_write(struct pfont *ps)
{
	int i;
	int enc_obj;
	struct font *fn = dev_fontopen(ps->desc);
	/* the encoding object */
	enc_obj = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /Encoding\n");
	pdfout("  /Differences [ %d", ps->gbeg % 256);
	for (i = ps->gbeg; i <= ps->gend; i++)
		pdfout(" /%s", font_glget(fn, i)->id);
	pdfout(" ]\n");
	pdfout(">>\n");
	obj_end();
	/* the font object */
	obj_beg(ps->obj);
	pdfout("<<\n");
	pdfout("  /Type /Font\n");
	if (fonttype(ps->path) == 't')
		pdfout("  /Subtype /TrueType\n");
	else
		pdfout("  /Subtype /Type1\n");
	pdfout("  /BaseFont /%s\n", ps->name);
	pdfout("  /FirstChar %d\n", ps->gbeg % 256);
	pdfout("  /LastChar %d\n", ps->gend % 256);
	pdfout("  /Widths [");
	for (i = ps->gbeg; i <= ps->gend; i++)
		pdfout(" %d", (long) font_glget(fn, i)->wid * 100 * 72 / dev_res);
	pdfout(" ]\n");
	pdfout("  /FontDescriptor %d 0 R\n", ps->des);
	pdfout("  /Encoding %d 0 R\n", enc_obj);
	pdfout(">>\n");
	obj_end();
	font_close(fn);
}

static void encodehex(struct sbuf *d, char *s, int n)
{
	static char hex[] = "0123456789ABCDEF";
	int i;
	for (i = 0; i < n; i++) {
		sbuf_chr(d, hex[((unsigned char) s[i]) >> 4]);
		sbuf_chr(d, hex[((unsigned char) s[i]) & 0x0f]);
		if (i % 40 == 39 && i + 1 < n)
			sbuf_chr(d, '\n');
	}
	sbuf_str(d, ">\n");
}

/* write the object corresponding to this CID font */
static void pfont_writecid(struct pfont *ps)
{
	int cid_obj;
	struct font *fn = dev_fontopen(ps->desc);
	int gcnt = 0;
	int i;
	/* CIDFont */
	cid_obj = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /Font\n");
	pdfout("  /Subtype /CIDFontType2\n");
	pdfout("  /BaseFont /%s\n", ps->name);
	pdfout("  /CIDSystemInfo <</Ordering(Identity)/Registry(Adobe)/Supplement 0>>\n");
	pdfout("  /FontDescriptor %d 0 R\n", ps->des);
	pdfout("  /DW 1000\n");
	while (font_glget(fn, gcnt))
		gcnt++;
	pdfout("  /W [ %d [", ps->gbeg);
	for (i = ps->gbeg; i <= ps->gend; i++)
		pdfout(" %d", (long) font_glget(fn, i)->wid * 100 * 72 / dev_res);
	pdfout(" ] ]\n");
	pdfout(">>\n");
	obj_end();
	/* the font object */
	obj_beg(ps->obj);
	pdfout("<<\n");
	pdfout("  /Type /Font\n");
	pdfout("  /Subtype /Type0\n");
	pdfout("  /BaseFont /%s\n", ps->name);
	pdfout("  /Encoding /Identity-H\n");
	pdfout("  /DescendantFonts [%d 0 R]\n", cid_obj);
	pdfout(">>\n");
	obj_end();
	font_close(fn);
}

/* write font descriptor; returns its object ID */
static int writedesc(struct font *fn)
{
	int str_obj = -1;
	int des_obj;
	char buf[1 << 10];
	int fntype = fonttype(font_path(fn));
	if (fntype == '1' || fntype == 't') {
		int fd = open(font_path(fn), O_RDONLY);
		struct sbuf *ffsb = sbuf_make();
		struct sbuf *sb = sbuf_make();
		int l1 = 0, l2 = 0, l3 = 0;
		int nr;
		/* reading the font file */
		while ((nr = read(fd, buf, sizeof(buf))) > 0)
			sbuf_mem(ffsb, buf, nr);
		close(fd);
		l1 = sbuf_len(ffsb);
		/* initialize Type 1 lengths */
		if (fntype == '1') {
			if (type1lengths(sbuf_buf(ffsb), sbuf_len(ffsb),
					&l1, &l2, &l3))
				l1 = 0;
			/* remove the fixed-content portion of the font */
			if (l3)
				sbuf_cut(ffsb, l1 + l2);
			l1 -= l3;
			l3 = 0;
		}
		/* encoding file contents */
		encodehex(sb, sbuf_buf(ffsb), sbuf_len(ffsb));
		/* write font data if it has nonzero length */
		if (l1) {
			str_obj = obj_beg(0);
			pdfout("<<\n");
			pdfout("  /Filter /ASCIIHexDecode\n");
			pdfout("  /Length %d\n", sbuf_len(sb));
			pdfout("  /Length1 %d\n", l1);
			if (fntype == '1')
				pdfout("  /Length2 %d\n", l2);
			if (fntype == '1')
				pdfout("  /Length3 %d\n", l3);
			pdfout(">>\n");
			pdfout("stream\n");
			pdfmem(sbuf_buf(sb), sbuf_len(sb));
			pdfout("endstream\n");
			obj_end();
		}
		sbuf_free(ffsb);
		sbuf_free(sb);
	}
	/* the font descriptor */
	des_obj = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /FontDescriptor\n");
	pdfout("  /FontName /%s\n", font_name(fn));
	pdfout("  /Flags 32\n");
	pdfout("  /FontBBox [-1000 -1000 1000 1000]\n");
	pdfout("  /MissingWidth 1000\n");
	pdfout("  /StemV 100\n");
	pdfout("  /ItalicAngle 0\n");
	pdfout("  /CapHeight 100\n");
	pdfout("  /Ascent 100\n");
	pdfout("  /Descent 100\n");
	if (str_obj >= 0)
		pdfout("  /FontFile%s %d 0 R\n",
			fntype == 't' ? "2" : "", str_obj);
	pdfout(">>\n");
	obj_end();
	return des_obj;
}

static int pfont_find(struct glyph *g)
{
	struct font *fn = g->font;
	char *name = font_name(fn);
	struct pfont *ps = NULL;
	int fntype = fonttype(font_path(fn));
	int sub = fntype == '1' ? font_glnum(fn, g) / 256 : 0;
	int i;
	for (i = 0; i < pfonts_n; i++)
		if (!strcmp(name, pfonts[i].name) && pfonts[i].sub == sub)
			return i;
	if (pfonts_n == pfonts_sz) {
		pfonts_sz += 16;
		pfonts = mextend(pfonts, pfonts_n,
				pfonts_sz, sizeof(pfonts[0]));
	}
	ps = &pfonts[pfonts_n];
	snprintf(ps->name, sizeof(ps->name), "%s", name);
	snprintf(ps->path, sizeof(ps->path), "%s", font_path(fn));
	snprintf(ps->desc, sizeof(ps->desc), "%s", font_desc(fn));
	ps->cid = fntype == 't';
	ps->obj = obj_map();
	ps->sub = sub;
	ps->gbeg = 1 << 20;
	for (i = 0; i < pfonts_n; i++)
		if (!strcmp(pfonts[i].name, ps->name))
			break;
	if (i < pfonts_n)
		ps->des = pfonts[i].des;
	else
		ps->des = writedesc(fn);
	return pfonts_n++;
}

static void pfont_done(void)
{
	int i;
	for (i = 0; i < pfonts_n; i++) {
		if (pfonts[i].cid)
			pfont_writecid(&pfonts[i]);
		else
			pfont_write(&pfonts[i]);
	}
	free(pfonts);
}

static void o_flush(void)
{
	if (o_queued == 1)
		sbuf_printf(pg, ">] TJ\n");
	o_queued = 0;
}

static int o_loadfont(struct glyph *g)
{
	int fn = pfont_find(g);
	o_iset[fn] = 1;
	return fn;
}

/* like pdfpos() but assume that uh and uv are multiplied by 100 */
static char *pdfpos00(int uh, int uv)
{
	static char buf[64];
	int h = (long) uh * 72 / dev_res;
	int v = (long) pdf_height * 100 - (long) uv * 72 / dev_res;
	sprintf(buf, "%s%d.%02d %s%d.%02d",
		h < 0 ? "-" : "", abs(h) / 100, abs(h) % 100,
		v < 0 ? "-" : "", abs(v) / 100, abs(v) % 100);
	return buf;
}

/* convert troff position to pdf position; returns a static buffer */
static char *pdfpos(int uh, int uv)
{
	return pdfpos00(uh * 100, uv * 100);
}

/* troff length to thousands of a unit of text space; returns a static buffer */
static char *pdfunit(int uh, int sz)
{
	static char buf[64];
	int h = (long) uh * 1000 * 72 / sz / dev_res;
	sprintf(buf, "%s%d", h < 0 ? "-" : "", abs(h));
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
	int gid;
	if (o_v != p_v) {
		o_flush();
		sbuf_printf(pg, "1 0 0 1 %s Tm\n", pdfpos(o_h, o_v));
		p_h = o_h;
		p_v = o_v;
	}
	if (!o_queued)
		sbuf_printf(pg, "[<");
	o_queued = 1;
	if (o_h != p_h)
		sbuf_printf(pg, "> %s <", pdfunit(p_h - o_h, o_s));
	/* printing glyph identifier */
	gid = font_glnum(g->font, g);
	if (pfonts[o_i].cid)
		sbuf_printf(pg, "%04x", gid);
	else
		sbuf_printf(pg, "%02x", gid % 256);
	/* updating gbeg and gend */
	if (gid < pfonts[o_i].gbeg)
		pfonts[o_i].gbeg = gid;
	if (gid > pfonts[o_i].gend)
		pfonts[o_i].gend = gid;
	/* advancing */
	p_h = o_h + font_wid(g->font, o_s, g->wid);
}

static void out_fontup(void)
{
	if (o_m != p_m) {
		o_flush();
		sbuf_printf(pg, "%s rg\n", pdfcolor(o_m));
		p_m = o_m;
	}
	if (o_i >= 0 && (o_i != p_i || o_s != p_s)) {
		struct pfont *ps = &pfonts[o_i];
		o_flush();
		if (ps->cid)
			sbuf_printf(pg, "/%s %d Tf\n", ps->name, o_s);
		else
			sbuf_printf(pg, "/%s.%d %d Tf\n", ps->name, ps->sub, o_s);
		p_i = o_i;
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
	o_i = o_loadfont(g);
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

void outrotate(double deg)
{
	o_flush();
	out_fontup();

	double x, y;
	if (o_rdeg) {
		x = cos(-o_rdeg*M_PI/180);
		y = sin(-o_rdeg*M_PI/180);
		sbuf_printf(pg, "1 0 0 1 %d %d cm\n", o_rh*72/dev_res, pdf_height-o_rv*72/dev_res);
		sbuf_printf(pg, "%f %f %f %f 0 0 cm\n", x, y, -y, x);
		sbuf_printf(pg, "1 0 0 1 %d %d cm\n", -o_rh*72/dev_res, -pdf_height+o_rv*72/dev_res);
	}
	o_rdeg = deg;
	o_rh = o_h;
	o_rv = o_v;
	x = cos(o_rdeg*M_PI/180);
	y = sin(o_rdeg*M_PI/180);
	sbuf_printf(pg, "1 0 0 1 %d %d cm\n", o_rh*72/dev_res, pdf_height-o_rv*72/dev_res);
	sbuf_printf(pg, "%f %f %f %f 0 0 cm\n", x, y, -y, x);
	sbuf_printf(pg, "1 0 0 1 %d %d cm\n", -o_rh*72/dev_res, -pdf_height+o_rv*72/dev_res);
}

void outeps(char *eps, int hwid, int vwid)
{
}

/* return a copy of a PDF object; returns a static buffer */
static char *pdf_copy(char *pdf, int len, int pos)
{
	static char buf[1 << 12];
	int datlen;
	pos += pdf_ws(pdf, len, pos);
	datlen = pdf_len(pdf, len, pos);
	if (datlen > sizeof(buf) - 1)
		datlen = sizeof(buf) - 1;
	memcpy(buf, pdf + pos, datlen);
	buf[datlen] = '\0';
	return buf;
}

static void pdf_dictcopy(char *pdf, int len, int pos, struct sbuf *sb);

/* write stream to sb */
static int pdf_strcopy(char *pdf, int len, int pos, struct sbuf *sb)
{
	int slen, val;
	int beg;
	if ((val = pdf_dval_val(pdf, len, pos, "/Length")) < 0)
		return -1;
	slen = atoi(pdf + val);
	pos = pos + pdf_len(pdf, len, pos);
	pos += pdf_ws(pdf, len, pos);
	if (pos + slen + 15 > len)
		return -1;
	beg = pos;
	pos += strlen("stream");
	if (pdf[pos] == '\r')
		pos++;
	pos += 1 + slen;
	if (pdf[pos] == '\r' || pdf[pos] == ' ')
		pos++;
	if (pdf[pos] == '\n')
		pos++;
	pos += strlen("endstream") + 1;
	sbuf_mem(sb, pdf + beg, pos - beg);
	return 0;
}

/* copy a PDF object and return its new identifier */
static int pdf_objcopy(char *pdf, int len, int pos)
{
	int id;
	if ((pos = pdf_ref(pdf, len, pos)) < 0)
		return -1;
	if (pdf_type(pdf, len, pos) == 'd') {
		struct sbuf *sb = sbuf_make();
		pdf_dictcopy(pdf, len, pos, sb);
		sbuf_chr(sb, '\n');
		if (pdf_dval(pdf, len, pos, "/Length") >= 0)
			pdf_strcopy(pdf, len, pos, sb);
		id = obj_beg(0);
		pdfmem(sbuf_buf(sb), sbuf_len(sb));
		obj_end();
		sbuf_free(sb);
	} else {
		id = obj_beg(0);
		pdfmem(pdf + pos, pdf_len(pdf, len, pos));
		pdfout("\n");
		obj_end();
	}
	return id;
}

/* copy a PDF dictionary recursively */
static void pdf_dictcopy(char *pdf, int len, int pos, struct sbuf *sb)
{
	int i;
	int key, val, id;
	sbuf_printf(sb, "<<");
	for (i = 0; ; i++) {
		if ((key = pdf_dkey(pdf, len, pos, i)) < 0)
			break;
		sbuf_printf(sb, " %s", pdf_copy(pdf, len, key));
		val = pdf_dval(pdf, len, pos, pdf_copy(pdf, len, key));
		if (pdf_type(pdf, len, val) == 'r') {
			if ((id = pdf_objcopy(pdf, len, val)) >= 0)
				sbuf_printf(sb, " %d 0 R", id);
		} else {
			sbuf_printf(sb, " %s", pdf_copy(pdf, len, val));
		}
	}
	sbuf_printf(sb, " >>");
}

/* copy resources dictionary */
static void pdf_rescopy(char *pdf, int len, int pos, struct sbuf *sb)
{
	char *res_fields[] = {"/ProcSet", "/ExtGState", "/ColorSpace",
		"/Pattern", "/Shading", "/Properties", "/Font", "/XObject"};
	int res, i;
	sbuf_printf(sb, "  /Resources <<\n");
	for (i = 0; i < LEN(res_fields); i++) {
		if ((res = pdf_dval_val(pdf, len, pos, res_fields[i])) >= 0) {
			if (pdf_type(pdf, len, res) == 'd') {
				sbuf_printf(sb, "    %s ", res_fields[i]);
				pdf_dictcopy(pdf, len, res, sb);
				sbuf_printf(sb, "\n");
			} else {
				sbuf_printf(sb, "    %s %s\n", res_fields[i],
					pdf_copy(pdf, len, res));
			}
		}
	}
	sbuf_printf(sb, "  >>\n");
}

static int pdfbbox100(char *pdf, int len, int pos, int dim[4])
{
	int val;
	int i;
	for (i = 0; i < 4; i++) {
		int n = 0, f1 = 0, f2 = 0;
		if ((val = pdf_lval(pdf, len, pos, i)) < 0)
			return -1;
		for (; isdigit((unsigned char) pdf[val]); val++)
			n = n * 10 + pdf[val] - '0';
		if (pdf[val] == '.') {
			if (isdigit((unsigned char) pdf[val + 1])) {
				f1 = pdf[val + 1] - '0';
				if (isdigit((unsigned char) pdf[val + 2]))
					f2 = pdf[val + 2] - '0';
			}
		}
		dim[i] = n * 100 + f1 * 10 + f2;
	}
	return 0;
}

static int pdfext(char *pdf, int len, int hwid, int vwid)
{
	char *cont_fields[] = {"/Filter", "/DecodeParms"};
	int trailer, root, cont, pages, page1, res;
	int kids_val, page1_val, val, bbox;
	int xobj_id, length;
	int dim[4];
	int hzoom = 100, vzoom = 100;
	struct sbuf *sb;
	int i;
	if (xobj_n == LEN(xobj))
		return -1;
	if ((trailer = pdf_trailer(pdf, len)) < 0)
		return -1;
	if ((root = pdf_dval_obj(pdf, len, trailer, "/Root")) < 0)
		return -1;
	if ((pages = pdf_dval_obj(pdf, len, root, "/Pages")) < 0)
		return -1;
	if ((kids_val = pdf_dval_val(pdf, len, pages, "/Kids")) < 0)
		return -1;
	if ((page1_val = pdf_lval(pdf, len, kids_val, 0)) < 0)
		return -1;
	if ((page1 = pdf_ref(pdf, len, page1_val)) < 0)
		return -1;
	if ((cont = pdf_dval_obj(pdf, len, page1, "/Contents")) < 0)
		return -1;
	if ((val = pdf_dval_val(pdf, len, cont, "/Length")) < 0)
		return -1;
	res = pdf_dval_val(pdf, len, page1, "/Resources");
	length = atoi(pdf + val);
	bbox = pdf_dval_val(pdf, len, page1, "/MediaBox");
	if (bbox < 0)
		bbox = pdf_dval_val(pdf, len, pages, "/MediaBox");
	if (bbox >= 0 && !pdfbbox100(pdf, len, bbox, dim)) {
		if (hwid > 0)
			hzoom = (long) hwid * (100 * 7200 / dev_res) / (dim[2] - dim[0]);
		if (vwid > 0)
			vzoom = (long) vwid * (100 * 7200 / dev_res) / (dim[3] - dim[1]);
		if (vwid <= 0)
			vzoom = hzoom;
		if (hwid <= 0)
			hzoom = vzoom;
	}
	sb = sbuf_make();
	sbuf_printf(sb, "<<\n");
	sbuf_printf(sb, "  /Type /XObject\n");
	sbuf_printf(sb, "  /Subtype /Form\n");
	sbuf_printf(sb, "  /FormType 1\n");
	if (bbox >= 0)
		sbuf_printf(sb, "  /BBox %s\n", pdf_copy(pdf, len, bbox));
	sbuf_printf(sb, "  /Matrix [%d.%02d 0 0 %d.%02d %s]\n",
		hzoom / 100, hzoom % 100, vzoom / 100, vzoom % 100,
		pdfpos(o_h, o_v));
	if (res >= 0)
		pdf_rescopy(pdf, len, res, sb);
	sbuf_printf(sb, "  /Length %d\n", length);
	for (i = 0; i < LEN(cont_fields); i++)
		if ((val = pdf_dval_val(pdf, len, cont, cont_fields[i])) >= 0)
			sbuf_printf(sb, "  %s %s\n", cont_fields[i],
				pdf_copy(pdf, len, val));
	sbuf_printf(sb, ">>\n");
	pdf_strcopy(pdf, len, cont, sb);
	xobj_id = obj_beg(0);
	pdfmem(sbuf_buf(sb), sbuf_len(sb));
	obj_end();
	sbuf_free(sb);
	xobj[xobj_n++] = xobj_id;
	return xobj_n - 1;
}

void outpdf(char *pdf, int hwid, int vwid)
{
	char buf[1 << 12];
	struct sbuf *sb;
	int xobj_id;
	int fd, nr;
	/* reading the pdf file */
	sb = sbuf_make();
	fd = open(pdf, O_RDONLY);
	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		sbuf_mem(sb, buf, nr);
	close(fd);
	/* the XObject */
	xobj_id = pdfext(sbuf_buf(sb), sbuf_len(sb), hwid, vwid);
	sbuf_free(sb);
	o_flush();
	out_fontup();
	if (xobj_id >= 0)
		sbuf_printf(pg, "ET /FO%d Do BT\n", xobj_id);
	p_h = -1;
	p_v = -1;
}

void outlink(char *lnk, int hwid, int vwid)
{
	if (ann_n == LEN(ann))
		return;
	o_flush();
	ann[ann_n++] = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Type /Annot\n");
	pdfout("  /Subtype /Link\n");
	pdfout("  /Rect [%s", pdfpos(o_h, o_v));
	pdfout(" %s]\n", pdfpos(o_h + hwid, o_v + vwid));
	if (lnk[0] == '#') {	/* internal links */
		pdfout("  /A << /S /GoTo /D (%s) >>\n", lnk + 1);
	} else {		/* external links */
		pdfout("  /A << /S /URI /URI %s >>\n", pdftext_static(lnk));
	}
	pdfout(">>\n");
	obj_end();
}

void outname(int n, char (*desc)[64], int *page, int *off)
{
	int i;
	o_flush();
	pdf_dests = obj_beg(0);
	pdfout("<<\n");
	for (i = 0; i < n; i++) {
		if (page[i] > 0 && page[i] - 1 < page_n)
			pdfout("  /%s [ %d 0 R /XYZ 0 %d 0 ]\n",
				desc[i], page_id[page[i] - 1],
				pdf_height - (off[i] * 72 / dev_res));
	}
	pdfout(">>\n");
	obj_end();
}

void outmark(int n, char (*desc)[256], int *page, int *off, int *level)
{
	int *objs = malloc(n * sizeof(objs[0]));
	int i, j;
	int cnt = 0;
	/* allocating objects */
	pdf_outline = obj_map();
	for (i = 0; i < n; i++)
		objs[i] = obj_map();
	o_flush();
	/* root object */
	obj_beg(pdf_outline);
	pdfout("<<\n");
	for (i = 0; i < n; i++)
		if (level[i] == level[0])
			cnt++;
	pdfout("  /Count %d\n", cnt);
	pdfout("  /First %d 0 R\n", objs[0]);
	for (i = n - 1; i > 0 && level[i] > level[0]; i--)
		;
	pdfout("  /Last %d 0 R\n", objs[i]);
	pdfout(">>\n");
	obj_end();
	/* other objects */
	for (i = 0; i < n; i++) {
		int cnt = 0;
		for (j = i + 1; j < n && level[j] > level[i]; j++)
			if (level[j] == level[i] + 1)
				cnt++;
		obj_beg(objs[i]);
		pdfout("<<\n");
		pdfout("  /Title %s\n", pdftext_static(desc[i]));
		/* the parent field */
		for (j = i - 1; j >= 0 && level[j] >= level[i]; j--)
			;
		pdfout("  /Parent %d 0 R\n", j >= 0 ? objs[j] : pdf_outline);
		/* the next field */
		for (j = i + 1; j < n && level[j] > level[i]; j++)
			;
		if (j < n && level[j] == level[i])
			pdfout("  /Next %d 0 R\n", objs[j]);
		/* the prev field */
		for (j = i - 1; j >= 0 && level[j] > level[i]; j--)
			;
		if (j >= 0 && level[j] == level[i])
			pdfout("  /Prev %d 0 R\n", objs[j]);
		/* node children */
		if (cnt) {
			int last = 0;
			pdfout("  /Count %d\n", cnt);
			pdfout("  /First %d 0 R\n", objs[i + 1]);
			for (j = i + 1; j < n && level[j] > level[i]; j++)
				if (level[j] == level[i] + 1)
					last = j;
			pdfout("  /Last %d 0 R\n", objs[last]);
		}
		if (page[i] > 0 && page[i] - 1 < page_n)
			pdfout("  /Dest [ %d 0 R /XYZ 0 %d 0 ]\n",
				page_id[page[i] - 1],
				pdf_height - (off[i] * 72 / dev_res));
		pdfout(">>\n");
		obj_end();
	}
	free(objs);
}

void outinfo(char *kwd, char *val)
{
	if (!strcmp("Author", kwd))
		snprintf(pdf_author, sizeof(pdf_author), "%s", val);
	if (!strcmp("Title", kwd))
		snprintf(pdf_title, sizeof(pdf_title), "%s", val);
}

void outset(char *var, char *val)
{
	if (!strcmp("linewidth", var))
		pdf_linewid = atoi(val);
	if (!strcmp("linecap", var))
		pdf_linecap = atoi(val);
	if (!strcmp("linejoin", var))
		pdf_linejoin = atoi(val);
}

void outpage(void)
{
	o_v = 0;
	o_h = 0;
	p_i = 0;
	p_v = 0;
	p_h = 0;
	p_s = 0;
	p_f = 0;
	p_m = 0;
	o_i = -1;
	o_rdeg = 0;
}

void outmnt(int f)
{
	if (p_f == f)
		p_f = -1;
}

void outgname(int g)
{
}

void drawbeg(void)
{
	o_flush();
	out_fontup();
	sbuf_printf(pg, "%s m\n", pdfpos(o_h, o_v));
}

static int l_page, l_size, l_wid, l_cap, l_join;	/* drawing line properties */

void drawend(int close, int fill)
{
	fill = !fill ? 2 : fill;
	if (l_page != page_n || l_size != o_s || l_wid != pdf_linewid ||
			l_cap != pdf_linecap || l_join != pdf_linejoin) {
		int lwid = pdf_linewid * o_s;
		sbuf_printf(pg, "%d.%03d w\n", lwid / 1000, lwid % 1000);
		sbuf_printf(pg, "%d J %d j\n", pdf_linecap, pdf_linejoin);
		l_page = page_n;
		l_size = o_s;
		l_wid = pdf_linewid;
		l_cap = pdf_linecap;
		l_join = pdf_linejoin;
	}
	if (fill & 2)				/* stroking color */
		sbuf_printf(pg, "%s RG\n", pdfcolor(o_m));
	if (fill & 1)
		sbuf_printf(pg, (fill & 2) ? "b\n" : "f\n");
	else
		sbuf_printf(pg, close ? "s\n" : "S\n");
	p_v = 0;
	p_h = 0;
}

void drawmbeg(char *s)
{
}

void drawmend(char *s)
{
}

void drawl(int h, int v)
{
	outrel(h, v);
	sbuf_printf(pg, "%s l\n", pdfpos(o_h, o_v));
}

/* draw circle/ellipse quadrant */
static void drawquad(int ch, int cv)
{
	long b = 551915;
	long x0 = o_h * 1000;
	long y0 = o_v * 1000;
	long x3 = x0 + ch * 1000 / 2;
	long y3 = y0 + cv * 1000 / 2;
	long x1 = x0;
	long y1 = y0 + cv * b / 1000 / 2;
	long x2 = x0 + ch * b / 1000 / 2;
	long y2 = y3;
	if (ch * cv < 0) {
		x1 = x3 - ch * b / 1000 / 2;
		y1 = y0;
		x2 = x3;
		y2 = y3 - cv * b / 1000 / 2;
	}
	sbuf_printf(pg, "%s ", pdfpos00(x1 / 10, y1 / 10));
	sbuf_printf(pg, "%s ", pdfpos00(x2 / 10, y2 / 10));
	sbuf_printf(pg, "%s c\n", pdfpos00(x3 / 10, y3 / 10));
	outrel(ch / 2, cv / 2);
}

/* draw a circle */
void drawc(int c)
{
	drawquad(+c, +c);
	drawquad(+c, -c);
	drawquad(-c, -c);
	drawquad(-c, +c);
	outrel(c, 0);
}

/* draw an ellipse */
void drawe(int h, int v)
{
	drawquad(+h, +v);
	drawquad(+h, -v);
	drawquad(-h, -v);
	drawquad(-h, +v);
	outrel(h, 0);
}

/* draw an arc */
void drawa(int h1, int v1, int h2, int v2)
{
	drawl(h1 + h2, v1 + v2);
}

/* draw a spline */
void draws(int h1, int v1, int h2, int v2)
{
	int x0 = o_h;
	int y0 = o_v;
	int x1 = x0 + h1;
	int y1 = y0 + v1;
	int x2 = x1 + h2;
	int y2 = y1 + v2;

	sbuf_printf(pg, "%s ", pdfpos((x0 + 5 * x1) / 6, (y0 + 5 * y1) / 6));
	sbuf_printf(pg, "%s ", pdfpos((x2 + 5 * x1) / 6, (y2 + 5 * y1) / 6));
	sbuf_printf(pg, "%s c\n", pdfpos((x1 + x2) / 2, (y1 + y2) / 2));

	outrel(h1, v1);
}

void docheader(char *title, int pagewidth, int pageheight, int linewidth)
{
	if (title)
		outinfo("Title", title);
	obj_map();
	pdf_root = obj_map();
	pdf_pages = obj_map();
	pdfout("%%PDF-1.6\n\n");
	pdf_width = (pagewidth * 72 + 127) / 254;
	pdf_height = (pageheight * 72 + 127) / 254;
	pdf_linewid = linewidth;
}

void doctrailer(int pages)
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
	if (pdf_dests > 0)
		pdfout("  /Dests %d 0 R\n", pdf_dests);
	if (pdf_outline > 0)
		pdfout("  /Outlines %d 0 R\n", pdf_outline);
	pdfout(">>\n");
	obj_end();
	/* fonts */
	pfont_done();
	/* info object */
	info_id = obj_beg(0);
	pdfout("<<\n");
	if (pdf_title[0])
		pdfout("  /Title %s\n", pdftext_static(pdf_title));
	if (pdf_author[0])
		pdfout("  /Author %s\n", pdftext_static(pdf_author));
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
}

void docpagebeg(int n)
{
	pg = sbuf_make();
	sbuf_printf(pg, "q\n");
	sbuf_printf(pg, "BT\n");
}

void docpageend(int n)
{
	int cont_id;
	int i;
	o_flush();
	sbuf_printf(pg, "ET\n");
	sbuf_printf(pg, "Q\n");
	/* page contents */
	cont_id = obj_beg(0);
	pdfout("<<\n");
	pdfout("  /Length %d\n", sbuf_len(pg) - 1);
	pdfout(">>\n");
	pdfout("stream\n");
	pdfmem(sbuf_buf(pg), sbuf_len(pg));
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
	pdfout("  /Rotate %f\n", o_rdeg);
	pdfout("  /Parent %d 0 R\n", pdf_pages);
	pdfout("  /Resources <<\n");
	pdfout("    /Font <<");
	for (i = 0; i < pfonts_n; i++) {
		if (o_iset[i]) {
			struct pfont *ps = &pfonts[i];
			if (ps->cid)
				pdfout(" /%s %d 0 R", ps->name, ps->obj);
			else
				pdfout(" /%s.%d %d 0 R", ps->name, ps->sub, ps->obj);
		}
	}
	pdfout(" >>\n");
	if (xobj_n) {				/* XObjects */
		pdfout("    /XObject <<");
		for (i = 0; i < xobj_n; i++)
			pdfout(" /FO%d %d 0 R", i, xobj[i]);
		pdfout(" >>\n");
	}
	pdfout("  >>\n");
	pdfout("  /Contents %d 0 R\n", cont_id);
	if (ann_n) {
		pdfout("  /Annots [");
		for (i = 0; i < ann_n; i++)
			pdfout(" %d 0 R", ann[i]);
		pdfout(" ]\n");
	}
	pdfout(">>\n");
	obj_end();
	sbuf_free(pg);
	memset(o_iset, 0, pfonts_n * sizeof(o_iset[0]));
	xobj_n = 0;
	ann_n = 0;
}
