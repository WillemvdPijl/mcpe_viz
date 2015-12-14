/*
  Minecraft Pocket Edition (MCPE) World File Visualization & Reporting Tool
  (c) Plethora777, 2015.9.26

  GPL'ed code - see LICENSE

  utility code for mcpe_viz -- stuff that is not leveldb/mcpe specific
*/

#ifndef __MCPE_VIZ_UTIL_H__
#define __MCPE_VIZ_UTIL_H__

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <string.h>
#include <png.h>
#include <sys/stat.h>
#include <libgen.h>
#include <math.h>
#include "mcpe_viz.version.h"

namespace mcpe_viz {

#ifndef htobe32
  int32_t local_htobe32(const int32_t src);
#define htobe32 local_htobe32
#endif

#ifndef be32toh
  int32_t local_be32toh(const int32_t src);
#define be32toh local_be32toh
#endif

  int local_mkdir(std::string path);
  
  // these hacks work around "const char*" problems
  std::string mybasename( const std::string fn );
  std::string mydirname( const std::string fn );
  
  int file_exists(const char* fn);
  
  std::string escapeString(const std::string& s, const std::string& escapeChars);

  std::string makeIndent(int indent, const char* hdr);
  
  typedef std::vector< std::pair<std::string, std::string> > StringReplacementList;
  int copyFileWithStringReplacement ( const std::string fnSrc, const std::string fnDest,
				      const StringReplacementList& replaceStrings );
  
  int copyFile ( const std::string fnSrc, const std::string fnDest );

  int copyDirToDir ( const std::string dirSrc, const std::string dirDest );

  int deleteFile ( const std::string fn );
  
  bool vectorContains( const std::vector<int> &v, int i );

  

  enum LogType : int32_t {
    // todobig - be more clever about this
    kLogInfo1 = 0x0001,
      kLogInfo2 = 0x0002,
      kLogInfo3 = 0x0004,
      kLogInfo4 = 0x0008,

      kLogInfo = 0x0010,
      kLogWarning = 0x0020,
      kLogError = 0x0040,
      kLogFatalError = 0x0080,
      kLogDebug = 0x1000,
      kLogAll = 0xffff
      };

  const int32_t kLogVerbose = kLogAll ^ (kLogDebug);
  const int32_t kLogDefault = kLogVerbose;
  const int32_t kLogQuiet = ( kLogWarning | kLogError | kLogFatalError );

  
  
  class Logger {
  public:
    int32_t logLevelMask;
    FILE *fpStdout;
    FILE *fpStderr;
    bool doFlushFlag;
    
    Logger() {
      init();
    }
   
    void init() {
      logLevelMask = kLogDefault;
      fpStdout = nullptr;
      fpStderr = nullptr;
    }

    void setFlush(bool f) {
      doFlushFlag = f;
    }
    
    void setLogLevelMask(int32_t m) {
      logLevelMask = m;
    }

    void setStdout(FILE *fp) {
      fpStdout = fp;
    }

    void setStderr(FILE *fp) {
      fpStderr = fp;
    }
    
    // from: http://stackoverflow.com/questions/12573968/how-to-use-gccs-printf-format-attribute-with-c11-variadic-templates
    // make gcc check calls to this function like it checks printf et al
    __attribute__((format(printf, 3, 4)))
      int msg (int levelMask, const char *fmt, ...) {
      // check if we care about this message
      if ( (levelMask & logLevelMask) || (levelMask & kLogFatalError) ) {
	// we care
      } else {
	// we don't care
	return -1;
      }
      
      // todobig - be more clever about logging - if very important or interesting, we might want to print to log AND to stderr
      FILE *fp = fpStdout;

      if ( fp == nullptr ) {
	// todobig - output to screen?
	return -1;
      }
      
      if (levelMask & kLogFatalError)   { fprintf(fp,"** FATAL ERROR: "); }
      else if (levelMask & kLogError)   { fprintf(fp,"ERROR: "); }
      else if (levelMask & kLogWarning) { fprintf(fp,"WARNING: "); }
      else if (levelMask & kLogInfo)    { } // fprintf(fp,"INFO: "); }
      else                              { } // fprintf(fp,"UNKNOWN: "); }
      
      va_list argptr;
      va_start(argptr,fmt);
      vfprintf(fp,fmt,argptr);
      va_end(argptr);
      
      if (levelMask & kLogFatalError) {
	fprintf(fp,"** Exiting on FATAL ERROR\n");
	fflush(fp);
	exit(-1);
      }

      if ( doFlushFlag ) {
	fflush(fp);
      }
      
      return 0;
    }
    
  };    

  extern Logger slogger;
  

  
  class PngWriter {
  public:
    std::string fn;
    FILE *fp;
    png_structp png;
    png_infop info;
    png_bytep *row_pointers;
    bool openFlag;

