#include <stdlib.h>

#include <alg.h>
#include <btypeface.h>
#include <bimage.h>
#include <bmask.h>

struct Algorithm_Rec_str
{
    B_Mask edge;
    B_Mask slant[2];
    B_Mask curve[4];
};

#define DIM_NORM 100.0
#define EDGE 1
#define NOEDGE 0

static double Alg_calculate( const B_Image image, B_Mask masks[], int numConvs )
{
    int width = B_Image_getWidth( image );
    int height = B_Image_getHeight( image );
    int area = width * height;
    int score = 0;
    for( int r = 0; r < height; r++ )
    {
        for( int c = 0; c < width; c++ )
        {
            for( int i = 0; i < numConvs; i++ )
            {
                int toAdd = B_Mask_convolvePixel( masks[i], image, r, c );
                score += toAdd<0?0:toAdd;
            }
        }
    }
    area = area>0?area:1;
    double finalScore = (double)score/(double)area;

    // normalize
    finalScore = finalScore >= 1.0 ? 1.0 : finalScore;
    return finalScore;
}

// wrapper for doing operations that require entire glyph set
static double Alg_CalcAll( Algorithm alg, const BT_Face face, double (*algorithm)( Algorithm, const BT_Glyph ) )
{
    double scoreTotal = 0.0;
    int num = 0;
    for( int c = FIRST_GLYPH; c <= LAST_GLYPH; c++ )
    {
        const BT_Glyph glyph = BT_Face_getGlyph( face, c );
        if( glyph )
        {
            scoreTotal += algorithm( alg, glyph);
            ++num;
        }
    }
    double scoreAverage = scoreTotal/num;
    // normalize
    scoreAverage = scoreAverage > 1.0? 1.0 : scoreAverage;
    scoreAverage = scoreAverage < 0.0? 0.0 : scoreAverage;
    return scoreAverage;
}

/////// Dimensional /////
static void Alg_WidthInit( Algorithm alg ){}
static void Alg_WidthDone( Algorithm alg ){}
static double Alg_WidthAlg( Algorithm alg, const BT_Glyph glyph )
{
    const B_Image image = BT_Glyph_getPlain(glyph);
    double width = (double)B_Image_getWidth( image );
    // normalize
    return width / DIM_NORM;
}
static double Alg_WidthCalc( Algorithm alg, const BT_Face face )
{
    return Alg_CalcAll( alg, face, &Alg_WidthAlg );
}

static void Alg_HeightInit( Algorithm alg ){}
static void Alg_HeightDone( Algorithm alg ){}
static double Alg_HeightAlg( Algorithm alg, const BT_Glyph glyph )
{
    const B_Image image = BT_Glyph_getPlain(glyph);
    double height = (double)B_Image_getHeight( image );
    // normalize
    return height / DIM_NORM;
}
static double Alg_HeightCalc( Algorithm alg, const BT_Face face )
{
    return Alg_CalcAll( alg, face, &Alg_HeightAlg );
}

static void Alg_AspectRatioInit( Algorithm alg ){}
static void Alg_AspectRatioDone( Algorithm alg ){}
static double Alg_AspectRatioAlg( Algorithm alg, const BT_Glyph glyph )
{
    const B_Image image = BT_Glyph_getPlain(glyph);
    int widthInt = B_Image_getWidth( image );
    int heightInt = B_Image_getHeight( image );
    // deal with 0s: like with space
    widthInt = widthInt? widthInt : 1;
    heightInt = heightInt? heightInt : 1;

    double width = (double)widthInt;
    double height = (double)heightInt;
    double arScore = 0.5;   // default score
    double offset  = 0.0;   // offset from score

    if( heightInt > widthInt )
    {
        // offset from 0.5 is determined by the multiple
        offset = height/width/10.0;
    }
    else
    {
        // offset from 0.5 is determined by the multiple
        offset = width/height/10.0;
    }
    arScore += offset;
    return arScore;
}
static double Alg_AspectRatioCalc( Algorithm alg, const BT_Face face )
{
    return Alg_CalcAll( alg, face, &Alg_AspectRatioAlg );
}

