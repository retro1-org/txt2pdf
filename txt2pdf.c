/*
1         2         3         4         5         6         7         8         9        10        11        12        13
123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012
*/

/**
 *
 *  Copyright (c) 2017 Steven J Zoppi (sjzoppi@gmail.com)
 *  All rights reserved.
 *
 *  Name: txt2pdf.c
 *
 *  Description:
 *
 *      Provide a filter through which text files can be turned into 
 *      PDF 1.3 conformant streams.
 *
 *      Emulate ASA/ANSI 60 line by 132 column lineprinter with PDF output.
 *
 *  Copyright Notice ---------------------------------------------------------
 *
 *  Permission to use, copy, modify and distribute this software and
 *  its documentation is hereby granted, provided that both the copyright
 *  notice and this permission notice appear in all copies of the
 *  software, derivative works or modified versions, and any portions
 *  thereof, and that both notices appear in supporting documentation.
 *
 *  STEVEN J. ZOPPI ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 *  CONDITION.  STEVEN J. ZOPPI DISCLAIMS ANY LIABILITY OF ANY KIND
 *  FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *  --------------------------------------------------------------------------
 *
 *  Created:
 *  txt2pdf; Steven J. Zoppi, Oct 24, 2017
 *
 *  Tested with xpdf, gv/ghostview, and acroread (PC version) PDF interpreters.
 *  Validate Against: https://www.pdf-online.com/osa/validate.aspx
 *
 */

/**
 * -----------------------------------------------------------------------------
 *
 *   V1: SZoppi Enhancements (201701024):
 *
 * -----------------------------------------------------------------------------
 *  TODO List:
 *      Add Metadata Stream (PDF 1.4) in addition to a "File Trailer" INFO
 *          entry at the end of the file (Section 7.5.5 of PDF32000_2008)
 *          Page 548 of PDF32000_2008 (Section 14.3)
 * -----------------------------------------------------------------------------
 */

 /**
  *  Headers and Includes 
  */

#include "stdafx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <tchar.h>
#include "unistd.h"
#include "XGetopt.h"

/**
 * Compiler Function Definitions 
 */

#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define ABS(x)          ((x) < 0 ? -(x) : (x))


/**
 *	Output Headings and Constants
 */

static  TCHAR GV_DashCode[256];
static  TCHAR GV_BodyFontName[256];
static  TCHAR GV_HeadingFontName[256];

const   float GV_VersionNumber = 1.1f;
const   int CGreyScaleValue = 0xC0C0C0;
const   int CTealBarValue = 0xC0F0F0;

/**
 *  Global Variable Declarations
 *  Initializations take place in main() block
 */

float   GV_BodyFontSize;
float   GV_StandardLineSize;
float   GV_LinesPerPage;
float   GV_PDFPageYPosition;
float   GV_PageDepth;
float   GV_PageMarginBottom;
float   GV_PageMarginLeft;
float   GV_PageMarginRight;
float   GV_PageMarginTop;
float   GV_PageWidth;
float   GV_TitleFontSize;
float   GV_UnitMultiplier;

bool    GV_IsASA;
bool    GV_IsExtendedASCII;
bool    GV_IsPrintPageNumbers;
bool    GV_IsPrintLineNumbers;
bool    GV_IsPerPageLineNumbers;
bool    GV_IsPageCountPositionTop;

int     GV_ShadeStep;
int     GV_CurrentLineCount;
int     GV_CurrentPageCount;

static  TCHAR GV_TitleLeft[256];
static  TCHAR GV_TitleRight[256];
static  TCHAR GV_ImpactTop[256];

int     GV_PDFObjectId = 1;
int     GV_PDFPageTreeId;
int     GV_PDFNumberOfPages = 0;
int     GV_PDFXRefCount = 0;
int     GV_PDFStreamId;
int     GV_PDFStreamLengthId;

long    GV_PDFStreamStart;
long   *GV_XReferences = NULL;

/**
 *	Structures and Type Definitions
 */

struct _RGB
    {
    double r;
    double g;
    double b;
    };

typedef _RGB RGB;

struct _PageList
    {
    struct _PageList *next;
    int page_id;
    };

typedef _PageList PageList;

PageList *GV_PAGE_LIST = NULL;
PageList **GV_INSERT_PAGE = &GV_PAGE_LIST;

/**
 *	Color Definitions used throughout the solution
 */

RGB   GV_OVERSTRIKE_COLOR;
RGB   GV_BAR_COLOR;
RGB   GV_FONT_COLOR;
RGB   GV_CURRENT_COLOR;
RGB   GV_LINE_NUMBER_COLOR;
RGB   GV_TITLE_COLOR;

/**
 *	Function Prototypes
 */

RGB  colorConverter(long hexValue);
long colorInverter(struct _RGB colorValue);
void adjust_pdf_ypos(float mult);
void do_process_pages();
void do_text_translation();
void end_pdf_page();
void print_margin_label();
void print_pdf_title_at(float xvalue, float yvalue, TCHAR *string);
void print_pdf_pagebars();
void print_pdf_string(TCHAR *buffer);
void print_pdf_impact_top();
void showhelp(int itype);
void start_pdf_object(int id);
void start_pdf_page();
void store_pdf_page(int id);


/*--------------------------------------------------------------------------
**  Purpose:        System initialization and main program loop.
**
**  Parameters:     Name        Description.
**                  argc        Argument count.
**                  argv        Array of argument strings.
**
**  Returns:        Zero.
**
**------------------------------------------------------------------------*/