    PngWriter() {
      fn = "";
      fp = nullptr;
      png = nullptr;
      info = nullptr;
      row_pointers = nullptr;
      openFlag = false;
    }

    ~PngWriter() {
      close();
    }

    int init(const std::string xfn, const std::string imageDescription, int w, int h, int numRowPointers, bool rgbaFlag) {
      fn = std::string(xfn);
      return open(imageDescription,w,h,numRowPointers,rgbaFlag);
    }

    int open(const std::string imageDescription, int width, int height, int numRowPointers, bool rgbaFlag) {
      fp = fopen(fn.c_str(), "wb");
      if(!fp) {
	slogger.msg(kLogInfo1,"ERROR: Failed to open output file (%s)\n", fn.c_str());
	return -1;
      }
	
      // todo - add handlers for warn/err etc?
      /*
	png_structp   png_create_write_struct   (
	png_const_charp   user_png_ver,  
	png_voidp  error_ptr,  
	png_error_ptr  error_fn, 
	png_error_ptr warn_fn);
      */
      png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
      if (!png) {
	slogger.msg(kLogInfo1,"ERROR: Failed png_create_write_struct\n");
	fclose(fp);
	return -2;
      }
	
      info = png_create_info_struct(png);
      if (!info) {
	slogger.msg(kLogInfo1,"ERROR: Failed png_create_info_struct\n");
	fclose(fp);
	return -2;
      }
	
      // todobig - can we do something more clever here?
      if (setjmp(png_jmpbuf(png))) abort();
	
      png_init_io(png, fp);
	
      // Output is 8bit depth, RGB format.
      int color_type = PNG_COLOR_TYPE_RGB;
      if ( rgbaFlag ) {
	color_type = PNG_COLOR_TYPE_RGB_ALPHA;
      }
      png_set_IHDR(
		   png,
		   info,
		   width, height,
		   8,
		   color_type,
		   PNG_INTERLACE_NONE,
		   PNG_COMPRESSION_TYPE_DEFAULT,
		   PNG_FILTER_TYPE_DEFAULT
		   );

      // note: the following two options have a *significant* impact on speed with a minimal cost in filesize
      //png_set_compression_level(png, Z_BEST_COMPRESSION);
      png_set_compression_level(png, 1); // speed!
      png_set_filter(png, 0, PNG_FILTER_NONE);

      // add text comments to png
      addText("Program", mcpe_viz_version);
      addText("Description", imageDescription);
      addText("URL", "https://github.com/Plethora777/mcpe_viz");
      // todo - other text?
      
      png_write_info(png, info);
	
      row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * numRowPointers);
      openFlag = true;
      return 0;
    }

    int close() {
      if ( fp != nullptr && openFlag ) {
	png_write_end(png, info);
	png_destroy_write_struct(&png, &info);
	free(row_pointers);
	fclose(fp);
	fp = nullptr;
      }
      openFlag = false;
      return 0;
    }