static void Alg_xHeightInit( Algorithm alg ){}
static void Alg_xHeightDone( Algorithm alg ){}
/*
static double Alg_xHeightAlg( Algorithm alg, const B_Image image )
{
    return 0.0;
}
*/
static double Alg_xHeightCalc( Algorithm alg, const BT_Face face )
{
    const B_Image x = BT_Face_getChar(face, 'x');
    double height  = B_Image_getHeight(x);
    return height / DIM_NORM;
}


/////// Density /////////
static void Alg_DensityInit( Algorithm alg ){}
static void Alg_DensityDone( Algorithm alg ){}
static double Alg_DensityAlg( Algorithm alg, const BT_Glyph glyph )
{
    const B_Image image = BT_Glyph_getPlain(glyph);
    int width = B_Image_getWidth( image );
    int height = B_Image_getHeight( image );
    int area = width * height;
    int calc = 0;
    for( int r = 0; r < height; r++ )
    {
        for( int c = 0; c < width; c++ )
        {
            if( B_Image_getPixel( image, c, r ) > 128 )
                ++calc;
        }
    }
    area = area>0?area:1;
    double density = (double)calc / (double)area;
    return density;
}
static double Alg_DensityCalc( Algorithm alg, const BT_Face face )
{
    return Alg_CalcAll( alg, face, &Alg_DensityAlg );
}

/////// Slant //////
static int slant_l[] = {
/*
                          -1, -1, -1, -1,  9,
                          -1, -1, -1,  9, -1,
                          -1, -1,  9, -1, -1,
                          -1,  9, -1, -1, -1,
                           9, -1, -1, -1, -1,
*/
                          -1, -1, -1, -1,  2,
                          -1, -1, -1,  2, -1,
                          -1, -1,  2, -1, -1,
                          -1,  2, -1, -1, -1,
                           2, -1, -1, -1, -1,
                        };
#define NUM_SLANT_MASKS 2
static void Alg_SlantInit( Algorithm alg )
{
    alg->slant[0] = B_Mask_new( slant_l, 1, 5, 5 );
    alg->slant[1] = B_Mask_flipHor( alg->slant[0] );
}
static void Alg_SlantDone( Algorithm alg )
{
    B_Mask_delete( alg->slant[0] );
    B_Mask_delete( alg->slant[1] );
}
static double Alg_SlantAlg( Algorithm alg, const BT_Glyph glyph )
{
    const B_Image image = BT_Glyph_getEdge(glyph);
    double score = Alg_calculate( image, alg->slant, NUM_SLANT_MASKS );
    return score;
}
static double Alg_SlantCalc( Algorithm alg, const BT_Face face )
{
    return Alg_CalcAll( alg, face, &Alg_SlantAlg );
}

///////   Curvature  /////////

static int curve_tl[] = {
/*
                          -9, -9, -9,  1,  1,
                          -9, -9,  3,  7,  7,
                          -9,  3,  7,  3, -9,
                           1,  7,  3, -9, -9,
                           1,  7, -9, -9, -9,
*/
                          -9, -9, -9,  0,  0,
                          -9, -9,  0,  5,  5,
                          -9,  0,  5,  0, -9,
                           0,  5,  0, -9, -9,
                           0,  5, -9, -9, -9,
                        };