int main(int argc, TCHAR *argv[])
    {
    /*
    **
    **  How getopt is typically used. The key points to notice are:
    **
    **  *	Normally, getopt is called in a loop. When getopt returns -1,
    **      indicating no more options are present, the loop terminates.
    **  *	A switch statement is used to dispatch on the return value from
    **      getopt. In typical use, each case just sets a variable that is
    **      used later in the program.
    **  *	A second loop is used to process the remaining non-option
    **      arguments.
    */

    char *varname;
    char subbuff[16];

    int index;
    int c;
    int ix;
    int ibar;
    float fmargin;

    /*
    ** Initialize Global Values to Their Defaults
    */
    GV_IsASA = TRUE;                                    //  ANSI/ASA Processor
    GV_IsExtendedASCII = FALSE;                         //  Mode is Extended ASCII 
    GV_UnitMultiplier = 72.0f;                          //  Standard 72 units per Inch
    GV_IsPrintPageNumbers = FALSE;                      //  Display Page Numbers
    GV_IsPrintLineNumbers = FALSE;                      //  Insert Line Numbers
    GV_IsPerPageLineNumbers = TRUE;                     //  Enumerate Page Numbers Per Page
                                                        //  Otherwise Enumerate over Whole Document
    GV_IsPageCountPositionTop = FALSE;                  //  Display Page Numbers on Top of Page
                                                        //  Otherwise Display on Bottom of Page
    GV_ShadeStep = 2;                                   //  Lines per Shade/Unshade Step
    GV_PageDepth = 8.5f * GV_UnitMultiplier;            //  8.5"    = 612.0
    GV_PageWidth = 11.0f * GV_UnitMultiplier;           //  11.0"   = 792.0
    GV_PageMarginTop = 0.5f * GV_UnitMultiplier;        //  0.5"    =  36.0
    GV_PageMarginBottom = GV_PageMarginTop;             //  Same as Top Margin
    GV_PageMarginLeft = 0.75f * GV_UnitMultiplier;      //  0.75"   =  54.0
    GV_PageMarginRight = GV_PageMarginLeft;             //  Same as Left Margin
    GV_LinesPerPage = 60.0f;                            //  Units / In Lines
    GV_CurrentLineCount = 0;                            //  Current Line Count
    GV_CurrentPageCount = 0;                            //  Current Page Count

    GV_FONT_COLOR = colorConverter(0x000000l);          //  Default is BLACK font
    GV_CURRENT_COLOR = GV_FONT_COLOR;                   //  Start with Default
    GV_LINE_NUMBER_COLOR = colorConverter(0x330099);    //  (Blue)
    GV_TITLE_COLOR = colorConverter(0xFF3300);          //  (Orange)
    GV_BAR_COLOR = colorConverter(CGreyScaleValue);     //  (Grey)

    GV_TitleLeft[0] = '\0';                             //  Default is "Zero-Length String"
    GV_TitleRight[0] = '\0';                            //  Default is "Zero-Length String"
    GV_ImpactTop[0] = '\0';                             //  Default is "Zero-Length String"

    strncpy(GV_DashCode, "", sizeof(GV_DashCode));      //  DashLines (None)
    strncpy(GV_BodyFontName, "Courier",                 //  Default is Courier
            sizeof(GV_BodyFontName));
    strncpy(GV_HeadingFontName, "Courier-Bold",         //  Default is Courier-Bold
            sizeof(GV_HeadingFontName));

    GV_TitleFontSize = 12.0;                            //  12 Points (Fixed)

    varname = getenv("IMPACT_GRAYBAR");                 //  If the user supplied the right
    if (varname != (char)NULL)                          //  environment variable - use it.
        {
        if (varname[0] != '\0')
            {
            sscanf(varname, "%x", &ibar);
            if (ibar > 0)
                {
                GV_BAR_COLOR = colorConverter(ibar);
                }
            }
        }

    varname = getenv("IMPACT_TOP");                     //  If the user supplied the right
    if (varname != (char)NULL)                          //  environment variable - use it.
        {
        if (varname[0] != '\0')
            {
            strncpy(GV_ImpactTop, varname, sizeof(GV_ImpactTop));
            }
        }
    opterr = 0;

    while ((c = getopt(argc, argv, _T("1:2:A:B:d:g:H:hi:L:l:M:n:N:o:pPR:t:T:u:W:vxX"))) != EOF)
        switch (c)
            {
                case _T('A'): GV_IsASA = (bool)((int)strtol(optarg, NULL, 10) == 1);           break; /* Formatted as ANSI/ASA    */
                case _T('H'): GV_PageDepth = (float)strtod(optarg, NULL) * GV_UnitMultiplier;  break; /* Height                   */
                case _T('W'): GV_PageWidth = (float)strtod(optarg, NULL) * GV_UnitMultiplier;  break; /* Width                    */

                case _T('M'):                                                                         /* Parse the Margin Options */

                    ix = 1;
                    while ((optarg[ix] != 0) && (ix < sizeof(subbuff)))
                        {
                        subbuff[ix - 1] = optarg[ix];
                        ix++;
                        }
                    subbuff[ix - 1] = '\0';
                    fmargin = (float)strtod(subbuff, NULL) * GV_UnitMultiplier;

                    switch (optarg[0])
                        {

                            case _T('A'):           //  All Margins
                            case _T('a'):
                                GV_PageMarginLeft = fmargin;
                                GV_PageMarginRight = fmargin;
                                GV_PageMarginTop = fmargin;
                                GV_PageMarginBottom = fmargin;
                                break;

                            case _T('L'):           //  Left Margin
                            case _T('l'):
                                GV_PageMarginLeft = fmargin;
                                break;

                            case _T('R'):           //  Right Margin
                            case _T('r'):
                                GV_PageMarginRight = fmargin;
                                break;

                            case _T('T'):           //  Top Margin
                            case _T('t'):
                                GV_PageMarginTop = fmargin;
                                break;

                            case _T('B'):           //  Bottom Margin
                            case _T('b'):
                                GV_PageMarginBottom = fmargin;
                                break;
                            default:
                                fprintf(stderr, "(info) SWITCH IS %c\n", c);
                                if (isprint(optarg[0]))
                                    {
                                    fprintf(stderr, "(warning) Unknown MARGIN Identifier '%c'. Only Tt, Ll, Rr, Bb permitted.\n", optarg[0]);
                                    }
                                else
                                    {
                                    fprintf(stderr, "(warning) Unknown Option '%s'.\n", argv[optind - 1]);
                                    }
                                showhelp(2);
                                exit(1);
                                break;

                        }
                    break;

                case _T('g'): GV_BAR_COLOR = colorConverter(strtol(optarg, NULL, 16));         break; /* color of graybar         */

                case _T('l'): GV_LinesPerPage = (float)strtod(optarg, NULL);                   break; /* lines per page           */
                case _T('u'): GV_UnitMultiplier = (float)strtod(optarg, NULL);                 break; /* unit of measure mult     */
                case _T('i'): GV_ShadeStep = (int)strtod(optarg, NULL);                        break; /* increment for bars       */

                case _T('R'): strncpy(GV_TitleRight, optarg, sizeof(GV_TitleRight));           break; /* margin right label       */
                case _T('L'): strncpy(GV_TitleLeft, optarg, sizeof(GV_TitleLeft));             break; /* margin left label        */
                case _T('T'): strncpy(GV_ImpactTop, optarg, sizeof(GV_ImpactTop));             break; /* IMPACT-TOP               */

                case _T('d'): strncpy(GV_DashCode, optarg, sizeof(GV_DashCode));               break; /* dash code                */
                case _T('1'): strncpy(GV_BodyFontName, optarg, sizeof(GV_BodyFontName));       break; /* font                     */
                case _T('2'): strncpy(GV_HeadingFontName, optarg, sizeof(GV_HeadingFontName)); break; /* font                     */
                case _T('o'): GV_OVERSTRIKE_COLOR = colorConverter(strtol(optarg, NULL, 16));  break; /* color of overprint       */

                case _T('n'):
                    GV_LINE_NUMBER_COLOR = colorConverter(strtol(optarg, NULL, 16));
                    GV_IsPrintLineNumbers = TRUE;                                              break; /* Color of Line Numbers    */

                case _T('t'):
                    GV_TITLE_COLOR = colorConverter(strtol(optarg, NULL, 16));                 break; /* Color of Line Numbers    */

                case _T('N'):
                    GV_IsPrintLineNumbers = TRUE;
                    GV_IsPerPageLineNumbers = (bool)((int)strtol(optarg, NULL, 10) == 1);      break; /* Printer Line Numbers     */

                case _T('P'): GV_IsPrintPageNumbers = TRUE; GV_IsPageCountPositionTop = TRUE;  break; /* display page #s - top    */
                case _T('p'): GV_IsPrintPageNumbers = TRUE; GV_IsPageCountPositionTop = FALSE; break; /* display page #s - bottom */

                case _T('h'): showhelp(1); exit(1);                                            break; /* help                     */
                case _T('x'): case _T('X'): showhelp(2); exit(1);                              break; /* Show parameters          */
                case _T('v'): fprintf(stderr, "(info) txt2pdf version %f\n", GV_VersionNumber);
                    exit((int)GV_VersionNumber);                                               break; /* version                  */

                case _T('?'):
                    fprintf(stderr, "(info) SWITCH IS %c\n", c);
                    if (isprint(optopt))
                        {
                        fprintf(stderr, "(error) Unknown Option '-%c'.\n", optopt);
                        }
                    else
                        {
                        fprintf(stderr, "(error) Unknown Option '%s'.\n", argv[optind - 1]);
                        }
                    showhelp(2);
                    exit(1);
                    return 1;

                default:
                    abort();
            }

    if (GV_ShadeStep < 1)
        {
        fprintf(stderr, "(warning) Resetting -i %d to -i 1\n", GV_ShadeStep);
        GV_ShadeStep = 1;
        }

    for (index = optind; index < argc; index++)
        {
        fprintf(stderr, "(warning) Non-option Argument %s\n", argv[index]);
        }
    do_process_pages();
    exit(0);
    }


