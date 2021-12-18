#include <iostream>
#include <queue>
#include <string>

#include "gdal_priv.h"
#include "cpl_conv.h"

// My own temporary libraries
#include <time.h>
#include <fstream>
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

  // Fill entire raster with zeros
  void fillOnes() {
    unsigned int total_pixels = max_x*max_y;
    for (int i = 0; i < total_pixels; ++i) pixels.push_back(1);
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
    return ((this->elevation > other.elevation) || (this->elevation == other.elevation) && (this->insertion_order > other.insertion_order));
  }
};

// Write the values in a linked raster cell (useful for debugging)
std::ostream& operator<<(std::ostream& os, const RasterCell& c) {
  os << "{h=" << c.elevation << ", o=" << c.insertion_order << ", x=" << c.x << ", y=" << c.y << "}";
  return os;
}

int main(int argc, const char * argv[]) {

    clock_t start = clock();
  
  // Open dataset
  GDALDataset  *input_dataset;
  GDALAllRegister();
  input_dataset = (GDALDataset *)GDALOpen("/Users/danieldobson/Library/CloudStorage/OneDrive-Personal/GEOMATICS/GEO1015-Y2/geo1015.2021/hw/02/data/N46E008.hgt", GA_ReadOnly); // a nice tile I used for testing
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
  std::vector<RasterCell> cells_to_process_accumulation;

  // to do
  // Create the output dataset for writing

    // Extraction of initial potential outlets (borders of input_raster)
    int countIO = 0;
    for (int i=0, j=0; i<(input_raster.max_x - 1) && j<(input_raster.max_y - 1); i++, j++) {
        int xTop, yTop; // row and column of the cell
        int xBottom, yBottom; // row and column of the cell
        int xLeft, yLeft; // row and column of the cell
        int xRight, yRight; // row and column of the cell

        int elevationTop;
        int elevationBottom;
        int elevationLeft;
        int elevationRight;

        int insertion_orderTop;
        int insertion_orderBottom;
        int insertion_orderLeft;
        int insertion_orderRight;

        xTop = i; yTop = 0;
        xBottom = i; yBottom = (input_raster.max_y - 1);
        xLeft = 0; yLeft = j;
        xRight = (input_raster.max_x - 1); yRight = j;

        elevationTop = input_raster(xTop, yTop);
        elevationBottom = input_raster(xBottom, yBottom);
        elevationLeft = input_raster(xLeft, yLeft);
        elevationRight = input_raster(xRight, yRight);

        //std::cout << elevationTop << std::endl;
        //std::cout << elevationBottom << std::endl;
        //std::cout << elevationLeft << std::endl;
        //std::cout << elevationRight << std::endl;

        insertion_orderTop = countIO;
        //countIO++;
        insertion_orderBottom = countIO;
        //countIO++;
        insertion_orderLeft = countIO;
        //countIO++;
        insertion_orderRight = countIO;
        countIO++;


        RasterCell nTop(xTop, yTop, elevationTop, insertion_orderTop);
        RasterCell nBottom(xBottom, yBottom, elevationBottom, insertion_orderBottom);
        RasterCell nLeft(xLeft, yLeft, elevationLeft, insertion_orderLeft);
        RasterCell nRight(xRight, yRight, elevationRight, insertion_orderRight);

        cells_to_process_flow.push(nTop);
        cells_to_process_flow.push(nBottom);
        cells_to_process_flow.push(nLeft);
        cells_to_process_flow.push(nRight);


    }
    std::cout << "the count of the border extraction loop, versus the size of the priority queue accumulation: " << countIO <<
    ", " << cells_to_process_flow.size() << std::endl;

    /*
    std::ofstream flow, accumulation;
    flow.open("pq_flow.txt");
    accumulation.open("pq_accumulation.txt");

    for (int i = cells_to_process_accumulation.size() - 1; i >= 0; i--) {
        accumulation << cells_to_process_accumulation[i] << std::endl;
    }

    while (!cells_to_process_flow.empty()) {
        flow << cells_to_process_flow.top() <<std::endl;
        cells_to_process_flow.pop();
    }

    flow.close();
    accumulation.close();
    */

    std::cout << "Do you want to indicate flat elevation values? y/n ";
    std::string response;
    std::getline(std::cin, response);

    if (response == "y") {
        std::cout << "Type the value of elevation of flat area (sea, lake, ocean etc.) rounded down as integer: ";
        int flat;
        std::cin >> flat;
        while (flat != 0){
            // Extraction of flat area's to add to priority queue
            for (int i = 1; i < input_raster.max_x; i++) {
                for (int j = 1; j < input_raster.max_y; j++) {

                    if (input_raster(i, j) == flat) {
                        RasterCell flat(i, j, input_raster(i, j), countIO);
                        cells_to_process_flow.push(flat);
                        countIO++;
                    }

                }
            }
            std::cout << "Type in another value for flat area, or 0 to cancel: ";
            std::cin >> flat;
        }
    }
    /*
    // Extraction of flat area's to add to priority queue
    for (int i = 1; i < input_raster.max_x; i++) {
        for (int j = 1; j < input_raster.max_y; j++) {

            //ele_log << input_raster(i, j) << std::endl;
            int input;

            if (input_raster(i, j) == 191) {
                RasterCell flat(i, j, input_raster(i, j), countIO);
                cells_to_process_flow.push(flat);
                countIO++;
            }

        }
    }
    */
    // Direction value mapping
    const int north = 10, northEast = 20, east = 30, southEast = 40, south = 50, southWest = 60, west = 70, northWest = 80;

    // Stack for accumulation
    std::stack<RasterCell, std::deque<RasterCell>> stack;

    // Search operation
    while (!cells_to_process_flow.empty()) {

        int x, y, elevation, insertion_order;
        x = cells_to_process_flow.top().x;
        y = cells_to_process_flow.top().y;
        elevation = cells_to_process_flow.top().elevation;
        insertion_order = countIO;

        RasterCell r(x, y, elevation, insertion_order);
        stack.push(r);

        if (((y - 1) >= 0) && ((x - 1) >= 0) && (flow_direction(x - 1, y - 1) == 0)) {
            flow_direction.operator()(x - 1, y - 1) = southEast;

            elevation = input_raster(x - 1, y - 1);
            RasterCell r1(x - 1, y - 1, elevation, insertion_order);
            cells_to_process_flow.push(r1);
            countIO++;
        }

        if (((y - 1) >= 0) && (flow_direction(x, y - 1) == 0)) {
            flow_direction.operator()(x, y - 1) = east;

            elevation = input_raster(x, y - 1);
            RasterCell r2(x, y - 1, elevation, insertion_order);
            cells_to_process_flow.push(r2);
            countIO++;
        }

        if (((y - 1) >= 0) && ((x + 1) <= flow_direction.max_x) && (flow_direction(x + 1, y - 1) == 0)){
            flow_direction.operator()(x + 1, y - 1) = northEast;

            elevation = input_raster(x + 1, y - 1);
            RasterCell r3(x + 1, y - 1, elevation, insertion_order);
            cells_to_process_flow.push(r3);
            countIO++;
        }

        if (((x + 1) <= flow_direction.max_x) && (flow_direction(x + 1, y) == 0)){
            flow_direction.operator()(x + 1, y) = north;

            elevation = input_raster(x + 1, y);
            RasterCell r4(x + 1, y, elevation, insertion_order);
            cells_to_process_flow.push(r4);
            countIO++;
        }

        if (((x + 1) <= flow_direction.max_x) && ((y + 1) < flow_direction.max_y) && (flow_direction(x + 1, y + 1) == 0)) {
            flow_direction.operator()(x + 1, y + 1) = northWest;

            elevation = input_raster(x + 1, y + 1);
            RasterCell r5(x + 1, y + 1, elevation, insertion_order);
            cells_to_process_flow.push(r5);
            countIO++;
        }

        if (((y + 1) < flow_direction.max_y) && (flow_direction(x, y + 1) == 0)) {
            flow_direction.operator()(x, y + 1) = west;

            elevation = input_raster(x, y + 1);
            RasterCell r6(x, y + 1, elevation, insertion_order);
            cells_to_process_flow.push(r6);
            countIO++;
        }
        if (((y + 1) < flow_direction.max_y) && ((x - 1) >= 0) && (flow_direction(x - 1, y + 1) == 0)) {
            flow_direction.operator()(x - 1, y + 1) = southWest;

            elevation = input_raster(x - 1, y + 1);
            RasterCell r7(x - 1, y + 1, elevation, insertion_order);
            cells_to_process_flow.push(r7);
            countIO++;
        }
        if (((x - 1) >= 0) && (flow_direction(x - 1, y) == 0)) {
            flow_direction.operator()(x - 1, y) = south;

            elevation = input_raster(x - 1, y);
            RasterCell r8(x - 1, y, elevation, insertion_order);
            cells_to_process_flow.push(r8);
            countIO++;
        }

        cells_to_process_flow.pop();

    }

    std::cout << "this is how big stack is: " << stack.size() << std::endl;
    /*
    std::ofstream accumulation;
    accumulation.open("pq_accumulation.txt");



    while (!stack.empty()) {
        accumulation << stack.top() <<std::endl;
        stack.pop();
    }

    accumulation.close();
    */

    GDALDataset  *output_flow_direction;
    GDALDriver *driverGeotiff;
    const char *const flow_dir_filename = "output_flow_direction_katrin_flat.tif";

    driverGeotiff = GetGDALDriverManager()->GetDriverByName("GTiff");
    output_flow_direction = driverGeotiff->Create(flow_dir_filename,nXSize,nYSize,1,GDT_Int32,NULL);
    output_flow_direction->SetGeoTransform(geo_transform);
    output_flow_direction->SetProjection(input_dataset->GetProjectionRef());

    // Write flow direction to file
    int *scanline2 = (int *)CPLMalloc(sizeof(float)*nYSize);
    for (int i=0; i<nXSize; i++) {
        for (int j = 0; j < nYSize; j++) {
            scanline2[j] = (int)flow_direction(j, i);
        }
        output_flow_direction->GetRasterBand(1)->RasterIO(GF_Write, 0, i, nYSize, 1, scanline2, nYSize, 1, GDT_Int32, 0,
                                                          0);
    }
    CPLFree(scanline2);

   // Here I successfully accessed a value at (x, y) to read the z value from the created flow_direction raster
    std::cout << "this is the value from flow_direction raster at (0, 0): " << flow_direction(0,0) << std::endl;


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

  Raster flow_accumulation(input_raster.max_x, input_raster.max_y);
  flow_accumulation.fill();
  
  // Write flow accumulation
  // to do

    std::cout << "flow accumulation stack before: " << stack.size() << std::endl;

    // Calculate flow accumulation
    while (!stack.empty()) {
        int x, y, accumulation, direction;
        x = stack.top().x;
        y = stack.top().y;
        accumulation = flow_accumulation(x, y);
        direction = flow_direction(x, y);
        //std::cout << accumulation << std::endl;
        //std::cout << direction << std::endl;


        // linksboven
        if (((y - 1) >= 0) && ((x - 1) >= 0) && (direction == northWest)) {
            flow_accumulation.operator()(x - 1, y - 1)++;
            //std::cout << "northwest" << std::endl;
        }
        // links
        if (((y - 1) >= 0) && (direction == west)) {
            flow_accumulation.operator()(x, y - 1)++;
            //std::cout << "west" << std::endl;
        }
        // linksonder
        if (((y - 1) >= 0) && ((x + 1) <= flow_accumulation.max_x) && (direction == southWest)) {
            flow_accumulation.operator()(x + 1, y - 1)++;
            //std::cout << "soutwest" << std::endl;
        }
        // onder
        if  (((x + 1) <= flow_accumulation.max_x) && (direction == south)) {
            flow_accumulation.operator()(x + 1, y)++;
            //std::cout << "south" << std::endl;
        }
        // rechtsonder
        if (((x + 1) <= flow_accumulation.max_x) && ((y + 1) < flow_accumulation.max_y) &&
            (direction == southEast)) {
            flow_accumulation.operator()(x + 1, y + 1)++;
            //std::cout << "southeast" << std::endl;
        }
        // rechts
        if (((y + 1) < flow_accumulation.max_y) && (direction == east)) {
            flow_accumulation.operator()(x, y + 1)++;
            //std::cout << "east" << std::endl;
        }
        // rechtsboven
        if (((y + 1) < flow_accumulation.max_y) && ((x - 1) >= 0) && (direction == northEast)) {
            flow_accumulation.operator()(x - 1, y + 1)++;
            //std::cout << "northeast" << std::endl;
        }
        // boven
        if (((x - 1) >= 0) && (direction == north)) {
            flow_accumulation.operator()(x - 1, y)++;
            //std::cout << "north" << std::endl;
        }

        stack.pop();

    }

    /*
    // Calculate flow accumulation
    while (!stack.empty()) {
        int x, y, accumulation, direction;
        x = stack.top().x;
        y = stack.top().y;
        accumulation = flow_accumulation(x, y);
        direction = flow_direction(x, y);
        stack.pop();

        // linksboven
        if (((y - 1) >= 0) && ((x - 1) >= 0) && (direction = northWest)) {
             flow_accumulation.operator()(x - 1, y - 1) += accumulation;
        }
        // links
        if (((y - 1) >= 0) && (direction == west)) {
             flow_accumulation.operator()(x, y - 1) += accumulation;
        }
        // linksonder
        if (((y - 1) >= 0) && ((x + 1) <= flow_accumulation.max_x) && (direction == southWest)) {
             flow_accumulation.operator()(x + 1, y - 1) += accumulation;
        }
        // onder
        if (((x + 1) <= flow_accumulation.max_x) && (direction == south)) {
             flow_accumulation.operator()(x + 1, y) += accumulation;
        }
        // rechtsonder
        if (((x + 1) <= flow_accumulation.max_x) && ((y + 1) < flow_accumulation.max_y) &&
            (direction == southEast)) {
             flow_accumulation.operator()(x + 1, y + 1) += accumulation;
        }
        // rechts
        if (((y + 1) < flow_accumulation.max_y) && (direction == east)) {
             flow_accumulation.operator()(x, y + 1) += accumulation;
        }
        // rechtsboven
        if (((y + 1) < flow_accumulation.max_y) && ((x - 1) >= 0) && (direction == northEast)) {
             flow_accumulation.operator()(x - 1, y + 1) += accumulation;
        }
        // boven
        if (((x - 1) >= 0) && (direction == north)) {
             flow_accumulation.operator()(x - 1, y) += accumulation;
        }

    }
    */
    std::cout << "flow accumulation stack after: " << stack.size() << std::endl;

    GDALDataset  *output_flow_accumulation;
    const char *const flow_acc_filename = "output_flow_accumulation_katrin_flat.tif";

    output_flow_accumulation = driverGeotiff->Create(flow_acc_filename,nXSize,nYSize,1,GDT_Int32,NULL);
    output_flow_accumulation->SetGeoTransform(geo_transform);
    output_flow_accumulation->SetProjection(input_dataset->GetProjectionRef());

    // Write flow direction to file
    int *scanline3 = (int *)CPLMalloc(sizeof(float)*nYSize);
    for (int i=0; i<nXSize; i++) {
        for (int j = 0; j < nYSize; j++) {
            scanline3[j] = (int)flow_accumulation(j, i);
        }
        output_flow_accumulation->GetRasterBand(1)->RasterIO(GF_Write, 0, i, nYSize, 1, scanline3, nYSize, 1, GDT_Int32, 0,
                                                          0);
    }
    CPLFree(scanline3);
  
  // Close input dataset
  GDALClose(input_dataset);
  GDALClose(output_flow_direction);
  GDALClose(output_flow_accumulation);
  GDALDestroyDriverManager();

  clock_t stop = clock();
  double elapsed = (double) (stop - start) / CLOCKS_PER_SEC;
  printf("\nTime elapsed: %.5f\n", elapsed);
  
  return 0;
}