#define NUM_CURVE_MASKS 4
static void Alg_CurveInit( Algorithm alg )
{
    alg->curve[0] = B_Mask_new( curve_tl, 1, 5, 5 );
    alg->curve[1] = B_Mask_rotate( alg->curve[0] );
    alg->curve[2] = B_Mask_rotate( alg->curve[1] );
    alg->curve[3] = B_Mask_rotate( alg->curve[2] );
}
static void Alg_CurveDone( Algorithm alg )
{
    for( int i = 0; i < NUM_CURVE_MASKS; i++ )
    {
        B_Mask_delete( alg->curve[i] );
    }
}
static double Alg_CurveAlg( Algorithm alg, const BT_Glyph glyph )
{
    // get an edge detected image
    const B_Image image = BT_Glyph_getEdge(glyph);
    double score = Alg_calculate( image, alg->curve, NUM_CURVE_MASKS );
    return score;
}
static double Alg_CurveCalc( Algorithm alg, const BT_Face face )
{
    return Alg_CalcAll( alg, face, &Alg_CurveAlg );
}

///////   Serif   /////////
#define NUM_PROJECTION_MASKS 4
static void Alg_SerifInit( Algorithm alg )
{
}
static void Alg_SerifDone( Algorithm alg )
{
}
/*
static double Alg_SerifAlg( Algorithm alg, const BT_Glyph glyph )
{
    // get an edge detected image
    const B_Image image = BT_Glyph_getChar(glyph);

    return score;
}
*/
static inline int Alg_SerifSumRow( const B_Image image, int row, int width )
{
    int sum = 0;
    for( int col = 0; col < width; col++ )
    {
        sum += B_Image_getPixel( image, col, row );
    }
    return sum;
}
static inline int Alg_SerifSumCol( const B_Image image, int col, int height )
{
    int sum = 0;
    for( int row = 0; row < height; row++ )
    {
        sum += B_Image_getPixel( image, col , row );
    }
    return sum;
}
#define DIFF_THRESHOLD 255
static char Alg_SerifArray[] = {'I','E','H','L'};
static double Alg_SerifCalc( Algorithm alg, const BT_Face face )
{
    // get a delta on row reading an I
    double score = 0;
    for( unsigned int i = 0; i < sizeof(Alg_SerifArray)/sizeof(Alg_SerifArray[0]); i++)
    {
        const B_Image image = BT_Face_getChar(face,Alg_SerifArray[i]);
        int height = B_Image_getHeight( image );
        int width = B_Image_getWidth( image );
        if( height == 0 || width == 0 )
            return score;

        for( int r = 1, prevSum = Alg_SerifSumRow( image, 0, width ); r < height; r++ )
        {
            int sum = Alg_SerifSumRow( image, r, width );

            if( !(prevSum - DIFF_THRESHOLD <= sum && sum <= prevSum + DIFF_THRESHOLD) )
                score += 0.001;

            prevSum = sum;
        }
        for( int c = 1, prevSum = Alg_SerifSumCol( image, 0, height ); c < width; c++ )
        {
            int sum = Alg_SerifSumCol( image, c, height );

            if( !(prevSum - DIFF_THRESHOLD <= sum && sum <= prevSum + DIFF_THRESHOLD) )
                score += 0.001;

            prevSum = sum;
        }
    }
    return score;
}
//////////////////////////////

static Algorithm algorithm = NULL;
static int algRefCount = 0;

static void Alg_init( Algorithm alg )
{
    Alg_WidthInit( alg );
    Alg_HeightInit( alg );
    Alg_AspectRatioInit( alg );
    Alg_xHeightInit( alg );
    Alg_DensityInit( alg );
    Alg_SlantInit( alg );
    Alg_CurveInit( alg );
    Alg_SerifInit( alg );
}

static void Alg_done( Algorithm alg )
{
    Alg_WidthDone( alg );
    Alg_HeightDone( alg );
    Alg_AspectRatioDone( alg );
    Alg_xHeightDone( alg );
    Alg_DensityDone( alg );
    Alg_SlantDone( alg );
    Alg_CurveDone( alg );
    Alg_SerifDone( alg );
}



Algorithm Alg_getInstance(void)
{
    if( !algorithm )
    {
        // new instance!
        algRefCount = 0;
        algorithm = (Algorithm) malloc(sizeof(Algorithm_Rec));
        Alg_init( algorithm );
    }
    ++algRefCount;
    return algorithm;
}