void do_process_pages()
    {

    int		i;
    int		catalog_id;
    int		font_id0;
    int		font_id1;
    long	start_xref;
    
    /*
    ** Indicate standard supporting METADATA STREAMS
    */
    printf("%%PDF-1.4\n");

    /**
     *  General PDF Convention:
     *
     *  If a PDF file contains binary data, as most do , it is
     *  recommended that the header line be immediately followed by a
     *  comment line containing at least four binary characters--that is,
     *  characters whose codes are 128 or greater. 
     *
     *  This will ensure proper behavior of file transfer applications 
     *  that inspect data near the beginning of a file to determine whether
     *  to treat the file's contents as text or as binary.
     *
     *  The convention is to use the Magic Number of E2E3CFD3.
     */

    fprintf(stdout, "%%%c%c%c%c\n", 0xE2, 0xE3, 0xCF, 0xD3);        //  PDF Magic Number
    fprintf(stdout, "%% PDF: Adobe Portable Document Format\n");

    /**
     *  This is a SquareBox calculation and only works for monospace fonts
     *  in which all characters fit within the same space.
     *
     *  Therefore any fonts which are variable pitch (proportional) cannot be used
     *  reliably in the rendering of a listing.  For this reason we choose the defaults
     *  of Courier (monospace) because it is inbuilt (automatically provided) in the
     *  Adobe provided PDF Engine.
     */

    GV_StandardLineSize = (GV_PageDepth - GV_PageMarginTop - GV_PageMarginBottom) / GV_LinesPerPage;
    GV_BodyFontSize = GV_StandardLineSize;

    GV_PDFObjectId = 1;
    GV_PDFPageTreeId = GV_PDFObjectId++;
    
    /*
    **  Process all of the inputs from STDIN
    */
    do_text_translation();

    /*
    **  Font Object 0 Is used for the general body content
    */
    font_id0 = GV_PDFObjectId++;
    start_pdf_object(font_id0);
    printf("<</Type/Font/Subtype/Type1/BaseFont/%s/Encoding/WinAnsiEncoding>>\nendobj\n", GV_BodyFontName);

    /*
    **  Font Object 1 Is used for the body text and line numbers
    */
    font_id1 = GV_PDFObjectId++;
    start_pdf_object(font_id1);
    printf("<</Type/Font/Subtype/Type1/BaseFont/%s/Encoding/WinAnsiEncoding>>\nendobj\n", GV_HeadingFontName);

    /*
    **  Now that the Font Resources are declared, we generate the page tree object
    */

    start_pdf_object(GV_PDFPageTreeId);
    printf("<</Type /Pages /Count %d\n", GV_PDFNumberOfPages);

    PageList *ptr = GV_PAGE_LIST;
    PageList *ptrfree = GV_PAGE_LIST;

    printf("/Kids[\n");
    while (ptr != NULL)
        {
        printf("%d 0 R\n", ptr->page_id);
        ptrfree = ptr;
        ptr = ptr->next;
        free(ptrfree);
        }
    printf("]\n");


    /*
    **  Now create the Subordinate Resources objects
    */

    printf("/Resources<</ProcSet[/PDF/Text]/Font<<");
    printf("/F0 %d 0 R\n", font_id0);
    printf("/F1 %d 0 R\n", font_id1);
    printf("/F2<</Type /Font /Subtype /Type1 /BaseFont /%s /Encoding /WinAnsiEncoding >> >>\n", GV_HeadingFontName);
    printf(">>/MediaBox [ 0 0 %g %g ]\n", GV_PageWidth, GV_PageDepth);
    printf(">>\nendobj\n");
    
    /*
    **  Now create the Catalog and Cross-References object
    */

    catalog_id = GV_PDFObjectId++;
    start_pdf_object(catalog_id);
    printf("<</Type /Catalog /Pages %d 0 R>>\nendobj\n", GV_PDFPageTreeId);
    start_xref = ftell(stdout);
    printf("xref\n");
    printf("0 %d\n", GV_PDFObjectId);
    printf("0000000000 65535 f \n");

    for (i = 1; i < GV_PDFObjectId; i++)
        {
        printf("%010ld 00000 n \n", GV_XReferences[i]);
        }

    free(GV_XReferences);

    /*
    **  Now Complete the file by writing the trailer with the
    **  appropriate back-references to the Cross-Reference Object
    **  and the Root object.
    */
    printf("trailer\n<<\n/Size %d\n/Root %d 0 R\n>>\n", GV_PDFObjectId, catalog_id);
    printf("startxref\n%ld\n%%%%EOF\n", start_xref);
    }

/**
 *  Color Manipulation Routines
 */

RGB colorConverter(long hexValue)
    {
    struct _RGB rgbColor;
    rgbColor.r = ((hexValue >> 16) & 0xFF) / 255.0;  // Extract the RR byte
    rgbColor.g = ((hexValue >> 8) & 0xFF) / 255.0;   // Extract the GG byte
    rgbColor.b = ((hexValue) & 0xFF) / 255.0;        // Extract the BB byte

    return rgbColor;
    }