    int addText(const std::string key, const std::string val) {
      png_text text[1];
      char* skey = new char[key.size()+1];
      strcpy(skey, key.c_str());
      char* sval = new char[val.size()+1];
      strcpy(sval, val.c_str());

      text[0].compression = PNG_TEXT_COMPRESSION_NONE;
      text[0].key = skey;
      text[0].text = sval;
      png_set_text(png, info, text, 1);

      delete [] skey;
      delete [] sval;
      return 0;
    }      
  };



  class PngReader {
  public:
    std::string fn;
    FILE *fp;
    png_structp png;
    png_infop info;
    png_infop end_info;
    png_bytepp row_pointers;
    bool openFlag;

    PngReader() {
      fn = "";
      fp = nullptr;
      png = nullptr;
      info = nullptr;
      row_pointers = nullptr;
      openFlag = false;
    }

    ~PngReader() {
      close();
    }

    int init(const std::string xfn) {
      fn = std::string(xfn);
      return open();
    }

    int open() {
      fp = fopen(fn.c_str(), "rb");
      if(!fp) {
	slogger.msg(kLogInfo1,"ERROR: Failed to open output file (%s)\n", fn.c_str());
	return -1;
      }
	
      // todo - add handlers for warn/err etc?
      /*
	png_structp   png_create_write_struct   (
	png_const_charp   user_png_ver,  
	png_voidp  error_ptr,  
	png_error_ptr  error_fn, 
	png_error_ptr warn_fn);
      */
      png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
      if (!png) {
	slogger.msg(kLogInfo1,"ERROR: Failed png_create_write_struct\n");
	fclose(fp);
	return -2;
      }

      info = png_create_info_struct(png);
      if (!info) {
	slogger.msg(kLogInfo1,"ERROR: Failed png_create_info_struct (info)\n");
	fclose(fp);
	png_destroy_read_struct(&png,
				(png_infopp)NULL, (png_infopp)NULL);
	return -3;
      }
      
      end_info = png_create_info_struct(png);
      if (!end_info) {
	slogger.msg(kLogInfo1,"ERROR: Failed png_create_info_struct (end_info)\n");
	fclose(fp);
	png_destroy_read_struct(&png, &info,(png_infopp)NULL);
	return -4;
      }
      
      // todobig - can we do something more clever here?
      if (setjmp(png_jmpbuf(png))) {
	slogger.msg(kLogInfo1,"ERROR: PngReader setjmp triggered\n");
	png_destroy_read_struct(&png, &info, &end_info);
	fclose(fp);
	return -5;
      }
	
      png_init_io(png, fp);
	
      openFlag = true;
      return 0;
    }

    int getWidth() {
      return png_get_image_width(png,info);
    }
    int getHeight() {
      return png_get_image_height(png,info);
    }
    int getColorType() {
      return png_get_color_type(png,info);
    }
    
    int read() {
      png_read_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
      row_pointers = png_get_rows(png, info);
      return 0;
    }

    int close() {
      if ( fp != nullptr && openFlag ) {
	// png_read_end(png, end_info);
	png_destroy_read_struct(&png, &info, &end_info);
	fclose(fp);
	fp = nullptr;
      }
      openFlag = false;
      return 0;
    }
  };



  class PngTiler {
  public:
    std::string filename;
    int tileWidth;
    int tileHeight;
    std::string dirOutput;
	
    PngTiler(const std::string fn, int tileW, int tileH, const std::string dirOut) {
      filename = fn;
      tileWidth = tileW;
      tileHeight = tileH;
      dirOutput = dirOut;
    }

    int doTile() {
      // todobig - store tile filenames?

      char tmpstring[256];

      // open source file
      PngReader pngSrc;
      pngSrc.init(filename);
      pngSrc.read();

      int srcW = pngSrc.getWidth();
      int srcH = pngSrc.getHeight();
      int colorType = pngSrc.getColorType();
      bool rgbaFlag = false;
      int bpp = 3;
      if ( colorType == PNG_COLOR_TYPE_RGB_ALPHA ) {
	bpp = 4;
	rgbaFlag = true;
      }
      int numPngW = (int)ceil((double)srcW / (double)tileWidth);

      PngWriter *pngOut = new PngWriter[numPngW];
      uint8_t **buf;
      buf = new uint8_t*[numPngW];
      for (int i=0; i < numPngW; i++) {
	buf[i] = new uint8_t[tileWidth * tileHeight * bpp];
      }
	
      bool initPngFlag = false;
      int tileCounterY=0;

      for (int sy=0; sy < srcH; sy++) {

	// initialize png helpers
	if ( ! initPngFlag ) {
	  initPngFlag = true;
	  for (int i=0; i < numPngW; i++) {
	    sprintf(tmpstring,"%s/%s.%d.%d.png", dirOutput.c_str(), mybasename(filename).c_str(),
		    tileCounterY, i);
	    std::string fname = tmpstring;
	    pngOut[i].init(fname, "MCPE Viz Image Tile", tileWidth, tileHeight, tileHeight, rgbaFlag);

	    // clear buffer
	    memset(&buf[i][0], 0, tileWidth * tileHeight * bpp);
	      
	    // setup row_pointers
	    for (int ty=0; ty < tileHeight; ty++) {
	      pngOut[i].row_pointers[ty] = &buf[i][ty*tileWidth * bpp];
	    }
	  }
	  tileCounterY++;
	}

	uint8_t *srcbuf = pngSrc.row_pointers[sy];

	int tileOffsetY = sy % tileHeight;
	  
	// todobig - step in tileWidth and memcpy as we go - need to check the last one for out of bounds
	for (int sx=0; sx < srcW; sx++) {
	  int tileCounterX = sx / tileWidth;
	  int tileOffsetX = sx % tileWidth;
	  memcpy(&buf[tileCounterX][((tileOffsetY * tileWidth) + tileOffsetX) * bpp], &srcbuf[sx*bpp], bpp);
	}
	  
	// write tile png files when they are ready
	if ( ((sy+1) % tileHeight) == 0 ) {
	  // write pngs
	  for (int i=0; i < numPngW; i++) {
	    png_write_image(pngOut[i].png, pngOut[i].row_pointers);
	    pngOut[i].close();
	  }
	  initPngFlag = false;
	}
      }

      // close final tiles
      if ( initPngFlag ) {
	// write pngs
	for (int i=0; i < numPngW; i++) {
	  png_write_image(pngOut[i].png, pngOut[i].row_pointers);
	  pngOut[i].close();
	}
      }

      delete [] pngOut;

      for (int i=0; i < numPngW; i++) {
	delete [] buf[i];
      }
      delete [] buf;

      pngSrc.close();
	
      return 0;
    }
      
  };


  int oversampleImage(const std::string fnSrc, const std::string fnDest, int oversample);
  
  
  int rgb2hsb(int32_t red, int32_t green, int32_t blue, double& hue, double& saturation, double &brightness);
    
  int hsl2rgb ( double h, double s, double l, int32_t &r, int32_t &g, int32_t &b );
  
  int makeHslRamp ( int32_t *pal, int32_t start, int32_t stop, double h1, double h2, double s1, double s2, double l1, double l2 );



  class ColorInfo {
  public:
    std::string name;
    int32_t color;
    int r,g,b;
    double h, s, l;
      
    ColorInfo(std::string n, int32_t c) {
      name = std::string(n);
      color = c;
      calcHSL();
    }

    ColorInfo& operator=(const ColorInfo& other) = default;

    int calcHSL() {
      r = (color & 0xFF0000) >> 16;
      g = (color & 0xFF00) >> 8;
      b = color & 0xFF;
      h = s = l = 0.0;
      rgb2hsb(r,g,b, h,s,l);
      //rgb2hsv(r,g,b, h,s,l);

      // todo - we could adjust some items to help with the sort
      //if ( s < 0.2 ) { s=0.0; }
	
      return 0;
    }

    std::string toHtml() const {
      char tmpstring[256];
      std::string ret = "<div class=\"colorBlock";
      if ( l < 0.2 ) {
	ret += " darkBlock";
      }
      ret += "\" style=\"background-color:";
      sprintf(tmpstring,"#%02x%02x%02x",r,g,b);
      ret += tmpstring;
      ret += "\">" + name + " (";
      sprintf(tmpstring,"0x%06x",color);
      ret += tmpstring;
      ret += ") ";
      sprintf(tmpstring,"[ h=%lf s=%lf l=%lf ]", h, s, l);
      ret += tmpstring;
      ret += "</div>\n";
      return ret;
    }
  };
    
  bool compareColorInfo(std::unique_ptr<ColorInfo> const& a, std::unique_ptr<ColorInfo> const& b);



  // quick-n-dirty emulation of java random number generator
  // details from: https://docs.oracle.com/javase/8/docs/api/java/util/Random.html
  class JavaRandom {
  public:
    int64_t seed;
    
    void setSeed(int64_t newseed) {
      seed = (newseed ^ (int64_t)0x5DEECE66D) & (((int64_t)1 << 48) - 1);
    }
    
    int32_t next(int64_t bits) {
      // seed = (seed * 0x5DEECE66DL + 0xBL) & ((1L << 48) - 1)
      seed = (seed * (int64_t)0x5DEECE66D + (int64_t)0xB) & (((int64_t)1 << 48) - 1);
      
      // return (int)(seed >>> (48 - bits)).
      // todo - is this good enough for an unsigned shift right?
      int64_t ret = (uint64_t)seed >> (48 - bits);
      
      return (int32_t)ret;
    }
    
    int32_t nextInt(int64_t bound) {
      if ((bound & -bound) == bound)  // i.e., bound is a power of 2
	return ((bound * this->next(31)) >> 31);
      
      int32_t bits, val;
      do {
	bits = this->next(31);
	val = bits % bound;
      } while (bits - val + (bound - 1) < 0);
      
      return val;
    }
  };


  
  // todobig - template this for diff types?
  typedef std::pair<int32_t, int32_t> HistogramItem;
  typedef std::map<int32_t, int32_t> HistogramMap;
  typedef std::vector< HistogramItem > HistogramVector;
  class Histogram {
  public:
    HistogramMap map;
    
    bool has_key(int32_t k) {
      return  map.find(k) != map.end();
    }
    
    void add(int32_t k) {
      if ( has_key(k) ) {
	map[k]++;
      } else {
	map[k] = 1;
      }
    }

    int32_t getTotal() {
      int32_t total=0;
      for (auto& it : map) {
	total += it.second;
      }
      return total;
    }
    
    HistogramVector sort(int32_t order) {
      HistogramVector vector(map.begin(), map.end());
      
      if ( order <= 0 ) {
	std::sort(vector.begin(), vector.end(), compare_less_());
      } else {
	std::sort(vector.begin(), vector.end(), compare_more_());
      }

      return vector;
    }
    
  private:
    struct compare_less_
      : std::binary_function<HistogramItem,HistogramItem,bool>
    {
      inline bool operator()(const HistogramItem& lhs, const HistogramItem& rhs) {
	return lhs.second < rhs.second;
      }
    };
    struct compare_more_
      : std::binary_function<HistogramItem,HistogramItem,bool>
    {
      inline bool operator()(const HistogramItem& lhs, const HistogramItem& rhs) {
	return lhs.second > rhs.second;
      }
    };
  };

} // namespace mcpe_viz

#endif // __MCPE_VIZ_UTIL_H__
