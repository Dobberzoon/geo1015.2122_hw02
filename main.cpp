#include <iostream>
#include <queue>

#include "gdal_priv.h"
#include "cpl_conv.h"
#include <matplot/matplot.h>

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
    return this->elevation > other.elevation;
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
  input_dataset = (GDALDataset *)GDALOpen("/Users/danieldobson/Library/CloudStorage/OneDrive-Personal/GEOMATICS/GEO1015-Y2/geo1015.2021/hw/02/data/N56E105.hgt", GA_ReadOnly); // a nice tile I used for testing
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
  // Create the output dataset for writing



  // With this for loop I access the flow_direction raster to write the z values at (x, y)
  for (int i=0; i<flow_direction.max_x; i++) {
      for (int j=0; j<flow_direction.max_y; j++) {
          int elevation = i*10;
          int insertion_order = j * -1;
          int x = i;
          int z = j;
          //RasterCell rc(x, y, elevation, insertion_order);
          flow_direction.operator()(i, j) = z; //RasterCell is for the priority queue, not like this...
      }
  }

    // For starting with the top row:
    for (int i=0; i<input_raster.max_x; i++) {
        // this line prints the z values to check if correct values are read
        //std::cout << "this is z for top row input_raster: " << input_raster(i, 0) << std::endl;

        int x, y; // row and column of the cell
        int elevation;
        int insertion_order;
        x = i; y = 0;
        elevation = input_raster(x, y);
        insertion_order = i;
        RasterCell n(x, y, elevation, insertion_order);
        cells_to_process_flow.push(n);

    }

    while (cells_to_process_flow.empty() == false) {
        std::cout << cells_to_process_flow.top() << " \n";
        cells_to_process_flow.pop();
    }
    /* // This will print whether included driver from input_data supports Create() or CreateCopy()
     * // Spoiler alert: it's CreateCopy()
    const char *pszFormat = "SRTMHGT";
    GDALDriver *poDriver;
    char **papszMetadata;
    poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
    if( poDriver == NULL )
        exit( 1 );
    papszMetadata = poDriver->GetMetadata();
    if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATE, FALSE ) )
        printf( "Driver %s supports Create() method.\n", pszFormat );
    if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATECOPY, FALSE ) )
        printf( "Driver %s supports CreateCopy() method.\n", pszFormat );
    */

  // This loop reads all input_raster values
  /*
    for (int i=0; i<input_raster.max_x; i++) {
        for (int j=0; j<input_raster.max_y; j++) {
            //std::cout << j << std::endl;
            std::cout << "this is the value at (" << i << ", " << j << ")" << input_raster.operator()(i, j) << std::endl;
        }
    }
  */
    // This loop I broke while experimenting, but with added for j blabla you can read each value in the input_raster
    /*
    std::vector<int>
    for (int i=0; i<input_raster.max_x; i++) {
            std::cout << "this is the value at (" << i << ", " << j << ")" << input_raster.operator()(i, j) << std::endl;
        }
    }
     */
   // Here I successfully accessed a value at (x, y) to read the z value from the created flow_direction raster
    std::cout << "this is the value from flow_direction raster at (2, 3000): " << flow_direction(2,3000) << std::endl;


  // Here I will attempt to access the individual columns or rows with indexing
  /* Metz et al:
   * LCP algo
   * 1. determine ultimate outlets and add them to priority list (!) see https://www.geeksforgeeks.org/priority-queue-in-cpp-stl/ for explanation
   * --> on DEMs the potential outlets are:
   * - grid cells along the map boundaries
   * - cells with >= 1 neighboring cells with z = ?
   * priority is defined by elevation and order of addition, where lowest == highest priority && earlier addition == higher priority
   * example: if two cells have equal elevation, cell that was added earlier is chosen
   * OUTPUT: should be in a tree datastructure, which could be one or more trees
   *
   * Tips
    - The initial potential outlets should be all the pixels on the edges of the dataset (ie the top/bottom rows and the leftmost/rightmost columns).
    - If there are very large large rivers/lakes/seas in the area, you might also want to mark all the cells neighbouring them (usually identifiable as large flat areas) as potential outlets. Hardcoding these (eg using a seeding point inside the area), or modifying the DEM to mark them is fine.
    - You want to choose specific values that represent the 8 possible flow directions of a pixel (eg 10 for rightwards and 30 for downwards), as well as special values for pixels at the different stages of processing. Choose sensible values to make visualisation intuitive.
    - We might test your code with other input. Let us know how to run it.
    - rasterio allows you to clone a dataset’s properties using the profile dictionary, while the same applies to GDAL’s CreateCopy. This is very handy when you want to create a new raster with similar properties as an existing one. That being said, you might want to create a new raster in GDAL instead and only copy some properties of the original (eg CRS).
    - Make sure that the raster data type you choose is able to fit the values that you want to store in it.
    - You should have no loops in the flow directions raster. If you do, you’re doing something wrong. --> I think to fill the raster, as in line 123/124
    - If you’re having performance problems, you may crop your input DEM. However, there will be a penalty if your code can only work with tiny datasets.
    - The SRTM data is lat-long, but that doesn’t matter for LCP.*/
  
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
