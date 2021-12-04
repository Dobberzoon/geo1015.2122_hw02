#include <iostream>
#include <queue>

#include "gdal_priv.h"
#include "cpl_conv.h"

// Storage and access of a raster of a given size
struct Raster {
  std::vector<int> pixels; // where everything is stored
  int max_x, max_y; // number of columns and rows
  
  // Initialise a raster with x columns and y rows
  Raster(int x, int y) {
    max_x = x;
    max_y = y;
    unsigned int total_pixels = x*y;
    pixels.reserve(total_pixels);
  }
  
  // Fill values of an entire row
  void add_scanline(const int *line) {
    for (int i = 0; i < max_x; ++i) pixels.push_back(line[i]);
  }
  
  // Fill entire raster with zeros
  void fill() {
    unsigned int total_pixels = max_x*max_y;
    for (int i = 0; i < total_pixels; ++i) pixels.push_back(0);
  }
  
  // Access the value of a raster cell to read or write it
  int &operator()(int x, int y) {
    assert(x >= 0 && x < max_x);
    assert(y >= 0 && y < max_y);
    return pixels[x + y*max_x];
  }
  
  // Access the value of a raster cell to read it
  int operator()(int x, int y) const {
    assert(x >= 0 && x < max_x);
    assert(y >= 0 && y < max_y);
    return pixels[x + y*max_x];
  }
};

// A structure that links to a single cell in a Raster
struct RasterCell {
  int x, y; // row and column of the cell
  int elevation;
  int insertion_order;
  
  // Defines a new link to a cell
  RasterCell(int x, int y, int elevation, int insertion_order) {
    this->x = x;
    this->y = y;
    this->elevation = elevation;
    this->insertion_order = insertion_order;
  }
  
  // Define the order of the linked cells (to be used in a priority_queue)
  bool operator<(const RasterCell &other) const {
    // to do with statements like if (this->elevation > other.elevation) return false/true;
  }
};

// Write the values in a linked raster cell (useful for debugging)
std::ostream& operator<<(std::ostream& os, const RasterCell& c) {
  os << "{h=" << c.elevation << ", o=" << c.insertion_order << ", x=" << c.x << ", y=" << c.y << "}";
  return os;
}

int main(int argc, const char * argv[]) {
  
  // Open dataset
  GDALDataset  *input_dataset;
  GDALAllRegister();
  input_dataset = (GDALDataset *)GDALOpen("/Users/ken/Downloads/N25W101.hgt", GA_ReadOnly); // a nice tile I used for testing
  if (input_dataset == NULL) {
    std::cerr << "Couldn't open file" << std::endl;
    return 1;
  }
  
  // Print dataset info
  double geo_transform[6];
  std::cout << "Driver: " << input_dataset->GetDriver()->GetDescription() << "/" << input_dataset->GetDriver()->GetMetadataItem(GDAL_DMD_LONGNAME) << std::endl;;
  std::cout << "Size is " << input_dataset->GetRasterXSize() << "x" << input_dataset->GetRasterYSize() << "x" << input_dataset->GetRasterCount() << std::endl;
  if (input_dataset->GetProjectionRef() != NULL) std::cout << "Projection is '" << input_dataset->GetProjectionRef() << "'" << std::endl;
  if (input_dataset->GetGeoTransform(geo_transform) == CE_None) {
    std::cout << "Origin = (" << geo_transform[0] << ", " << geo_transform[3] << ")" << std::endl;
    std::cout << "Pixel Size = (" << geo_transform[1] << ", " << geo_transform[5] << ")" << std::endl;
  }
  
  // Print Band 1 info
  GDALRasterBand *input_band;
  int nBlockXSize, nBlockYSize;
  int bGotMin, bGotMax;
  double adfMinMax[2];
  input_band = input_dataset->GetRasterBand(1);
  input_band->GetBlockSize(&nBlockXSize, &nBlockYSize);
  std::cout << "Band 1 Block=" << nBlockXSize << "x" << nBlockYSize << " Type=" << GDALGetDataTypeName(input_band->GetRasterDataType()) << " ColorInterp=" << GDALGetColorInterpretationName(input_band->GetColorInterpretation()) << std::endl;
  adfMinMax[0] = input_band->GetMinimum(&bGotMin);
  adfMinMax[1] = input_band->GetMaximum(&bGotMax);
  if (!(bGotMin && bGotMax)) GDALComputeRasterMinMax((GDALRasterBandH)input_band, TRUE, adfMinMax);
  std::cout << "Min=" << adfMinMax[0] << " Max=" << adfMinMax[1] << std::endl;
  
  // Read Band 1 line by line
  int nXSize = input_band->GetXSize();
  int nYSize = input_band->GetYSize();
  Raster input_raster(nXSize, nYSize);
  for (int current_scanline = 0; current_scanline < nYSize; ++current_scanline) {
    int *scanline = (int *)CPLMalloc(sizeof(float)*nXSize);
    if (input_band->RasterIO(GF_Read, 0, current_scanline, nXSize, 1,
                         scanline, nXSize, 1, GDT_Int32,
                         0, 0) != CPLE_None) {
      std::cerr << "Couldn't read scanline " << current_scanline << std::endl;
      return 1;
    } input_raster.add_scanline(scanline);
    CPLFree(scanline);
  } std::cout << "Created raster: " << input_raster.max_x << "x" << input_raster.pixels.size()/input_raster.max_y << " = " << input_raster.pixels.size() << std::endl;
  
  // Flow direction
  Raster flow_direction(input_raster.max_x, input_raster.max_y);
  flow_direction.fill();
  std::priority_queue<RasterCell, std::deque<RasterCell>> cells_to_process_flow;
  // to do
  
  // Write flow direction
  // to do
  
  // Flow accumulation
  Raster flow_accumulation(input_raster.max_x, input_raster.max_y);
  // to do
  
  // Write flow accumulation
  // to do
  
  // Close input dataset
  GDALClose(input_dataset);
  
  return 0;
}