long colorInverter(struct _RGB colorValue)
    {
    long colorLong;
    colorLong = ((long)(colorValue.r * 255) << 16) +
        ((long)(colorValue.g * 255) << 8) +
        ((long)(colorValue.b * 255));
    return (colorLong);
    }

/**
 *  PDF Generation routines
 */

void store_pdf_page(int id)
    {

    PageList *n = (PageList *)malloc(sizeof(*n));

    if (n == NULL)
        {
        fprintf(stderr, "(error) Unable to allocate array for page %d.", GV_PDFNumberOfPages + 1);
        exit(1);
        }
    n->next = NULL;
    n->page_id = id;

    *GV_INSERT_PAGE = n;
    GV_INSERT_PAGE = &n->next;

    GV_PDFNumberOfPages++;
    }


void start_pdf_object(int id)
    {
    if (id >= GV_PDFXRefCount)
        {

        long *new_xrefs;
        int  delta, new_num_xrefs;
        delta = GV_PDFXRefCount / 5;

        if (delta < 1000)
            {
            delta += 1000;
            }

        new_num_xrefs = GV_PDFXRefCount + delta;
        new_xrefs = (long *)malloc(new_num_xrefs * sizeof(*new_xrefs));

        if (new_xrefs == NULL)
            {
            fprintf(stderr, "(error) Unable to allocate array for object %d.", id);
            exit(1);
            }

        memcpy(new_xrefs, GV_XReferences, GV_PDFXRefCount * sizeof(*GV_XReferences));
        free(GV_XReferences);
        GV_XReferences = new_xrefs;
        GV_PDFXRefCount = new_num_xrefs;

        }

    GV_XReferences[id] = ftell(stdout);
    printf("%d 0 obj", id);

    }

void print_pdf_pagebars()
    {

    float x1;
    float y1;
    float height;
    float width;
    float step;

    /*
    **  Per the Postscript Specification:
    **      "R G B rg" where R G B are red, green, blue components
    **  in range 0.0 to 1.0 sets fill color, "RG" sets line
    **  color instead of fill color.
    **
    **  0.60 0.82 0.60 rg "Green Bar"
    **  fprintf(stdout, "%f g\n", 0.800781f); if you want to use gray scale value
    */
    
    fprintf(stdout, "%f %f %f rg\n", GV_BAR_COLOR.r, GV_BAR_COLOR.g, GV_BAR_COLOR.b);
    fprintf(stdout, "%d i\n", 1);

    x1 = GV_PageMarginLeft - (float) 0.1 * GV_BodyFontSize;
    height = GV_ShadeStep * GV_StandardLineSize;
    y1 = GV_PageDepth - GV_PageMarginTop - height - (float) 0.22 * GV_BodyFontSize;
    width = GV_PageWidth - GV_PageMarginLeft - GV_PageMarginRight;
    step = (float) 1.0;
    if (GV_DashCode[0] != '\0')
        {
        fprintf(stdout, "0 w [%s] 0 d\n", GV_DashCode); /* dash code array plus offset */
        }

    /**
     *  See Documentation at Bottom
     */

    while (y1 >= (GV_PageMarginBottom - height))
        {
        if (GV_DashCode[0] == '\0')
            {
            /* a shaded bar */
            fprintf(stdout, "%f %f %f %f re f\n", x1, y1, width, height);
            step = 2.0;
            /*
             * x1 y1 m x2 y2 l S
             * xxx w  # line width
               fprintf(stdout,"0.6 0.8 0.6 RG\n %f %f m %f %f l S\n",x1,y1,x1+width,y1);
            */
            }
        else
            {
            fprintf(stdout, "%f %f m ", x1, y1);
            fprintf(stdout, "%f %f l s\n", x1 + width, y1);
            }
        y1 = y1 - step*height;
        }
    if (GV_DashCode[0] != '\0')
        {
        fprintf(stdout, "[] 0 d\n");	/* set dash pattern to solid line */
        }

    fprintf(stdout, "%d G\n", 0);			/* */
    fprintf(stdout, "%d g\n", 0);			/* gray-scale value */

    }

void print_pdf_string(TCHAR *buffer)
    {

    /*
    **  Print string as (escaped_string) 
    **  where ()\ have a preceding \ character 
    **  added 
    */

    char c;


    if (GV_IsPrintLineNumbers)
        {
        /*
        **  If we are printing Line Numbers
        **
        **  We set the color, 
        **      print the line count, 
        **          reset the color to current.
        */
        fprintf(stdout, 
                "/F1 %f Tf\n %f %f %f rg\n (%6d | )Tj\n /F0 %f Tf\n %f %f %f rg ", 
                GV_BodyFontSize,
                GV_LINE_NUMBER_COLOR.r, GV_LINE_NUMBER_COLOR.g, GV_LINE_NUMBER_COLOR.b,
                GV_CurrentLineCount,
                GV_BodyFontSize,
                GV_CURRENT_COLOR.r, GV_CURRENT_COLOR.g, GV_CURRENT_COLOR.b);
        }
    else if (GV_CURRENT_COLOR.r != GV_FONT_COLOR.r || 
             GV_CURRENT_COLOR.g != GV_FONT_COLOR.g ||
             GV_CURRENT_COLOR.b != GV_FONT_COLOR.b )
        {
        /**
         *  If we are not printing line numbers
         *      we need to check to see if we need to emit
         *      a color change where different from the default.
         */
        fprintf(stdout,
                " %f %f %f rg\n",
                GV_CURRENT_COLOR.r, GV_CURRENT_COLOR.g, GV_CURRENT_COLOR.b);
        }

    putchar('(');

    while ((c = *buffer++) != '\0')
        {
        if (GV_IsExtendedASCII)
            {
            putchar(c + 127);
            }
        else
            {
            switch (c)      //  Escape the lower reserved characters
                {
                    case '(':
                    case ')':
                    case '\\':
                        putchar('\\');
                }
            putchar(c);
            }
        }


    putchar(')');
    }


void print_pdf_title_at(float xvalue, float yvalue, TCHAR *string)
    {

    fprintf(stdout, "BT /F2 %f Tf %f %f Td", GV_TitleFontSize, xvalue, yvalue);
    print_pdf_string(string);
    fprintf(stdout, " Tj ET\n");

    }


void print_pdf_impact_top()
    {

    float charwidth;
    float xvalue;
    float yvalue;
    float text_size = GV_TitleFontSize + 2.0f;
    
    if (GV_ImpactTop[0] != '\0') 
        {
            charwidth = text_size * (float) 0.60;	    /* assuming fixed-space font Courier-Bold */
            fprintf(stdout, "0.9 0.0 0.0 rg\n");		/* Bright Red */

            yvalue = GV_PageDepth - text_size;
            xvalue = GV_PageMarginLeft
                + ((GV_PageWidth - GV_PageMarginLeft - GV_PageMarginRight) / (float) 2.0)
                - (strlen(GV_ImpactTop) * charwidth / (float) 2.0);

            fprintf(stdout, "BT /F2 %f Tf %f %f Td", text_size, xvalue, yvalue);
            print_pdf_string(GV_ImpactTop);
            fprintf(stdout, " Tj ET\n");

         }

    }