void Alg_doneInstance( Algorithm alg )
{
    // Check if correct pointer
    if( alg != algorithm )
        return;

    if( algRefCount > 0 )
    {
        --algRefCount;
    }

    // delete if no more references
    if( algRefCount == 0 && algorithm )
    {
        Alg_releaseInstance();
    }
}
void Alg_releaseInstance(void)
{
    if( !algorithm )
        return;
    Alg_done( algorithm );
    free(algorithm);
    algorithm = NULL;
    algRefCount = 0;
}

double Alg_calculateMetric( Algorithm alg, const BT_Face face, const Metric metric )
{
    switch( metric )
    {
    case Metric_Width:
        return Alg_WidthCalc( alg, face );
        break;
    case Metric_Height:
        return Alg_HeightCalc( alg, face );
        break;
    case Metric_AspectRatio:
        return Alg_AspectRatioCalc( alg, face );
        break;
    case Metric_xHeight:
        return Alg_xHeightCalc( alg, face );
        break;
    case Metric_Density:
        return Alg_DensityCalc( alg, face );
        break;
    case Metric_Slant:
        return Alg_SlantCalc( alg, face );
        break;
    case Metric_Curve:
        return Alg_CurveCalc( alg, face );
        break;
    case Metric_Serif:
        return Alg_SerifCalc( alg, face );
        break;
    default:
        fprintf(stderr,"Not a valid metric!\n");
        break;
    }
    return 0.0;
}

void Alg_calculateMetrics( Algorithm alg, const BT_Face face, Metrics * results )
{
    //results->metrics[Metric_Serif] = Alg_calculateMetric( alg, face, Metric_Serif );
    for( int i = 0; i < NUM_METRICS; i++ )
    {
        results->metrics[i] = Alg_calculateMetric( alg, face, (Metric) i );
    }
}

void Metrics_fprintHeader( FILE * file )
{
    fprintf(file,"Family Name,Style Name,");
    for( int i = 0; i < NUM_METRICS; i++ )
    {
        fprintf(file,"%s,", Metric_toString((Metric)i));
    }
    fprintf(file,"\n");
}

void Metrics_fprint( FILE * file, const BT_Face face, const Metrics * results )
{
    fprintf(file,"%s,%s,", BT_Face_getFamilyName(face), BT_Face_getStyleName(face));
    for( int i = 0; i < NUM_METRICS; i++ )
    {
        fprintf(file,"%f,", results->metrics[i]);
    }
    fprintf(file,"\n");
}

const char * Metric_toString( Metric metric )
{
    switch( metric )
    {
    case Metric_Width:
        return "Width";
        break;
    case Metric_Height:
        return "Height";
        break;
    case Metric_AspectRatio:
        return "Aspect Ratio";
        break;
    case Metric_xHeight:
        return "x-height";
        break;
    case Metric_Density:
        return "Density";
        break;
    case Metric_Slant:
        return "Slant";
        break;
    case Metric_Curve:
        return "Curve";
        break;
    case Metric_Serif:
        return "Serif";
        break;
    default:
        return "Not a valid metric!";
        break;
    }
    return "";
}

Metric Metric_fromString( const char * name )
{
    if(!strcmp(name,"Width"))
        return Metric_Width;
    if(!strcmp(name,"Height"))
        return Metric_Height;
    if(!strcmp(name,"AspectRatio"))
        return Metric_AspectRatio;
    if(!strcmp(name,"xHeight"))
        return Metric_xHeight;
    if(!strcmp(name,"Density"))
        return Metric_Density;
    if(!strcmp(name,"Slant"))
        return Metric_Slant;
    if(!strcmp(name,"Curve"))
        return Metric_Curve;
    if(!strcmp(name,"Serif"))
        return Metric_Serif;
    return Metric_Error;
}
