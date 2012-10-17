#include "CImgWarpBilinear.h"
#include <gd.h>

const char *CImgWarpBilinear::className="CImgWarpBilinear";
void CImgWarpBilinear::render(CImageWarper *warper,CDataSource *sourceImage,CDrawImage *drawImage){
  #ifdef CImgWarpBilinear_DEBUG
  CDBDebug("Render");
  #endif
  int dImageWidth=drawImage->Geo->dWidth+1;
  int dImageHeight=drawImage->Geo->dHeight+1;
  
  double dfSourceExtW=(sourceImage->dfBBOX[2]-sourceImage->dfBBOX[0]);
  double dfSourceExtH=(sourceImage->dfBBOX[1]-sourceImage->dfBBOX[3]);
  double dfSourceW = double(sourceImage->dWidth);
  double dfSourceH = double(sourceImage->dHeight);
  double dfSourcedExtW=dfSourceExtW/dfSourceW;
  double dfSourcedExtH=dfSourceExtH/dfSourceH;
  double dfSourceOrigX=sourceImage->dfBBOX[0];
  double dfSourceOrigY=sourceImage->dfBBOX[3];
  
  double dfDestExtW = drawImage->Geo->dfBBOX[2]-drawImage->Geo->dfBBOX[0];
  double dfDestExtH = drawImage->Geo->dfBBOX[1]-drawImage->Geo->dfBBOX[3];
  double dfDestOrigX = drawImage->Geo->dfBBOX[0];
  double dfDestOrigY = drawImage->Geo->dfBBOX[3];
  double dfDestW = double(dImageWidth);
  double dfDestH = double(dImageHeight);
  double hCellSizeX = (dfSourceExtW/dfSourceW)/2.0f;
  double hCellSizeY = (dfSourceExtH/dfSourceH)/2.0f;
  double dfPixelExtent[4];
  int dPixelExtent[4];
  bool tryToOptimizeExtent=false;
  
  //  CDBDebug("enableBarb=%d enableVectors=%d drawGridVectors=%d", enableBarb, enableVector, drawGridVectors);
  if(tryToOptimizeExtent){
    //Reproject the boundingbox from the destination bbox: 
    for(int j=0;j<4;j++)dfPixelExtent[j]=drawImage->Geo->dfBBOX[j];  
    #ifdef CImgWarpBilinear_DEBUG
    for(int j=0;j<4;j++){
      CDBDebug("dfPixelExtent: %d %f",j,dfPixelExtent[j]);
    }
    #endif   
    //warper->findExtent(sourceImage,dfPixelExtent);
    warper->reprojBBOX(dfPixelExtent);
    
    //Convert the bbox to source image pixel extent
    dfPixelExtent[0]=((dfPixelExtent[0]-dfSourceOrigX)/dfSourceExtW)*dfSourceW;
    dfPixelExtent[1]=((dfPixelExtent[1]-dfSourceOrigY)/dfSourceExtH)*dfSourceH;
    dfPixelExtent[2]=((dfPixelExtent[2]-dfSourceOrigX)/dfSourceExtW)*dfSourceW+2;
    dfPixelExtent[3]=((dfPixelExtent[3]-dfSourceOrigY)/dfSourceExtH)*dfSourceH+3;
    
    //Make sure the images are not swapped in Y dir.
    if(dfPixelExtent[1]>dfPixelExtent[3]){
      float t1=dfPixelExtent[1];dfPixelExtent[1]=dfPixelExtent[3];dfPixelExtent[3]=t1;
    }
    
    //Make sure the images are not swapped in X dir.
    if(dfPixelExtent[0]>dfPixelExtent[2]){
      float t1=dfPixelExtent[0];dfPixelExtent[0]=dfPixelExtent[2];dfPixelExtent[2]=t1;
    }
    
    //Convert the pixel extent to integer values
    //Also stretch the BBOX a bit, to hide edge effects
    
    dPixelExtent[0]=int(dfPixelExtent[0]);
    dPixelExtent[1]=int(dfPixelExtent[1]);
    dPixelExtent[2]=int(dfPixelExtent[2]);
    dPixelExtent[3]=int(dfPixelExtent[3]);
    //Return if we are zoomed in to an infinite area
    if(dPixelExtent[0]==dPixelExtent[2]){
      CDBDebug("Infinite area, dPixelExtent[0]==dPixelExtent[2]: %d==%d, stopping.... ",dPixelExtent[0],dPixelExtent[2]);
      return;
    }
    if(dPixelExtent[1]==dPixelExtent[3]){
      CDBDebug("Infinite area, dPixelExtent[1]==dPixelExtent[3]: %d==%d, stopping.... ",dPixelExtent[1],dPixelExtent[3]);
      return;
    }
    
    dPixelExtent[0]-=2;
    dPixelExtent[1]-=4;
    dPixelExtent[2]+=2;
    dPixelExtent[3]+=2;
    //Extent should lie within the source image size
    if(dPixelExtent[0]<0)dPixelExtent[0]=0;
    if(dPixelExtent[1]<0)dPixelExtent[1]=0;
    if(dPixelExtent[2]>sourceImage->dWidth)dPixelExtent[2]=sourceImage->dWidth;
    if(dPixelExtent[3]>sourceImage->dHeight)dPixelExtent[3]=sourceImage->dHeight;
  }else{
    dPixelExtent[0]=0;
    dPixelExtent[1]=0;
    dPixelExtent[2]=sourceImage->dWidth;
    dPixelExtent[3]=sourceImage->dHeight;
  }
  
  
  //Get width and height of the pixel extent
  int dPixelDestW=dPixelExtent[2]-dPixelExtent[0];
  int dPixelDestH=dPixelExtent[3]-dPixelExtent[1];
  size_t numDestPixels = dPixelDestW*dPixelDestH;
  
  //TODO increase field resolution in order to create better contour plots.
  
  //Allocate memory
  #ifdef CImgWarpBilinear_DEBUG
  CDBDebug("Allocate, numDestPixels %d x %d",dPixelDestW,dPixelDestH);
  #endif
  int *dpDestX = new int[numDestPixels];//refactor to numGridPoints
  int *dpDestY = new int[numDestPixels];
  class ValueClass{
  public:
    ValueClass(){
      fpValues=NULL;
      valueData=NULL;
    }
    ~ValueClass(){
      if(fpValues!=NULL){delete[] fpValues;fpValues=NULL;}
      if(valueData!=NULL){delete[] valueData;valueData=NULL;}
    }
    float *fpValues;
    float *valueData;
  };
  ValueClass *valObj = new ValueClass[sourceImage->dataObject.size()];
  for(size_t dNr=0;dNr<sourceImage->dataObject.size();dNr++){
    #ifdef CImgWarpBilinear_DEBUG
    CDBDebug("Allocating valObj[%d].fpValues: numDestPixels %d x %d",dNr,dPixelDestW,dPixelDestH);
    CDBDebug("Allocating valObj[%d].valueData: imageSize %d x %d",dNr,dImageWidth,dImageHeight);
    #endif
    valObj[dNr].fpValues = new float [numDestPixels];
    valObj[dNr].valueData = new float[dImageWidth*dImageHeight];
  }
  
  if(!sourceImage->dataObject[0]->hasNodataValue){
    /* When the datasource has no nodata value, assign -9999.0f */
    #ifdef CImgWarpBilinear_DEBUG
    CDBDebug("Source image has no NoDataValue, assigning -9999.0f");
    #endif
    sourceImage->dataObject[0]->dfNodataValue=-9999.0f;
    sourceImage->dataObject[0]->hasNodataValue=true;
  }else{
    /* Create a real nodata value instead of a nanf. */
    
    if(!(sourceImage->dataObject[0]->dfNodataValue==sourceImage->dataObject[0]->dfNodataValue)){
      #ifdef CImgWarpBilinear_DEBUG
      CDBDebug("Source image has no nodata value NaNf, changing this to -9999.0f");
      #endif
      sourceImage->dataObject[0]->dfNodataValue=-9999.0f;
    }
  }
  //Get the nodatavalue
  float fNodataValue = sourceImage->dataObject[0]->dfNodataValue ;
  
  //Reproject all the points 
  #ifdef CImgWarpBilinear_DEBUG
  CDBDebug("Nodata value = %f",fNodataValue);
  
  StopWatch_Stop("Start Reprojecting all the points");
  char temp[32];
  CDF::getCDFDataTypeName(temp,31,sourceImage->dataObject[0]->cdfVariable->type);
  CDBDebug("datatype: %s",temp);
  for(int j=0;j<4;j++){
    CDBDebug("dPixelExtent[%d]=%d",j,dPixelExtent[j]);
  }
  #endif
  
  for(int y=dPixelExtent[1];y<dPixelExtent[3];y++){
    for(int x=dPixelExtent[0];x<dPixelExtent[2];x++){
      size_t p = size_t((x-(dPixelExtent[0]))+((y-(dPixelExtent[1]))*dPixelDestW));
      double destX,destY;
      destX=dfSourcedExtW*double(x)+dfSourceOrigX;
      destY=dfSourcedExtH*double(y)+dfSourceOrigY;
      destX+=hCellSizeX;
      destY+=hCellSizeY;
      //CDBDebug("%f - %f",destX,destY);
      //TODO:
      //int status = 
      warper->reprojpoint_inv(destX,destY);
      
      destX-=dfDestOrigX;
      destY-=dfDestOrigY;
      destX/=dfDestExtW;
      destY/=dfDestExtH;
      destX*=dfDestW;
      destY*=dfDestH;
      dpDestX[p]=(int)destX;//2-200;
      dpDestY[p]=(int)destY;//2+200;
      //CDBDebug("%f - %f s:%d x:%d  y:%d  p:%d",destX,destY,status,x,y,p);
      //  drawImage->setPixelIndexed(dpDestX[p],dpDestY[p],240);
      for(size_t varNr=0;varNr<sourceImage->dataObject.size();varNr++){
        void *data=sourceImage->dataObject[varNr]->cdfVariable->data;
        float *fpValues=valObj[varNr].fpValues;
        
        switch(sourceImage->dataObject[varNr]->cdfVariable->type){
          case CDF_CHAR:
            fpValues[p]= ((signed char*)data)[x+y*sourceImage->dWidth];
            break;
          case CDF_BYTE:
            fpValues[p]= ((signed char*)data)[x+y*sourceImage->dWidth];
            break;
          case CDF_UBYTE:
            fpValues[p]= ((unsigned char*)data)[x+y*sourceImage->dWidth];
            break;
          case CDF_SHORT:
            fpValues[p]= ((signed short*)data)[x+y*sourceImage->dWidth];
            break;
          case CDF_USHORT:
            fpValues[p]= ((unsigned short*)data)[x+y*sourceImage->dWidth];
            break;
          case CDF_INT:
            fpValues[p]= ((signed int*)data)[x+y*sourceImage->dWidth];
            break;
          case CDF_UINT:
            fpValues[p]= ((unsigned int*)data)[x+y*sourceImage->dWidth];
            break;
          case CDF_FLOAT:
            fpValues[p]= ((float*)data)[x+y*sourceImage->dWidth];
            break;
          case CDF_DOUBLE:
            fpValues[p]= ((double*)data)[x+y*sourceImage->dWidth];
            break;
        }
        if(!(fpValues[p]==fpValues[p]))fpValues[p]=fNodataValue;
        //if(valObj[0].fpValues[p]==fNodataValue)valObj[0].fpValues[p]=0;
        //valObj[0].fpValues[p]+=10.0f;
      }
    }
  }
  #ifdef CImgWarpBilinear_DEBUG
  StopWatch_Stop("reprojection finished");
  #endif
  
  //return;
  //If there are 2 components, we have wind u and v.
  //Use Jacobians for rotating u and v
  //After calculations 
  bool gridRelative=true;
  
  //TODO this should be enabled when U and V components are available. otherwise it should do nothing.
  if (sourceImage->dataObject.size()>1&&(enableVector||enableBarb)){
    // Check standard_name/var_name for first vector component
    // if x_wind/grid_east_wind of y_wind/grid_northward_wind then gridRelative=true
    // if eastward_wind/northward_wind then gridRelative=false
    // default is gridRelative=true
    CT::string standard_name;
    standard_name=sourceImage->dataObject[0]->variableName;
    try {
      sourceImage->dataObject[0]->cdfVariable->getAttribute("standard_name")->getDataAsString(&standard_name);
    } catch (CDFError e) {}
    if (standard_name.equals("x_wind")||standard_name.equals("grid_eastward_wind")||
      standard_name.equals("y_wind")||standard_name.equals("grid_northward_wind")) {
      gridRelative=true;
      } else {
        gridRelative=false;
      }
      CDBDebug("Grid propery gridRelative=%d", gridRelative);
    
    #ifdef CImgWarpBilinear_DEBUG
      StopWatch_Stop("u/v rotation started");
      #endif
      double delta=0.01;
      double deltaLon;
      double deltaLat;
      float *uValues=valObj[0].fpValues;
      float *vValues=valObj[1].fpValues;
      CDBDebug("Data raster: %f,%f with %f,%f (%f,%f) ll: (%d,%d) ur: (%d,%d) [%d,%d]\n", 
               dfSourceOrigX, dfSourceOrigY, dfSourcedExtW, dfSourcedExtH,
               dfSourceExtW, dfSourceExtH,
               dPixelExtent[0], dPixelExtent[1], dPixelExtent[2], dPixelExtent[3],
               dPixelDestW, dPixelDestH);
      if (dfSourcedExtH<0){
        deltaLat=-delta;
      } else {
        deltaLat=+delta;
      }
      if (dfSourcedExtW>0) {
        deltaLon=delta;
      } else {
        deltaLon=-delta;
      }
      deltaLat=+delta; //TODO Check this (and delete previous 2 if blocks)
      deltaLon=delta;
      double signLon=(dfSourceExtW<0)?-1:1; // sign for adaptation of Jacobian to grid organisation
      double signLat=(dfSourceExtH<0)?-1:1; // sign for adaptation of Jacobian to grid organisation
      CDBDebug("deltaLon %f deltaLat %f signLon %f signLat %f", deltaLon, deltaLat, signLon, signLat);
      
      for(int y=dPixelExtent[1];y<dPixelExtent[3];y=y+1){
        for(int x=dPixelExtent[0];x<dPixelExtent[2];x=x+1){
          size_t p = size_t((x-(dPixelExtent[0]))+((y-(dPixelExtent[1]))*dPixelDestW));
          if ((uValues[p]!=fNodataValue)&&(vValues[p]!=fNodataValue)) {
            double modelX, modelY;
            if (x==dPixelExtent[2]-1) {
              modelX=dfSourcedExtW*double(x-1)+dfSourceOrigX;
            } else {
              modelX=dfSourcedExtW*double(x)+dfSourceOrigX;
            }  
            if (y==dPixelExtent[3]-1) {
              modelY=dfSourcedExtH*double(y-1)+dfSourceOrigY;
            } else {
              modelY=dfSourcedExtH*double(y)+dfSourceOrigY;
            }
            double modelXLat, modelYLat;
            double modelXLon, modelYLon;
            double VJaa,VJab,VJba,VJbb;


//            uValues[p]=0; 
//            vValues[p]=6;
            if (gridRelative) {
              modelXLon=modelX+dfSourcedExtW;
              modelYLon=modelY;
              modelXLat=modelX;
              modelYLat=modelY+dfSourcedExtH;
              //              if (y==0) { CDBDebug("modelXY[%d,%d] {%d} (%f, %f) (%f,%f) (%f,%f) =>", x, y, gridRelative, modelX, modelY, modelXLon, modelYLon, modelXLat, modelYLat);}
              warper->reprojpoint_inv(modelX,modelY); // model to vis proj.
              warper->reprojpoint_inv(modelXLon, modelYLon);
              warper->reprojpoint_inv(modelXLat, modelYLat);
              //              if (y==0) { CDBDebug("modelXY[%d,%d] {%d} (%f, %f) (%f,%f) (%f,%f) =>", x, y, gridRelative, modelX, modelY, modelXLon, modelYLon, modelXLat, modelYLat);}
              double distLon=hypot(modelXLon-modelX, modelYLon-modelY);
              double distLat=hypot(modelXLat-modelX, modelYLat-modelY);
              //          if (y==0) { CDBDebug("(%f, %f) (%f,%f) (%f,%f): %f, %f {%d}\n", modelX,modelY, modelXLon, modelYLon, modelXLat, modelYLat, distLon, distLat, gridRelative); }
              
              VJaa=signLon*(modelXLon-modelX)/distLon;
              VJab=signLon*(modelXLat-modelX)/distLat;
              VJba=signLat*(modelYLon-modelY)/distLon;
              VJbb=signLat*(modelYLat-modelY)/distLat;
              double magnitude=hypot(uValues[p], vValues[p]);
              double uu;
              double vv;
              uu = VJaa*uValues[p]+VJab*vValues[p];
              vv = VJba*uValues[p]+VJbb*vValues[p];
              
              //           if (y==0) {CDBDebug("(%f, %f) ==> (%f,%f)", uValues[p], vValues[p], uu, vv);}
              double newMagnitude = hypot(uu, vv);
              uValues[p]=uu*magnitude/newMagnitude;
              vValues[p]=vv*magnitude/newMagnitude;
              //           if (y==0) {CDBDebug("==> (%f,%f)",uValues[p], vValues[p]);}  
              
            } else {
              warper->reprojModelToLatLon(modelX, modelY); // model to latlon proj.
              modelXLon=modelX+deltaLon; // latlons
              modelYLon=modelY;
              modelXLat=modelX;
              modelYLat=modelY+deltaLat;
              //            if (y==0) { CDBDebug("modelXY[%d,%d] {%d} {%f,%f} (%f,%f) (%f,%f) =>", x, y, gridRelative, modelX, modelY, modelXLon, modelYLon, modelXLat, modelYLat);}
              
//              if (y==0) { CDBDebug("modelXY[%d,%d] {%d} {%f,%f} (%f,%f) (%f,%f) =>", x, y, gridRelative, modelX, modelY, modelXLon, modelYLon, modelXLat, modelYLat);}
              warper->reprojfromLatLon(modelX, modelY); // latlon to vis proj.
              warper->reprojfromLatLon(modelXLon, modelYLon);
              warper->reprojfromLatLon(modelXLat, modelYLat);
              
//              if (y==0) { CDBDebug("modelXY[%d,%d] {%d} {%f,%f} (%f,%f) (%f,%f) =>", x, y, gridRelative, modelX, modelY, modelXLon, modelYLon, modelXLat, modelYLat);}
              
              double distLon=hypot(modelXLon-modelX, modelYLon-modelY);
              double distLat=hypot(modelXLat-modelX, modelYLat-modelY);
              
              VJaa=(modelXLon-modelX)/distLon;
              VJab=(modelXLat-modelX)/distLat;
              VJba=(modelYLon-modelY)/distLon;
              VJbb=(modelYLat-modelY)/distLat;
              
//              if (y==0) { CDBDebug("jaco: %f,%f,%f,%f\n", VJaa, VJab, VJba, VJbb);}
              
              double magnitude=hypot(uValues[p], vValues[p]);
              double uu;
              double vv;
//
//              vv = VJaa*uValues[p]+VJab*vValues[p];
//              uu =-( VJba*uValues[p]+VJbb*vValues[p]);
//
              uu = VJaa*uValues[p]+VJab*vValues[p];
              vv = VJba*uValues[p]+VJbb*vValues[p];              
              
              //           if (y==0) {CDBDebug("(%f, %f) ==> (%f,%f)", uValues[p], vValues[p], uu, vv);}
              double newMagnitude = hypot(uu, vv);
              uValues[p]=uu*magnitude/newMagnitude;
              vValues[p]=vv*magnitude/newMagnitude;
              //           if (y==0) {CDBDebug("==> (%f,%f)",uValues[p], vValues[p]);}    
              //              uValues[p]=6;
              //              vValues[p]=0;
      }
}
}
}
#ifdef CImgWarpBilinear_DEBUG
StopWatch_Stop("u/v rotation finished");
#endif
}

for(size_t varNr=0;varNr<sourceImage->dataObject.size();varNr++){
  float *fpValues=valObj[varNr].fpValues;
  float *valueData=valObj[varNr].valueData;
  
  //Smooth the data (better for contour lines)
  #ifdef CImgWarpBilinear_DEBUG
  CDBDebug("start smoothing data with filter %d",smoothingFilter);
  #endif
  smoothData(fpValues,fNodataValue,smoothingFilter, dPixelDestW,dPixelDestH);
  
  //Draw the obtained raster by using triangle tesselation (eg gouraud shading)
  int xP[4],yP[4];
  float vP[4];
  
  size_t drawImageSize = dImageWidth*dImageHeight;
  //Set default nodata values
  for(size_t j=0;j<drawImageSize;j++)valueData[j]=fNodataValue;
  //start drawing triangles
  #if defined(CImgWarpBilinear_DEBUG) || defined(CImgWarpBilinear_TIME)
  StopWatch_Stop("Start triangle generation");
  #endif
  
  /*
   * 
   * float cubicInterpolate (float p[4], float x) {
   *  return p[1] + 0.5 * x*(p[2] - p[0] + x*(2.0*p[0] - 5.0*p[1] + 4.0*p[2] - p[3] + x*(3.0*(p[1] - p[2]) + p[3] - p[0])));
   }
   
   float bicubicInterpolate (float p[4][4], float x, float y) {
     float arr[4];
     arr[0] = cubicInterpolate(p[0], y);
     arr[1] = cubicInterpolate(p[1], y);
     arr[2] = cubicInterpolate(p[2], y);
     arr[3] = cubicInterpolate(p[3], y);
     return cubicInterpolate(arr, x);
   }
   */
  for(int y=dPixelExtent[1];y<dPixelExtent[3]-1;y=y+1){
    for(int x=dPixelExtent[0];x<dPixelExtent[2]-1;x=x+1){
      size_t p = size_t((x-(dPixelExtent[0]))+((y-(dPixelExtent[1]))*dPixelDestW));
      size_t p00=p;
      size_t p10=p+1;
      size_t p01=p+dPixelDestW;
      size_t p11=p+1+dPixelDestW;
      
      
      
      if(fpValues[p00]!=fNodataValue&&fpValues[p10]!=fNodataValue&&
        fpValues[p01]!=fNodataValue&&fpValues[p11]!=fNodataValue)
      {
        yP[0]=dpDestY[p00]; yP[1]=dpDestY[p01]; yP[2]=dpDestY[p10]; yP[3]=dpDestY[p11];
        xP[0]=dpDestX[p00]; xP[1]=dpDestX[p01]; xP[2]=dpDestX[p10]; xP[3]=dpDestX[p11];
        vP[0]=fpValues[p00]; vP[1]=fpValues[p01]; vP[2]=fpValues[p10]; vP[3]=fpValues[p11]; 
        
        fillQuadGouraud(valueData,vP,  dImageWidth,dImageHeight, xP,yP);
        
        /*for(int iy=-15;iy<16;iy++){
          for(int ix=-15;ix<16;ix++){
            if(iy+dpDestY[p00]>0&&ix+dpDestX[p00]>0&&iy+dpDestY[p00]<dImageHeight&&ix+dpDestX[p00]<dImageWidth){
              valueData[(iy+dpDestY[p00])*dImageWidth+ix+dpDestX[p00]]=fpValues[p00];
            }
          }
        }*/
      }
    }
  }
  
  
  /*(for(int y=dPixelExtent[1];y<dPixelExtent[3]-3;y=y+1){
    for(int x=dPixelExtent[0];x<dPixelExtent[2]-3;x=x+1){
      size_t p = size_t((x-(dPixelExtent[0]))+((y-(dPixelExtent[1]))*dPixelDestW));
      size_t p00=p;
      size_t p10=p+1;
      size_t p01=p+dPixelDestW;
      size_t p11=p+1+dPixelDestW;
      
      size_t pc00=p;
      size_t pc01=p+1;
      size_t pc02=p+2;
      size_t pc03=p+3;
      
      
      float a = -0.5*fpValues[pc00]+
      
    }
  }*/
}

//Copy pointerdatabitmap to graphics
#ifdef CImgWarpBilinear_DEBUG
CDBDebug("Start converting float bitmap to graphics");
#endif

float *valueData=valObj[0].valueData;
//Draw bilinear, simple variable
if(drawMap==true&&enableShade==false&&enableVector==false&&enableBarb==false){
  for(int y=0;y<dImageHeight;y++){
    for(int x=0;x<dImageWidth;x++){
      setValuePixel(sourceImage,drawImage,x,y,valueData[x+y*dImageWidth]);
    }
  }
}
//Wind VECTOR
std::vector<CalculatedWindVector> windVectors; //holds windVectors after calculation to draw them on top
bool convertToKnots=false; //default is false
//if((enableVector||enableBarb))
{
  
  if(sourceImage->dataObject.size()==2){
    int firstXPos=0;
    int firstYPos=0;
    
    double tx=((-drawImage->Geo->dfBBOX[0])/(drawImage->Geo->dfBBOX[2]-drawImage->Geo->dfBBOX[0]))*double(dImageWidth);
    double ty=dImageHeight-((-drawImage->Geo->dfBBOX[1])/(drawImage->Geo->dfBBOX[3]-drawImage->Geo->dfBBOX[1]))*double(dImageHeight);
    
    //Are u/v values in m/s? (Should we convert for wind barb drawing?)
    //Depends on value units
    //Derive convertToKnots from units
    CT::string units="m/s";
    units=sourceImage->dataObject[0]->units;
    CDBDebug("units = %s", units.c_str());
    if (!(units.equals("kts")||units.equals("knots"))) convertToKnots=true;
    
    
    
    //Number of pixels between the vectors:
    int vectorDensityPy=50;//22;
    int vectorDensityPx=50;//22;
    firstXPos=int(tx)%vectorDensityPy;
    firstYPos=int(ty)%vectorDensityPx;
    double u,v;
    
    double direction;
    double strength;
    //double pi=3.141592654;
    int stepx=vectorDensityPx; //Raster stride at barb distances
    int stepy=vectorDensityPy;
    // If contouring, drawMap or shading is wanted, step through all destination raster points
    if(enableContour||enableShade||drawMap){
      stepy=1;
      stepx=1;
    }
    //Loops through complete destination image in screen coord.
    for(int y=firstYPos-vectorDensityPx;y<dImageHeight;y=y+stepy){
      for(int x=firstXPos-vectorDensityPy;x<dImageWidth;x=x+stepx){
        //        CDBDebug("pos: %d,%d", x, y);
        if(x>=0&&y>=0){
          size_t p = size_t(x+y*dImageWidth); //pointer in dest. image
          //          CDBDebug("pos: %d,%d ==> p", x, y, p);
          
          //valObj[0].fpValues;
          //drawImage->setPixel(dpDestX[p],dpDestY[p],240);
          u=valObj[0].valueData[p];
          v=valObj[1].valueData[p];
          
          if((u!=fNodataValue)&&(v!=fNodataValue)){
            direction=atan2(v,u);
            strength=sqrt(u*u+v*v);
            valObj[0].valueData[p]=strength;
            
            if(drawMap==true){       
              setValuePixel(sourceImage,drawImage,x,y,strength);
            }
            if (!drawGridVectors) {
              if((int(x-firstXPos)%vectorDensityPy==0&&(y-firstYPos)%vectorDensityPx==0)||(enableContour==false&&enableShade==false)){
                strength=(strength)*1.0;
                
                //Calculate coordinates from requested coordinate system
                double projectedCoordX=((double(x)/double(dImageWidth))*(drawImage->Geo->dfBBOX[2]-drawImage->Geo->dfBBOX[0]))+drawImage->Geo->dfBBOX[0];;
                double projectedCoordY=((double(dImageHeight-y)/double(dImageHeight))*(drawImage->Geo->dfBBOX[3]-drawImage->Geo->dfBBOX[1]))+drawImage->Geo->dfBBOX[1];;
                
                //CDBDebug("W= d H=%d",dImageWidth,dImageHeight);
                //CDBDebug("BBOX= %f,%f,%f,%f",drawImage->Geo->dfBBOX[0],drawImage->Geo->dfBBOX[1],drawImage->Geo->dfBBOX[2],drawImage->Geo->dfBBOX[3]);
                
                
                warper->reprojToLatLon(projectedCoordX,projectedCoordY);
                
                bool flip=projectedCoordY<0; //Remember if we have to flip barb dir for southern hemisphere
                flip=false;
                CalculatedWindVector wv(x, y, direction, strength,convertToKnots,flip);
                //                  drawImage->circle(x,y,2, 240);
                windVectors.push_back(wv);
                //                  if (enableVector) drawImage->drawVector(x,y,direction,strength,240);
                //                  if (enableBarb) drawImage->drawBarb(x,y,direction,strength,240,convertToKnots,flip);
              }
            }
          }else valObj[0].valueData[p]=sourceImage->dataObject[0]->dfNodataValue; //Set speeed to nodata if u OR v == no data
        }
      }
    }
  }
  
} 

if(((enableVector||enableBarb)&&drawGridVectors)){
  int wantedSpacing=30;
  int distPoint=int(hypot(dpDestX[1]-dpDestX[0], dpDestY[1]-dpDestY[0]));
  
  int stepx=int(float(wantedSpacing)/distPoint+0.5);
  if (stepx<1) stepx=1;
  int stepy=stepx;
  if(sourceImage->dataObject.size()==2){
    bool doprint=true;
    for(int y=dPixelExtent[1];y<dPixelExtent[3];y=y+stepy){ //TODO Refactor to GridExtent
        for(int x=dPixelExtent[0];x<dPixelExtent[2];x=x+stepx){
          size_t p = size_t((x-(dPixelExtent[0]))+((y-(dPixelExtent[1]))*dPixelDestW));
          //double destX,destY;
          // size_t pSrcData = size_t(x+y*dfSourceW);//TODO Refactor to GridWidth
          
          //Skip rest if x,y outside drawArea
          if ((dpDestX[p]>=0)&&(dpDestX[p]<dImageWidth)&&(dpDestY[p]>=0)&&(dpDestY[p]<dImageHeight)){
            double direction;
            //double pi=3.141592654;
            double strength;
            double u=valObj[0].fpValues[p];
            double v=valObj[1].fpValues[p];
            //             drawImage->circle(dpDestX[p], dpDestY[p]pi, 2, 246);
            
            if((u!=fNodataValue)&&(v!=fNodataValue)) {
              direction=atan2(v,u);
              
              strength=sqrt(u*u+v*v);
              //               valObj[0].valueData[p]=strength;
              
              //Calculate coordinates from requested coordinate system to determine barb flip
              double modelX=dfSourcedExtW*double(x)+dfSourceOrigX;
              double modelY=dfSourcedExtH*double(y)+dfSourceOrigY;
              //double XX=modelX; double YY=modelY;
              warper->reprojToLatLon(modelX,modelY);
              
              bool flip=modelY<0; //Remember if we have to flip barb dir for southern hemisphere
              flip=false;
              //                if ((fabs(projectedCoordX)<0.8)&&(fabs(projectedCoordY-52)<1)) {
                //  CDBDebug("[%d,%d] p=%d {w=%d} (%f,%f) MLL:{%f,%f} LL:{%f,%f} [%d,%d]", x, y, p, dPixelDestW, u, v, modelX, modelY, XX, YY, dpDestX[p], dpDestY[p]);
              //                }  
              //Project x,y source grid indexes topi vis. space
              //  double modelX=dfSourcedExtW*double(x)+dfSourceOrigX;
              //  double modelY=dfSourcedExtH*double(y)+dfSourceOrigY;
              //  warper->reprojpoint_inv(modelX,modelY); // model to vis proj.
              
              //               if (y==0) {CDBDebug("PLOT %d,%d at %d,%d {%f} (%f,%f)", x, y, dpDestX[p], dpDestY[p], p, direction, strength);}

              CalculatedWindVector wv(dpDestX[p], dpDestY[p], direction, strength,convertToKnots,flip);
              windVectors.push_back(wv);
            }
          }
        }
        doprint=false;
    }
  }
}
//Make Contour if desired
//drawContour(valueData,fNodataValue,500,5000,drawImage);
if(enableContour||enableShade){
 
  
  
  
  drawContour(valueData,fNodataValue,shadeInterval,&contourDefinitions,sourceImage,drawImage,enableContour,enableShade,enableContour);
}

/*
 *    for(int y=dPixelExtent[1];y<dPixelExtent[3];y++){
 *    for(int x=dPixelExtent[0];x<dPixelExtent[2];x++){
 *    size_t p = size_t((x-(dPixelExtent[0]))+((y-(dPixelExtent[1]))*dPixelDestW));
 *    for(int y1=-2;y1<4;y1++){
 *    for(int x1=-2;x1<4;x1++){
 *    drawImage->setPixel(dpDestX[p]+x1,dpDestY[p]+y1,248);
 * }
 * }
 * 
 * for(int y1=-1;y1<3;y1++){
 *  for(int x1=-1;x1<3;x1++){
 *  setValuePixel(sourceImage,drawImage,dpDestX[p]+x1,dpDestY[p]+y1,fpValues[p]);
 }
 }
 }
 }*/
/*if(drawMap==true)
 *    {
 *      for(int y=0;y<dImageHeight;y++){
 *        for(int x=0;x<dImageWidth;x++){
 *          setValuePixel(sourceImage,drawImage,x,y,valObj[0].valueData[x+y*dImageWidth]);
 * }
 * }
 * }*/

//                  if (enableVector) drawImage->drawVector(x,y,direction,strength,240);
//                  if (enableBarb) drawImage->drawBarb(x,y,direction,strength,240,convertToKnots,flip);
//                    
if (enableVector) {
  CalculatedWindVector wv;
  for (size_t sz=0; sz<windVectors.size();sz++) {
    wv=windVectors[sz];
    drawImage->drawVector(wv.x, wv.y, wv.dir, wv.strength, 240);
  }
}                 
if (enableBarb) {
  CalculatedWindVector wv;
  for (size_t sz=0; sz<windVectors.size();sz++) {
    wv=windVectors[sz];
    drawImage->drawBarb(wv.x, wv.y, wv.dir, wv.strength, 240,wv.convertToKnots,wv.flip);
  }
}                 

//Clean up
delete[] dpDestX;
delete[] dpDestY;
delete[] valObj;
 }
 
 
 
 
 /**
  * Checks at regular intervals wheter a contour line should be drawn or not.
  * @param val The four pixels to check
  * @param contourinterval The regular number to check against
  * @return True on contour with this interval needed, false on do nothing.
  */
 int checkContourRegularInterval(float *val,float contourinterval){
   if(contourinterval==0)return 0;
   float allowedDifference=contourinterval/100000;
   float min,max;
   min=val[0];max=val[0];
   for(int j=1;j<4;j++){
     if(val[j]<min)min=val[j];
     if(val[j]>max)max=val[j];
   }
   min=convertValueToClass(min,contourinterval);
   max=convertValueToClass(max,contourinterval)+contourinterval;
   float difference=max-min;
   if((max-min)/contourinterval<3&&(max-min)/contourinterval>1&&difference>allowedDifference){
     for(double c=min;c<max;c=c+contourinterval){
       if((val[0]>=c&&val[1]<c)||(val[0]>c&&val[1]<=c)||(val[0]<c&&val[1]>=c)||(val[0]<=c&&val[1]>c)||
         (val[0]>c&&val[2]<=c)||(val[0]>=c&&val[2]<c)||(val[0]<=c&&val[2]>c)||(val[0]<c&&val[2]>=c))
       {
         return 1;
       }
     }
   }
   return 0;
 }
 
  
  /**
    * Checks at defined intervals wheter a contour line should be drawn or not.
    * @param val The four pixels to check
    * @param contourinterval The numbers to check against
    * @return True on contour with this interval needed, false on do nothing.
    */
  int checkContourDefinedInterval(float *val,std::vector<float> *intervals){
    for(size_t j=0;j<intervals->size();j++){
      float c= (*intervals)[j];
      if((val[0]>=c&&val[1]<c)||(val[0]>c&&val[1]<=c)||(val[0]<c&&val[1]>=c)||(val[0]<=c&&val[1]>c)||
        (val[0]>c&&val[2]<=c)||(val[0]>=c&&val[2]<c)||(val[0]<=c&&val[2]>c)||(val[0]<c&&val[2]>=c))
      {
        return 1;
      }
    }
    return 0;
  }
  

  
  class ContourType{
  public:
      ContourDefinition *contourDefinition;    
      CT::string text;
  };
  
  /**
   * Check if this requires contours
   */
  int checkIfContourRequired(float *val,std::vector<ContourDefinition> *contourDefinitions){
    for(size_t j=0;j<contourDefinitions->size();j++){
      ContourDefinition *s=&(*contourDefinitions)[j];
      //Check for continuous intervals
      if(s->continuousInterval!=0){
        if(checkContourRegularInterval(val,s->continuousInterval)){
          return 1;
        }
      }
      //Check for defined intervals
      if(s->definedIntervals.size()>0){
        for(size_t j=0;j<s->definedIntervals.size();j++){
          float c= s->definedIntervals[j];
          if((val[0]>=c&&val[1]<c)||(val[0]>c&&val[1]<=c)||(val[0]<c&&val[1]>=c)||(val[0]<=c&&val[1]>c)||
            (val[0]>c&&val[2]<=c)||(val[0]>=c&&val[2]<c)||(val[0]<=c&&val[2]>c)||(val[0]<c&&val[2]>=c))
          {
          
            return 1;
          }
        }
      }
    }
    
    return 0;
  }
  
  /**
   * Get contour information including text
   */
  ContourType getContourInformation(float *val,std::vector<ContourDefinition> *contourDefinitions){
    ContourType contourType;
    contourType.contourDefinition=NULL;
    
    for(size_t j=0;j<contourDefinitions->size();j++){
      ContourDefinition *s=&(*contourDefinitions)[j];
      //Check for continuous intervals
      if(s->continuousInterval!=0){
        if(checkContourRegularInterval(val,s->continuousInterval)){
          contourType.contourDefinition=s;
          float binnedValue=convertValueToClass(val[0]+s->continuousInterval/2,s->continuousInterval);
          contourType.text.print(s->textFormat.c_str(),binnedValue);
          return contourType;
        }
      }
      //Check for defined intervals
      if(s->definedIntervals.size()>0){
        for(size_t j=0;j<s->definedIntervals.size();j++){
          float c= s->definedIntervals[j];
          if((val[0]>=c&&val[1]<c)||(val[0]>c&&val[1]<=c)||(val[0]<c&&val[1]>=c)||(val[0]<=c&&val[1]>c)||
            (val[0]>c&&val[2]<=c)||(val[0]>=c&&val[2]<c)||(val[0]<=c&&val[2]>c)||(val[0]<c&&val[2]>=c))
          {
            contourType.contourDefinition=s; 
            //s->textFormat="%2.1f";
            contourType.text.print(s->textFormat.c_str(),c);
            //contourType.text.print("%2.1f",c);
            return contourType;
          }
        }
      }
    }
    
    return contourType;
  }
 
 
 void CImgWarpBilinear::drawContour(float *valueData,float fNodataValue,float interval, std::vector<ContourDefinition> *pContourDefinitions,CDataSource *dataSource,CDrawImage *drawImage,bool drawLine, bool drawShade, bool drawText){
  
   //When using min/max stretching, the shadeclasses need to be extended according to its shade interval
   if(dataSource->stretchMinMax==true){
     if(dataSource->statistics!=NULL){
       float legendInterval=interval;
       float minValue=(float)dataSource->statistics->getMinimum();
       float maxValue=(float)dataSource->statistics->getMaximum();
       float iMin=convertValueToClass(minValue,legendInterval);
       float iMax=convertValueToClass(maxValue,legendInterval)+legendInterval;
       //Calculate new scale and offset for this:
       float ls=240/((iMax-iMin));
       float lo=-(iMin*ls);
       dataSource->legendScale=ls;
       dataSource->legendOffset=lo;
     }
   }
   #ifdef CImgWarpBilinear_DEBUG 
   CDBDebug("scale=%f offset=%f",dataSource->legendScale,dataSource->legendOffset);
   #endif  
   
   
  
   float ival = interval;
   //   float ivalLine = interval;
   //float idval=int(ival+0.5);
   //  if(idval<1)idval=1;
   //TODO
   
   char szTemp[8192];
   int dImageWidth=drawImage->Geo->dWidth+1;
   int dImageHeight=drawImage->Geo->dHeight+1;
   
   size_t imageSize=(dImageHeight+0)*(dImageWidth+1);
   #ifdef CImgWarpBilinear_DEBUG
   CDBDebug("imagesize = %d",(int)imageSize);
   #endif
   
   //Create a distance field, this is where the line information will be put in.
   unsigned int *distance = new unsigned int[imageSize];
   memset (distance,0,imageSize*sizeof(unsigned int));
   

   
   //TODO "pleister" om contourlijnen goed te krijgen.
   if(1==2){
     #ifdef CImgWarpBilinear_TIME
     StopWatch_Stop("substracting ival/100");
     #endif

     float substractVal=ival/100;
     for(int y=0;y<dImageHeight;y++){
       for(int x=0;x<dImageWidth;x++){
         valueData[x+y*dImageWidth]-=substractVal;
       }
     }
     fNodataValue-=substractVal;
     #ifdef CImgWarpBilinear_TIME
     StopWatch_Stop("finished substracting ival/100");
     #endif
   }
   
   #ifdef CImgWarpBilinear_DEBUG
   CDBDebug("start shade/contour with nodatavalue %f",fNodataValue);
   #endif
  
   float val[4];
   
   int snr=0;
   int numShadeDefs=(int)shadeDefinitions.size();
   float shadeDefMin[numShadeDefs];
   float shadeDefMax[numShadeDefs];
   unsigned char shadeColorR[numShadeDefs];
   unsigned char shadeColorG[numShadeDefs];
   unsigned char shadeColorB[numShadeDefs];
   for(snr=0;snr<numShadeDefs;snr++){
     shadeDefMin[snr]=shadeDefinitions[snr].min;
     shadeDefMax[snr]=shadeDefinitions[snr].max;
     
     if(shadeDefinitions[snr].foundColor){
      shadeColorR[snr]=shadeDefinitions[snr].fillColor.r;
      shadeColorG[snr]=shadeDefinitions[snr].fillColor.g;
      shadeColorB[snr]=shadeDefinitions[snr].fillColor.b;
     }else{
       CColor color=drawImage->getColorForIndex(getPixelIndexForValue(dataSource,shadeDefMin[snr]));
       shadeColorR[snr]=color.r;
       shadeColorG[snr]=color.g;
       shadeColorB[snr]=color.b;
     }
   }
   int lastShadeDef=0;
   for(int y=0;y<dImageHeight-1;y++){
     for(int x=0;x<dImageWidth-1;x++){
       size_t p1 = size_t(x+y*dImageWidth);
       val[0] = valueData[p1];
       val[1] = valueData[p1+1];
       val[2] = valueData[p1+dImageWidth];
       val[3] = valueData[p1+dImageWidth+1];
       
       //Check if all pixels have values...
       if(val[0]!=fNodataValue&&val[1]!=fNodataValue&&val[2]!=fNodataValue&&val[3]!=fNodataValue&&
         val[0]==val[0]&&val[1]==val[1]&&val[2]==val[2]&&val[3]==val[3]
       ){
         //Draw shading
         if(drawShade){
           if(interval!=0){
              setValuePixel(dataSource,drawImage,x,y,convertValueToClass(val[0],interval));
           }else{
             int done=numShadeDefs;
             if(val[0]>=shadeDefMin[lastShadeDef]&&val[0]<shadeDefMax[lastShadeDef]){
               done=0;
            }else{
              do{
                lastShadeDef++;if(lastShadeDef>numShadeDefs)lastShadeDef=0;
                done--;
                if(val[0]>=shadeDefMin[lastShadeDef]&&val[0]<shadeDefMax[lastShadeDef]){
                  done=0;
                }
              }while(done>0);
             }
             if(done==0){
               drawImage->setPixelTrueColor(x,y,shadeColorR[lastShadeDef],shadeColorG[lastShadeDef],shadeColorB[lastShadeDef]);
             }
           }
         }
         //Draw contourlines
         if(drawLine||drawText){
           if(checkIfContourRequired(val,pContourDefinitions)){
             distance[p1]=1;
           }
         }
       }
     }
   }

   #ifdef CImgWarpBilinear_DEBUG
   CDBDebug("finished shade/contour");
   #endif

    #ifdef CImgWarpBilinear_DEBUG
    CDBDebug("Starting to draw lines and text");
    #endif
    
    int xdir[]={0,-1, 0, 1,-1,-1, 1, 1,0,-1,-2,-2,-2,-2,-2,-1, 0, 1, 2, 2, 2, 2, 2, 1, 0}; //25 possible directions to try;
    int ydir[]={1,0 ,-1, 0, 1,-1,-1, 1,2, 2, 2, 1, 0,-1,-2,-2,-2,-2,-2,-1, 0, 1, 2, 2, 0};
    //A line can be traversed in two directions:
    int secondDirX,secondDirY;
    bool secondDirAvailable=false;
    int linePointDistance=5;
    
    //Loop distance image and create lines (lines need to be done in two directions)
    for(int y=0;y<dImageHeight;y++){
      for(int x=0;x<dImageWidth;x++){     
        if(secondDirAvailable){
          secondDirAvailable=false;
          x=secondDirX;
          y=secondDirY;
          for(int j=0;j<25;j++){
            if(x+xdir[j]>=1&&x+xdir[j]<dImageWidth-1&&
              y+ydir[j]>=1&&y+ydir[j]<dImageHeight-1){
              if(distance[x+xdir[j]+(y+ydir[j])*dImageWidth]==1){
                x=x+xdir[j];
                y=y+ydir[j];
                break;
              }
            }  
          }
        }
        
        int maxneighbordist=(int)distance[x+y*dImageWidth];
        
        //1 means that there is a line, but algorithm has not been there yet.
        if(maxneighbordist==1){
          //2 means that we are working on contour lines
          distance[x+y*dImageWidth]=2;
          maxneighbordist++;
          int j,tx=x,ty=y;
          int startX=x,startY=y;
          
          //Remember the start of this line segment to traverse later in opposite direction
          if(secondDirAvailable==false){
            secondDirAvailable=true;
            secondDirX=x;secondDirY=y;
          }
          
          //int col;
          CColor linecolor;
          CColor textcolor;
          float w;
          
          int lastXdir=-10,lastYdir;
          int distanceFromStart=0;
          int drawnText=0;
          int busyDrawingText=0;
          int drawTextStartX=0;
          int drawTextStartY=0;
          int needToDrawText=0;
          int textAreaLeft=25;//int(float(dImageWidth)*(1.0f/10.0f));
          int textAreaRight=dImageWidth-25;//int(float(dImageWidth)*(9.0f/10.0f));
          drawImage->moveTo(x,y);
          do{
            for(j=0;j<24;j++){
              if(x+xdir[j]>=1&&x+xdir[j]<dImageWidth-1&&
                y+ydir[j]>=1&&y+ydir[j]<dImageHeight-1){
                int d=distance[x+xdir[j]+(y+ydir[j])*dImageWidth];
              
                //Calculate whether we need to draw some text (busyDrawingText)
                if(drawText){
                  if(d%500==499){
                    drawnText=0;needToDrawText=0;
                  }
                  if(needToDrawText==0){
                    if((d%100==29)&&x>textAreaLeft&&x<textAreaRight&&y>24&&y<dImageHeight-25&&drawnText==0){
                      needToDrawText=1;
                    }
                  }else{
                    if(drawnText==0){
                      if(lastXdir!=-10&&lastYdir==0){
                        if(abs(startY-y)==0&&d>25){
                          //drawImage->line(x,y,x+4,y+4,2,240);
                          drawTextStartX=x;
                          drawTextStartY=y;
                          drawnText=1;
                          needToDrawText=0;

                          
                          ContourType contourType=getContourInformation(val,pContourDefinitions);
                          if(contourType.contourDefinition!=NULL){
                            if(contourType.text.length()>0){
                              snprintf(szTemp,8000,contourType.text.c_str());
                              busyDrawingText=int(float(strlen(szTemp))*1.3)+2;
                            }
                          }
                        }
                      }
                    }
                  }
                }
                //\DrawText
                
                
                //Line segment not yet done
                if(d==1){
                  lastXdir=xdir[j];
                  lastYdir=ydir[j];
                  x=x+lastXdir;
                  y=y+lastYdir;
                  maxneighbordist++;
                  size_t p=x+y*dImageWidth;
                  
                  val[0] = valueData[p];
                  val[1] = valueData[p+1];
                  val[2] = valueData[p+dImageWidth];
                  val[3] = valueData[p+dImageWidth+1];
                  
                  distance[p]=maxneighbordist;
                  
                  distanceFromStart = maxneighbordist-3;
                  
                  if(distanceFromStart%linePointDistance==0){
                    linecolor=CColor(0,0,0,255);
                    textcolor=CColor(0,0,0,255);
                    w=0.4;
                    
                    ContourType contourType=getContourInformation(val,pContourDefinitions);
                    if(contourType.contourDefinition!=NULL){
                      linecolor=contourType.contourDefinition->linecolor;
                      textcolor=contourType.contourDefinition->textcolor;
                      w=contourType.contourDefinition->lineWidth;
                    }
                    
                    if(drawLine){

                      //Draw Line, no text
                      if(busyDrawingText<=0){
                        //drawImage->lineTo(startX,startY,x,y,w,linecolor);
                        
                        drawImage->lineTo(x,y,w,linecolor);
                      }
                      
                      //Draw the text, draw no line
                      if(busyDrawingText>0){
                        drawImage->moveTo(x,y);
                        busyDrawingText--;
                        if(busyDrawingText==0){
                          //Really draw text when busyDrawingText is zero
                          float l=strlen(szTemp);
                          double angle;
                          if(y-drawTextStartY!=0){
                            angle=atan((double(y-drawTextStartY))/(double(drawTextStartX-x)));
                          }else angle=0;
                          float cx=(drawTextStartX+x)/2;
                          float cy=(drawTextStartY+y)/2;
                          float tx=cx;
                          float ty=cy;
                          float ca=cos(angle);
                          float sa=sin(angle);
                          float offX=((ca*l*2.3)+(sa*-2.3));
                          float offY=(-(sa*l*3.0)+(ca*-2.3));
                          tx-=offX;
                          ty-=offY;
                          drawImage->drawText( int(tx)+1, int(ty)+1, angle,szTemp,textcolor);
                        }
                      }
                    }else lastXdir=-10;
                    startX=x;startY=y;
                  }
                  j=0;
                }
              }
            }
          }while(j<24);
          drawImage->endLine();
          
          //Draw last connecting line segment
          if(drawLine){
            if(lastXdir!=-10&&distanceFromStart > 1){
              drawImage->line(x+lastXdir*2,y+lastYdir,startX-lastXdir*2,startY-lastYdir,w,linecolor);
            }
          }
          x=tx;y=ty;
        }
      }
    }

       #ifdef CImgWarpBilinear_DEBUG
       CDBDebug("Deleting distance[]");
       #endif
       
       delete[] distance;
       #ifdef CImgWarpBilinear_DEBUG
       CDBDebug("Finisched drawing lines and text");
       #endif
       
     }
     
     
     void CImgWarpBilinear::smoothData(float *valueData,float fNodataValue,int smoothWindow, int W,int H){
       
       //SmootH!
       #ifdef CImgWarpBilinear_TIME
       StopWatch_Stop("[SmoothData]");
       #endif
       if(smoothWindow==0)return;//No smoothing.
  size_t drawImageSize = W*H;
  float *valueData2 = new float[W*H];
  int smw=smoothWindow;
  //Create distance window;
  float distanceWindow[(smw+1)*2*(smw+1)*2];
  float distanceAmmount= 0;
  int dWinP=0;
  for(int y1=-smw;y1<smw+1;y1++){
    for(int x1=-smw;x1<smw+1;x1++){
      float d=sqrt(x1*x1+y1*y1);
      //d=d*8;
      
      d=1/(d+1);
      // d=1;
      distanceWindow[dWinP++]=d;
      distanceAmmount+=d;
    }
  }
  
  float d;
  for(int y=0;y<H;y++){
    for(int x=0;x<W;x++){
      size_t p = size_t(x+y*W);
      if(valueData[p]!=fNodataValue){
        dWinP=0;
        distanceAmmount=0;
        valueData2[p]=0;
        for(int y1=-smw;y1<smw+1;y1++){
          size_t yp=y1*W;
          for(int x1=-smw;x1<smw+1;x1++){
            if(x1+x<W&&y1+y<H&&x1+x>=0&&y1+y>=0){
              float val=valueData[p+x1+yp];
              if(val!=fNodataValue){
                d=distanceWindow[dWinP];
                distanceAmmount+=d;
                valueData2[p] += val*d;
              }
            }
            dWinP++;
          }
        }
        if(distanceAmmount>0)valueData2[p]/=distanceAmmount;
      }else valueData2[p]=fNodataValue;
    }
  }
  for(size_t p=0;p<drawImageSize;p++){valueData[p]=valueData2[p];}
  delete[] valueData2;
  #ifdef CImgWarpBilinear_TIME
  StopWatch_Stop("[/SmoothData]");
  #endif
  
  
}



