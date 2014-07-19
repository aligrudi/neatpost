#include "post.h"

void ps_pagebeg(int n)
{
	out("%%%%Page: %d %d\n", n, n);
	out("/saveobj save def\n");
	out("mark\n");
	out("%d pagesetup\n", n);
}

void ps_pageend(int n)
{
	out("cleartomark\n");
	out("showpage\n");
	out("saveobj restore\n");
}

void ps_trailer(int pages, char *fonts)
{
	out("%%%%Trailer\n");
	out("done\n");
	out("%%%%DocumentFonts: %s\n", fonts);
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
void ps_header(int pagewidth, int pageheight, int linewidth)
{
	out("%%!PS-Adobe-2.0\n");
	out("%%%%Version: 1.0\n");
	out("%%%%Creator: neatroff - http://litcave.rudi.ir/\n");
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
