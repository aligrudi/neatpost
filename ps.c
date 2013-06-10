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
	out("%%%%EndPage: %d %d\n", n, n);
}

void ps_trailer(int pages, char *fonts)
{
	out("%%%%Trailer\n");
	out("done\n");
	out("%%%%DocumentFonts: %s\n", fonts);
	out("%%%%Pages: %d\n", pages);
}

static char *prolog =
	"/linewidth .4 def\n"
	"/resolution 720 def\n"
	"/pagesize [612 792] def\n"
	"/inch {72 mul} bind def\n"
	"\n"
	"/setup {\n"
	"	counttomark 2 idiv {def} repeat pop\n"
	"\n"
	"	/scaling 72 resolution div def\n"
	"	linewidth setlinewidth\n"
	"	1 setlinecap\n"
	"\n"
	"	0 pagesize 1 get translate\n"
	"	scaling scaling scale\n"
	"\n"
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
	"/m {neg dup /y exch def moveto} bind def\n"
	"/g {neg moveto {glyphshow} forall} bind def\n"
	"/done {/lastpage where {pop lastpage} if} def\n"
	"\n"
	"/f {\n"
	"	dup /font exch def findfont exch\n"
	"	dup /ptsize exch def scaling div dup /size exch def scalefont setfont\n"
	"	linewidth ptsize mul scaling 10 mul div setlinewidth\n"
	"} bind def\n";

void ps_header(void)
{
	out("%%!PS-Adobe-2.0\n");
	out("%%%%Version: 1.0\n");
	out("%%%%Creator: neatroff - http://litcave.rudi.ir/\n");
	out("%%%%DocumentFonts: (atend)\n");
	out("%%%%Pages: (atend)\n");
	out("%%%%EndComments\n");

	out("%%%%BeginProlog\n");
	out("%s", prolog);
	out("%%%%EndProlog\n");
	out("%%%%BeginSetup\n");
	out("<< /PageSize pagesize /ImagingBBox null >> setpagedevice\n");
	out("mark\n");
	out("setup\n");
	out("%%%%EndSetup\n");
}