int CImgWarpBilinear::set(const char *pszSettings){
  //fprintf(stderr, "CImgWarpBilinear.set(%s)\n", pszSettings);
  //"drawMap=false;drawContour=true;contourSmallInterval=1.0;contourBigInterval=10.0;"
  if(pszSettings==NULL)return 0;
  if(strlen(pszSettings)==0)return 0;
  contourDefinitions.clear();
  CT::string settings(pszSettings);
  CT::string *nodes= settings.splitToArray(";");
  for(size_t j=0;j<nodes->count;j++){
    CT::string *values=nodes[j].splitToArray("=");
    if(values->count==2){
      if(values[0].equals("drawMap")){
        if(values[1].equals("true"))drawMap=true;
        if(values[1].equals("false"))drawMap=false;
      }
      if(values[0].equals("drawContour")){
        if(values[1].equals("true"))enableContour=true;
        if(values[1].equals("false"))enableContour=false;
      }
      if(values[0].equals("drawShaded")){
        if(values[1].equals("true"))enableShade=true;
        if(values[1].equals("false"))enableShade=false;
      }
      if(values[0].equals("drawVector")){
        if(values[1].equals("true"))enableVector=true;
        if(values[1].equals("false"))enableVector=false;
      }
      if(values[0].equals("drawBarb")){
        if(values[1].equals("true"))enableBarb=true;
        if(values[1].equals("false"))enableBarb=false;
      }
     
      if(values[0].equals("shadeInterval")){
        shadeInterval=values[1].toFloat();
       // if(shadeInterval==0.0f){CDBWarning("invalid value given for shadeInterval %s",pszSettings);}
      }
      if(values[0].equals("smoothingFilter")){
        smoothingFilter=values[1].toInt();
        if(smoothingFilter<0||smoothingFilter>20){CDBWarning("invalid value given for smoothingFilter %s",pszSettings);}
      }
      

      if(values[0].equals("contourBigInterval")){
        contourDefinitions.push_back(ContourDefinition(1.4,CColor(0,0,0,255),CColor(0,0,0,255), values[1].toFloat(),NULL));
      }
      
      if(values[0].equals("contourSmallInterval")){
        contourDefinitions.push_back(ContourDefinition(0.7,CColor(0,0,0,255),CColor(0,0,0,255), values[1].toFloat(),NULL));
      }
      
      if(values[0].equals("shading")){
        CColor fillcolor=CColor(0,0,0,0);
        float max,min;
        bool foundColor=false;
        CT::string *shadeSettings=values[1].splitToArray("$");
        for(size_t l=0;l<shadeSettings->count;l++){
          CT::string *kvp=shadeSettings[l].splitToArray("(");
          if(kvp[0].equals("min"))min=kvp[1].toFloat();
          if(kvp[0].equals("max"))max=kvp[1].toFloat();
          if(kvp[0].equals("fillcolor")){kvp[1].setSize(7);fillcolor=CColor(kvp[1].c_str());foundColor=true;}
          delete[] kvp;
        }

        shadeDefinitions.push_back(ShadeDefinition(min,max,fillcolor,foundColor));
        delete[] shadeSettings;
      }
      
      
      if(values[0].equals("contourline")){
        float lineWidth=1;
        CColor linecolor=CColor(0,0,0,255);
        CColor textcolor=CColor(0,0,0,255);
        float interval=0;
        CT::string textformat="";
        CT::string classes="";
        CT::string *lineSettings=values[1].splitToArray("$");
        for(size_t l=0;l<lineSettings->count;l++){
          CT::string *kvp=lineSettings[l].splitToArray("(");
          if(kvp[0].equals("width"))lineWidth=kvp[1].toFloat();
          if(kvp[0].equals("interval"))interval=kvp[1].toFloat();
          if(kvp[0].equals("classes")){classes.copy(kvp[1].c_str());}
          if(kvp[0].equals("linecolor")){kvp[1].setSize(7);linecolor=CColor(kvp[1].c_str());}
          if(kvp[0].equals("textcolor")){kvp[1].setSize(7);textcolor=CColor(kvp[1].c_str());}
          if(kvp[0].equals("textformatting")){textformat.copy(kvp[1].c_str(),kvp[1].length()-1);}
          delete[] kvp;
        }
        if(interval!=0){
          contourDefinitions.push_back(ContourDefinition(lineWidth,linecolor, textcolor,interval,textformat.c_str()));
        }else{
          if(classes.length()>0){
            contourDefinitions.push_back(ContourDefinition(lineWidth,linecolor, textcolor,classes.c_str(),textformat.c_str()));
          }
        }
        delete[] lineSettings;
      }
   
      
      if (values[0].equals("drawGridVectors")) {
        drawGridVectors=values[1].equals("true");
      }
    }
    delete[] values;
  }
  delete[] nodes;
  return 0;
}