void print_margin_label()
    {

    TCHAR pagestring[80];

    float charwidth;
    float position_left;
    float position_center;
    float position_right;
    bool  save_linenumber_state;

    save_linenumber_state = GV_IsPrintLineNumbers;
    GV_IsPrintLineNumbers = FALSE;

    print_pdf_impact_top();

    /* assuming fixed-space font Courier-Bold */
    charwidth = GV_TitleFontSize * 0.60f;

    fprintf(stdout, "%f %f %f rg\n", GV_TITLE_COLOR.r, GV_TITLE_COLOR.g, GV_TITLE_COLOR.b);

    if (GV_IsPrintPageNumbers)
        {
        sprintf_s(pagestring, sizeof(pagestring), _T("Page %04d"), GV_CurrentPageCount);
        position_center = GV_PageMarginLeft
            + ((GV_PageWidth - GV_PageMarginLeft - GV_PageMarginRight) / 2.0f)
            - (strlen(pagestring) * charwidth / 2.0f);
        }
    
    
    position_right = GV_PageWidth - GV_PageMarginRight - (strlen(GV_TitleRight)*charwidth); /* position_right Justified */
    position_left = GV_PageMarginLeft;                                               /* position_left justified */

    if (GV_TitleRight[0] != NULL)
        {
            print_pdf_title_at(position_right, GV_PageDepth - GV_PageMarginTop + 0.12f * GV_TitleFontSize, GV_TitleRight);
        }

    if (GV_IsPrintPageNumbers)
        {
        if (GV_IsPageCountPositionTop)
            {
                print_pdf_title_at(position_center, GV_PageDepth - GV_PageMarginTop + 0.12f * GV_TitleFontSize, pagestring);
            }
        else
            {
                print_pdf_title_at(position_center, GV_PageMarginBottom - GV_TitleFontSize, pagestring);
            }
        }

    if (GV_TitleLeft[0] != '\0')
        {
            print_pdf_title_at(position_left, GV_PageDepth - GV_PageMarginTop + 0.12f * GV_TitleFontSize, GV_TitleLeft);

        }

    GV_IsPrintLineNumbers = save_linenumber_state;

    fprintf(stdout, "%f %f %f rg\n",
            GV_FONT_COLOR.r, GV_FONT_COLOR.g, GV_FONT_COLOR.b);

    }


void start_pdf_page()
    {
    GV_PDFStreamId = GV_PDFObjectId++;
    GV_PDFStreamLengthId = GV_PDFObjectId++;
    GV_CurrentPageCount++;
    if (GV_IsPerPageLineNumbers)
        {
        GV_CurrentLineCount = 0;
        }
    start_pdf_object(GV_PDFStreamId);
    printf("<< /Length %d 0 R >>", GV_PDFStreamLengthId);
    printf("stream\n");
    GV_PDFStreamStart = ftell(stdout);

    print_pdf_pagebars();

    print_margin_label();

    printf("BT\n/F0 %g Tf\n", GV_BodyFontSize);
    GV_PDFPageYPosition = GV_PageDepth - GV_PageMarginTop;
    printf("%g %g Td\n", GV_PageMarginLeft, GV_PDFPageYPosition);
    printf("%g TL\n", GV_StandardLineSize);

    }


void end_pdf_page()
    {

    long stream_len;
    int page_id = GV_PDFObjectId++;

    store_pdf_page(page_id);
    printf("ET\n");
    stream_len = ftell(stdout) - GV_PDFStreamStart;
    printf("endstream\nendobj\n");
    start_pdf_object(GV_PDFStreamLengthId);
    printf("\n%ld\nendobj\n", stream_len);
    start_pdf_object(page_id);
    printf("<</Type/Page/Parent %d 0 R/Contents %d 0 R>>\nendobj\n", GV_PDFPageTreeId, GV_PDFStreamId);

    }


void adjust_pdf_ypos(float mult)
    {

    if (GV_PDFPageYPosition < GV_PageDepth - GV_PageMarginTop)
        {  /* if not at top of page */
        GV_PDFPageYPosition += GV_StandardLineSize*mult;
        }

    }


