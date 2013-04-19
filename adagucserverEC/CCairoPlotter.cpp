/*
 * CCairoPlotter2.cpp
 *
 *  Created on: Nov 11, 2011
 *      Author: vreedede
 */
#include "CCairoPlotter.h"
#ifdef ADAGUC_USE_CAIRO

const char *CCairoPlotter::className="CCairoPlotter";

cairo_status_t writerFunc(void *closure, const unsigned char *data, unsigned int length) {
  FILE *fp=(FILE *)closure;
  int nrec=fwrite(data, length, 1, fp);
  if (nrec==1) {
    return CAIRO_STATUS_SUCCESS;
  }

  return CAIRO_STATUS_WRITE_ERROR;
}

void CCairoPlotter::pixelBlend(int x,int y, unsigned char r,unsigned char g,unsigned char b,unsigned char a){
    if(x<0||y<0)return;
    if(x>=width||y>=height)return;
    float a1=1-(float(a)/255);//*alpha;
    if(a1==0){
      
      size_t p=x*4+y*stride;
      ARGBByteBuffer[p+2]=r;
      ARGBByteBuffer[p+1]=g;
      ARGBByteBuffer[p+0]=b;
      ARGBByteBuffer[p+3]=255;
    }else{
      size_t p=x*4+y*stride;
      // ALpha is increased
      float sf=ARGBByteBuffer[p+3];  
      float alphaRatio=(1-sf/255);
      float tf=sf+a*alphaRatio;if(tf>255)tf=255;  
      if(sf==0.0f)a1=0;
      float a2=1-a1;//1-alphaRatio;
      //CDBDebug("Ratio: a1=%2.2f a2=%2.2f",a1,sf);
      
      float sr=ARGBByteBuffer[p+2];sr=sr*a1+r*a2;if(sr>255)sr=255;
      float sg=ARGBByteBuffer[p+1];sg=sg*a1+g*a2;if(sg>255)sg=255;
      float sb=ARGBByteBuffer[p+0];sb=sb*a1+b*a2;if(sb>255)sb=255;
      ARGBByteBuffer[p+2]=sr;
      ARGBByteBuffer[p+1]=sg;
      ARGBByteBuffer[p+0]=sb;
      ARGBByteBuffer[p+3]=tf;
      
    }
    
  }
  
    void CCairoPlotter::pixel(int x,int y, unsigned char r,unsigned char g,unsigned char b,unsigned char a){
    if(x<0||y<0)return;
    if(x>=width||y>=height)return;
    size_t p=x*4+y*stride;
    if(a!=255){
    ARGBByteBuffer[p]=(float(b)/256.0)*float(a);
    ARGBByteBuffer[p+1]=(float(g)/256.0)*float(a);
    ARGBByteBuffer[p+2]=(float(r)/256.0)*float(a);
    ARGBByteBuffer[p+3]=a;
    }else{
      ARGBByteBuffer[p]=b;
    ARGBByteBuffer[p+1]=g;
    ARGBByteBuffer[p+2]=r;
    ARGBByteBuffer[p+3]=255;
    }
  }
#endif