void do_text_translation()
    {

    char buffer1[4096];
    char buffer2[4096];
    char ASA;
    bool bResetColor;

    int i1;
    int i2;

    start_pdf_page();

    while (gets_s(&buffer1[0], sizeof(buffer1)) != NULL)
        {
        GV_CurrentLineCount++;

        bResetColor = FALSE;
        GV_IsExtendedASCII = FALSE;

        /* +1 for roundoff , using floating point point units */

        if (GV_PDFPageYPosition <= (GV_PageMarginBottom + 1) && strlen(buffer1) != 0 && (GV_IsASA && buffer1[0] != '+') )
            {
            end_pdf_page();
            start_pdf_page();
            }

        if (strlen(buffer1) == 0)
            { /* blank line */

            printf("T*()Tj\n");

            }
        else if (!GV_IsASA)

            /** This is the NON-ASA Format Processor
             **     We construct a parallel buffer (buffer2) which
             **     will be a substitute for the regular ASA buffer
             **/

            {

            i2 = 0;
            buffer2[i2] = '\0';      // NULL at the end of buffer2
            /**
             ** Scan the buffer starting from the offset through the end of the buffer
             **/
            for (i1 = 0; i1 < sizeof(buffer1); i1++)
                {
                switch (buffer1[i1])
                    {
                        case '\f':  //  formfeed character invokes new page
                            if (GV_PDFPageYPosition < GV_PageDepth - GV_PageMarginTop)
                                {
                                end_pdf_page();
                                start_pdf_page();
                                }
                            break;
                        case '\r':
                            GV_PDFPageYPosition -= GV_StandardLineSize;
                            /**
                             *  Don't process the final CR
                             */
                            if (buffer1[i1 + 1] != '\0')
                                {
                                /**
                                 *  just treat it as an overstrike
                                 */
                                GV_CURRENT_COLOR = GV_OVERSTRIKE_COLOR;
                                fprintf(stdout, "0 %f Td\n", GV_StandardLineSize);
                                adjust_pdf_ypos(1.0);
                                bResetColor = TRUE;
                                GV_CurrentLineCount--;
                                /**
                                 *  Fall through to print
                                 */
                                } 

                        case '\0':
                            if (buffer2[0] != '\0')
                                {
                                    printf("T*");
                                    print_pdf_string(&buffer2[0]);
                                    printf("Tj\n");
                                }
                            else
                                {
                                    adjust_pdf_ypos(1.0);
                                    GV_CurrentLineCount--;
                                }
                            /**
                             *  Move the pointer for buffer2 back
                             */
                            i2 = 0;
                            buffer2[i2] = '\0';

                            break;

                        default:
                            buffer2[i2] = buffer1[i1];
                            i2++;
                            buffer2[i2] = '\0';
                            break;
                    }
                }
            }   //  End of NON-ASA Processing
        else
            /*  This is the ASA Format Processor */
            {

            ASA = buffer1[0];

            switch (ASA)
                {

                    case '1':     /* start a new page before processing data on line */

                        if (GV_PDFPageYPosition < GV_PageDepth - GV_PageMarginTop)
                            {
                            end_pdf_page();
                            start_pdf_page();
                            }
                        break;

                    case '0':        /* put out a blank line before processing data on line */

                        printf("T*()Tj\n");
                        GV_PDFPageYPosition -= GV_StandardLineSize;
                        GV_CurrentLineCount++;
                        break;

                    case '-':        /* put out two blank lines before processing data on line */

                        printf("T*()Tj\n");
                        GV_PDFPageYPosition -= GV_StandardLineSize;
                        GV_PDFPageYPosition -= GV_StandardLineSize;
                        GV_CurrentLineCount++;
                        GV_CurrentLineCount++;
                        break;

                    case '+':        /* print at same y-position as previous line */

                        GV_CURRENT_COLOR = GV_OVERSTRIKE_COLOR;
                        fprintf(stdout, "0 %f Td\n", GV_StandardLineSize);
                        adjust_pdf_ypos(1.0);
                        bResetColor = TRUE;
                        GV_CurrentLineCount--;
                        break;

                    case 'R':        /* RED print at same y-position as previous line */
                    case 'G':        /* GREEN print at same y-position as previous line */
                    case 'B':        /* BLUE print at same y-position as previous line */

                        switch (ASA)
                            {
                            case 'R':
                                GV_CURRENT_COLOR = colorConverter(0xFF0000l);
                            case 'G':
                                GV_CURRENT_COLOR = colorConverter(0x00FF00l);
                            case 'B':
                                GV_CURRENT_COLOR = colorConverter(0x0000FFl);
                            }

                        bResetColor = TRUE;
                        fprintf(stdout, "0 %f Td\n", GV_StandardLineSize);
                        adjust_pdf_ypos(1.0);
                        GV_CurrentLineCount--;
                        break;

                    case 'H':        /* 1/2 line advance */

                        fprintf(stdout, "0 %f Td\n", GV_StandardLineSize / 2.0);
                        adjust_pdf_ypos(0.5);
                        break;

                    case 'r':        /* RED print */
                    case 'g':        /* GREEN print */
                    case 'b':        /* BLUE print */

                        switch (ASA)
                            {
                                case 'R':
                                    GV_CURRENT_COLOR = colorConverter(0xFF0000l);
                                case 'g':
                                    GV_CURRENT_COLOR = colorConverter(0x00FF00l);
                                case 'b':
                                    GV_CURRENT_COLOR = colorConverter(0x0000FFl);
                            }

                        bResetColor = TRUE;
                        break;

                    case '^':        /* print at same y-position as previous line like + but add 127 to character */

                        printf("0 %f Td\n", GV_StandardLineSize);
                        adjust_pdf_ypos(1.0);
                        GV_IsExtendedASCII = TRUE;
                        GV_CurrentLineCount--;
                        break;

                    case '>':        /* Unknown */

                        break;

                    case '\f':       /* ctrl-L is a common form-feed character on Unix, but NOT ASA */

                        end_pdf_page();
                        start_pdf_page();
                        break;

                    case ' ':

                        break;

                    default:

                        fprintf(stderr, "(warning) Unknown ASA Carriage Control Character %c\n", ASA);
                        break;

                }

            printf("T*");
            print_pdf_string(&buffer1[1]);
            printf("Tj\n");

            }   //  End of ASA Processing

            GV_PDFPageYPosition -= GV_StandardLineSize;

        if (bResetColor)
            {
            GV_CURRENT_COLOR = GV_FONT_COLOR;
            fprintf(stdout, "%f %f %f rg\n",
                    GV_FONT_COLOR.r, GV_FONT_COLOR.g, GV_FONT_COLOR.b);
            }

        }
    end_pdf_page();
    }



void showhelp(int itype)
    {
    switch (itype)
        {
            case 1:
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " | Steven J. Zoppi (2018 complete rewrite)                                      |\n");
                fprintf(stderr, " |     [Based on work by John Urban and P. G. Womack]                           |\n");
                fprintf(stderr, " | txt2pdf: A filter to convert text files with ASA carriage control to a PDF.  |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | SYNOPSIS:                                                                    |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |   txt2pdf(1) reads input from standard input. The first character            |\n");
                fprintf(stderr, " |   of each line is interpreted as a control character. Lines beginning with   |\n");
                fprintf(stderr, " |   any character other than those listed in the ASA carriage-control          |\n");
                fprintf(stderr, " |   characters table are interpreted as if they began with a blank,            |\n");
                fprintf(stderr, " |   and an appropriate diagnostic appears on standard error. The first         |\n");
                fprintf(stderr, " |   character of each line is not printed.                                     |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |     +------------+-----------------------------------------------+           |\n");
                fprintf(stderr, " |     | Character  |                                               |           |\n");
                fprintf(stderr, " |     +------------+-----------------------------------------------+           |\n");
                fprintf(stderr, " |     | +          | Do not advance; overstrike previous line.     |           |\n");
                fprintf(stderr, " |     | blank      | Advance one line.                             |           |\n");
                fprintf(stderr, " |     | null lines | Treated as if they started with a blank       |           |\n");
                fprintf(stderr, " |     | 0          | Advance two lines.                            |           |\n");
                fprintf(stderr, " |     | -          | Advance three lines (IBM extension).          |           |\n");
                fprintf(stderr, " |     | 1          | Advance to top of next page.                  |           |\n");
                fprintf(stderr, " |     | all others | Discarded (except for extensions listed below)|           |\n");
                fprintf(stderr, " |     +------------+-----------------------------------------------+           |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | ASA Extensions (while processing inputs)                                     |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |    H  Advance one-half line.                                                 |\n");
                fprintf(stderr, " |    R  Do not advance; overstrike previous line. Use red text color           |\n");
                fprintf(stderr, " |    G  Do not advance; overstrike previous line. Use green text color         |\n");
                fprintf(stderr, " |    B  Do not advance; overstrike previous line. Use blue text color          |\n");
                fprintf(stderr, " |    r  Advance one line. Use red text color                                   |\n");
                fprintf(stderr, " |    g  Advance one line. Use green text color                                 |\n");
                fprintf(stderr, " |    b  Advance one line. Use blue text color                                  |\n");
                fprintf(stderr, " |    ^  Overprint but add 127 to the ADE value of the character                |\n");
                fprintf(stderr, " |       (ie., use ASCII extended character set)                                |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " | PRINTABLE PAGE AREA                                                          |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " !  The page size may be specified using -H for height, -W for width, and -u    |\n");
                fprintf(stderr, " !  to indicate the points per unit (72 makes H and W in inches,                |\n");
                fprintf(stderr, " !  1 is used when units are in font points).                                   |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |  Page Margins are set using the -M<id><float> parameters                     |\n");
                fprintf(stderr, " |  Where:                                                                      |\n");
                fprintf(stderr, " |        <id>    = T|t, B|b, L|l, R|r      (Top, Bottom, Left, Right)          |\n");
                fprintf(stderr, " |                  A|a                     (All Margins)                       |\n");
                fprintf(stderr, " |        <float> = A Floating Point Number (In UNITS)                          |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |    For Example:                                                              |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |    -u 72 -H 8.5 -W 11   # page Height and Width                              |\n");
                fprintf(stderr, " |    -u 72 -MT0.5 -ML0.5 -MB0.5 -MR0.5 # margins (Top, Bottom, Left, Right)    |\n");
                fprintf(stderr, " |    -u 72 -MA0.5                      # margins (ALL are set to 0.5\")         |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |  common media sizes with -u 1:                                               |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |    +-------------------+------+------------+                                 |\n");
                fprintf(stderr, " |    | name              |  W   |        H   |                                 |\n");
                fprintf(stderr, " |    +-------------------+------+------------+                                 |\n");
                fprintf(stderr, " |    | Letterdj (11x8.5) | 792  |       612  | (LandScape)                     |\n");
                fprintf(stderr, " |    | A4dj              | 842  |       595  |                                 |\n");
                fprintf(stderr, " |    | Letter (8.5x11)   | 612  |       792  | (Portrait)                      |\n");
                fprintf(stderr, " |    | Legal             | 612  |       1008 |                                 |\n");
                fprintf(stderr, " |    | A5                | 420  |       595  |                                 |\n");
                fprintf(stderr, " |    | A4                | 595  |       842  |                                 |\n");
                fprintf(stderr, " |    | A3                | 842  |       1190 |                                 |\n");
                fprintf(stderr, " |    +-------------------+------+------------+                                 |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |SHADING AND COLOR                                                             |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |    -g RRGGBB       # Hex color value for shaded bars                         |\n");
                fprintf(stderr, " |    -O RRGGBB       # Hex color value for overstrike lines                    |\n");
                fprintf(stderr, " |    -n RRGGBB       # Hex color value for line numbering                      |\n");
                fprintf(stderr, " |    -i 2            # repeat shade pattern every N lines                      |\n");
                fprintf(stderr, " |    -d ' '          # dashcode pattern (seems buggy)                          |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |MARGIN LABELS                                                                 |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |   -C ''            # top middle page label.                                  |\n");
                fprintf(stderr, " |   -L ''            # top left page label.                                    |\n");
                fprintf(stderr, " |   -P               # add page numbers to TOP center                          |\n");
                fprintf(stderr, " |   -p               # add page numbers to BOTTOM center                       |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |TEXT OPTIONS                                                                  |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |   -l 60            # lines per page                                          |\n");
                fprintf(stderr, " |   -1 Courier       # PDF Built In Fonts (Standard 14 fonts)                  |\n");
                fprintf(stderr, " |                      Courier                                                 |\n");
                fprintf(stderr, " |                      Courier-Bold                                            |\n");
                fprintf(stderr, " |                      Courier-Oblique                                         |\n");
                fprintf(stderr, " |                      Courier-BoldOblique                                     |\n");
                fprintf(stderr, " |                      Symbol                                                  |\n");
                fprintf(stderr, " |                      ZapfDingbats                                            |\n");
                fprintf(stderr, " |                      Times-Roman                                             |\n");
                fprintf(stderr, " |                      Times-Italic                                            |\n");
                fprintf(stderr, " |                      Times-Bold                                              |\n");
                fprintf(stderr, " |                      Times-BoldItalic                                        |\n");
                fprintf(stderr, " |                      Helvetica                                               |\n");
                fprintf(stderr, " |                      Helvetica-Bold                                          |\n");
                fprintf(stderr, " |                      Helvetica-Oblique                                       |\n");
                fprintf(stderr, " |                      Helvetica-BoldOblique                                   |\n");
                fprintf(stderr, " |                      Times-Roman                                             |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |INTERPRETER OPTIONS                                                           |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |   -A (0|1)         # Non-ANSI/ANSI Formatted Inputs (Default ASA)            |\n");
                fprintf(stderr, " |   -N (0|1)         # add line numbers   0=Running or 1=Per-Page              |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " |   -v 3             # version number                                          |\n");
                fprintf(stderr, " |   -h               # display this help                                       |\n");
                fprintf(stderr, " |   -X               # display the parsed values and exit                      |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |ENVIRONMENT VARIABLES:                                                        |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | $IMPACT_TOP Will be printed in large red letters across the page top.        |\n");
                fprintf(stderr, " | $IMPACT_GRAYBAR sets the default gray-scale value, same as the -g switch.    |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, " |EXAMPLES:                                                                     |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | # create non-ASA file in portrait mode with a dashed line under every line   |\n");
                fprintf(stderr, " | txt2pdf -A0 -W 8.5 -H 11 -i 1 -d '2 4 1' -T 1 -B .75 < INFILE > junko.pdf    |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | # banner on top                                                              |\n");
                fprintf(stderr, " | env IMPACT_GRAYBAR=C0F0F0 IMPACT_TOP=CONFIDENTIAL                            |\n");
                fprintf(stderr, " | txt2pdf < test.txt > test.pdf                                                |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | # 132 landscape ASA                                                          |\n");
                fprintf(stderr, " |  txt2pdf -A1 LANDSCAPE <txt2pdf.c >junko.A.pdf                               |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | # 132 landscape with line numbers with dashed lines                          |\n");
                fprintf(stderr, " |  txt2pdf -L 'LANDSCAPE LINE NUMBERS' -d '3 1 2' \\                            |\n");
                fprintf(stderr, " |  -N -T .9 <txt2pdf.c >test.pdf                                               |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | # portrait 80 non-ASA file with dashed lines                                 |\n");
                fprintf(stderr, " |  txt2pdf -A0 PORTRAIT -S 1 -W 8.5 -H 11 -i 1 -d '2 4 1' \\                    |\n");
                fprintf(stderr, " |  -MT1 -MB.75 < txt2pdf.c > test.pdf                                          |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | # portrait 80 with line numbers , non-ASA                                    |\n");
                fprintf(stderr, " |  txt2pdf -L 'PORTRAIT LINE NUMBERS' -l 66 -A0 -W 8.5 -H 11 \\                 |\n");
                fprintf(stderr, " |  -i 1 -MT1 -MB.75 -N < txt2pdf.c > test.pdf                                  |\n");
                fprintf(stderr, " |                                                                              |\n");
                fprintf(stderr, " | # titling with ASA                                                           |\n");
                fprintf(stderr, " |  txt2pdf -d '1 0 1' -C \"$USER\" -i 1 -P -N -T 1 \\                             |\n");
                fprintf(stderr, " |  -A1 \"txt2pdf.c\" <txt2pdf.c >test.pdf                                        |\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");

            case 2:
                fprintf(stderr, " +----------------------   Current Settings Requested (below)   ----------------+\n");
                fprintf(stderr, " +------------------------------------------------------------------------------+\n");
                fprintf(stderr, "\t\t--== Operating Mode ==--\n");
                fprintf(stderr, "\t-A  [flag=%d]\t: Interpreter Mode (ASA/ANSI!=0)\n\n", GV_IsASA);

                fprintf(stderr, "\t-l  %f\t: Lines Per Page\n\n", GV_LinesPerPage);

                fprintf(stderr, "\t\t--== Page Dimensions ==--\n");
                fprintf(stderr, "\t-u  %f\t: Unit of Measure Multiplier (72.0 = 1 Inch)\n", GV_UnitMultiplier);

                fprintf(stderr, "\t-MT %f\t: Top margin\n", GV_PageMarginTop / GV_UnitMultiplier);
                fprintf(stderr, "\t-MB %f\t: Bottom margin\n", GV_PageMarginBottom / GV_UnitMultiplier);
                fprintf(stderr, "\t-ML %f\t: Left margin\n", GV_PageMarginLeft / GV_UnitMultiplier);
                fprintf(stderr, "\t-MR %f\t: Right margin\n\n", GV_PageMarginRight / GV_UnitMultiplier);

                fprintf(stderr, "\t-W  %f\t: Page Width  (In Units)\n", GV_PageWidth / GV_UnitMultiplier);
                fprintf(stderr, "\t-H  %f\t: Page Height (In Units)\n\n", GV_PageDepth / GV_UnitMultiplier);

                fprintf(stderr, "\t\t--== Color Specifications ==--\n");
                fprintf(stderr, "\t-o  R:%f\t: RGB of Overstrike   (0x%06X)\n\t    G:%f\n\t    B:%f\n", GV_OVERSTRIKE_COLOR.r, colorInverter(GV_OVERSTRIKE_COLOR), GV_OVERSTRIKE_COLOR.g, GV_OVERSTRIKE_COLOR.b);
                fprintf(stderr, "\t-g  R:%f\t: RGB of Greybar      (0x%06X)\n\t    G:%f\n\t    B:%f\n", GV_BAR_COLOR.r, colorInverter(GV_BAR_COLOR), GV_BAR_COLOR.g, GV_BAR_COLOR.b);
                fprintf(stderr, "\t-t  R:%f\t: RGB of Title        (0x%06X)\n\t    G:%f\n\t    B:%f\n", GV_TITLE_COLOR.r, colorInverter(GV_TITLE_COLOR), GV_TITLE_COLOR.g, GV_TITLE_COLOR.b);
                fprintf(stderr, "\t-n  R:%f\t: RGB of Line Numbers (0x%06X)\n\t    G:%f\n\t    B:%f\n\n", GV_LINE_NUMBER_COLOR.r, colorInverter(GV_LINE_NUMBER_COLOR), GV_LINE_NUMBER_COLOR.g, GV_LINE_NUMBER_COLOR.b);

                fprintf(stderr, "\t-i  %d\t\t: Shading Line Increment\n", GV_ShadeStep);
                fprintf(stderr, "\t-d  [%s]\t: Shading Line Dash Code\n\n", GV_DashCode);

                fprintf(stderr, "\t\t--== Fonts and Labeling ==--\n");
                fprintf(stderr, "\t-1  [%s]\t: Body Font Name\n", GV_BodyFontName);
                fprintf(stderr, "\t-2  [%s]\t: Heading Font Name\n\n", GV_HeadingFontName);

                fprintf(stderr, "\t-R  [%s]\t: Right Header Margin Label\n", GV_TitleRight);
                fprintf(stderr, "\t-L  [%s]\t: Left Header Margin Label\n\n", GV_TitleLeft);

                fprintf(stderr, "\t-N  [flag=%d]\t: add line numbers\n", GV_IsPrintLineNumbers);
                fprintf(stderr, "\t-P  [flag=%d]\t: Printing Page Numbers\n", GV_IsPrintPageNumbers);
                fprintf(stderr, "\t    [flag=%d]\t: Page Numbers Position TOP (!=0) BOTTOM (==0)\n", GV_IsPageCountPositionTop);

                fprintf(stderr, "\t\t--== Miscellaneous ==--\n");
                fprintf(stderr, "\t-v  %f\t: Version Number\n", GV_VersionNumber);
                fprintf(stderr, "\t-X  \t\t: Display Settings\n");
                fprintf(stderr, "\t-h  \t\t: Display Help and Settings\n");
                break;
        }
    }



/* ============================================================================================================================== */
/*
 *  PDF Reference:
 *  http://www.adobe.com/devnet/pdf/pdf_reference.html
 *
 */

 /*
 8.4.3.6       Line Dash Pattern

 The line dash pattern shall control the pattern of dashes and gaps used to stroke paths. It shall be specified by
 a dash array and a dash phase. The dash array's elements shall be numbers that specify the lengths of
 alternating dashes and gaps; the numbers shall be nonnegative and not all zero. The dash phase shall specify
 the distance into the dash pattern at which to start the dash. The elements of both the dash array and the dash
 phase shall be expressed in user space units.

 Before beginning to stroke a path, the dash array shall be cycled through, adding up the lengths of dashes and
 gaps. When the accumulated length equals the value specified by the dash phase, stroking of the path shall
 begin, and the dash array shall be used cyclically from that point onward. Table 56 shows examples of line
 dash patterns. As can be seen from the table, an empty dash array and zero phase can be used to restore the
 dash pattern to a solid line.

 Table 56 ­ Examples of Line Dash Patterns

 Dash Array       Appearance                   Description
 and Phase

 [] 0                                          No dash; solid, unbroken lines

 [3] 0                                         3 units on, 3 units off, ...

 [2] 1                                         1 on, 2 off, 2 on, 2 off, ...

 [2 1] 0                                       2 on, 1 off, 2 on, 1 off, ...

 [3 5] 6                                       2 off, 3 on, 5 off, 3 on, 5 off, ...

 [ 2 3 ] 11                                    1 on, 3 off, 2 on, 3 off, 2 on, ...


 Dashed lines shall wrap around curves and corners just as solid stroked lines do. The ends of each dash shall
 be treated with the current line cap style, and corners within dashes shall be treated with the current line join
 style. A stroking operation shall take no measures to coordinate the dash pattern with features of the path; it
 simply shall dispense dashes and gaps along the path in the pattern defined by the dash array.

 When a path consisting of several subpaths is stroked, each subpath shall be treated independently--that is,
 the dash pattern shall be restarted and the dash phase shall be reapplied to it at the beginning of each subpath.
 */