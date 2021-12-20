    #include <iostream>
    #include <queue>
    #include <string>
    #include <time.h>
    #include <fstream>

    #include "gdal_priv.h"
    #include "cpl_conv.h"

	// Runoff modelling based on LCP approach.

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
        // This statement defines the priority criteria of the priority queue
        return ((this->elevation > other.elevation) || (this->elevation == other.elevation) && (this->insertion_order > other.insertion_order));
    }
    };

    // Write the values in a linked raster cell (useful for debugging)
    std::ostream& operator<<(std::ostream& os, const RasterCell& c) {
    os << "{h=" << c.elevation << ", o=" << c.insertion_order << ", x=" << c.x << ", y=" << c.y << std::endl;
    return os;
    }

    int main(int argc, const char * argv[]) {

    clock_t start = clock();

    // Open dataset
    GDALDataset  *input_dataset;
    GDALAllRegister();

    // Change path to where your local SRTM input file is stored
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

    // Initialize flow direction data structures
    Raster flow_direction(input_raster.max_x, input_raster.max_y);
    flow_direction.fill();
    std::priority_queue<RasterCell, std::deque<RasterCell>> cells_to_process_flow;
    Raster flats(input_raster.max_x, input_raster.max_y);
    flats.fill();
    // Stack for accumulation according to LCP algorithm
    std::stack<RasterCell, std::deque<RasterCell>> cells_to_process_accumulation;

    // Direction value mapping
    const int north = 10, northEast = 20, east = 30, southEast = 40, south = 50, southWest = 60, west = 70, northWest = 80;

    // Extraction of initial potential outlets (borders of input_raster) into priority queue
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

        insertion_orderTop = countIO;
        countIO++;
        insertion_orderBottom = countIO;
        countIO++;
        insertion_orderLeft = countIO;
        countIO++;
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
    std::cout << std::endl;

    // Get optional input from console for flat area marking.
    std:: cout << "Give (optional) console input for addition of potential outlets from flat areas." << std::endl;
    // Pro: reduces artefacts in large flat areas, without marking of input dataset
    // Con: makes program non-automatic, as user needs knowledge of the dataset
    std::cout << std::endl;
    std::cout << "Disclaimer: this is a function in beta, and works for removing artefacts in larger flat surfaces ie sea, ocean, lake." << std::endl;
    std::cout << "Not advised for smaller rivers." << std::endl;
    std::cout << std::endl;

    std::cout << "Do you want to indicate flat elevation values? Note: only add wet areas! y/n ";
    std::string response;
    std::getline(std::cin, response);

    // Response processing into flat area acquisition for later processing
    if (response == "y") {
        std::cout << "Input of flat area will take into account a standard deviation of 4, but try to pinpoint the middle";
        std::cout << " elevation value of a river or lake." << std::endl;
        std::cout << "Now type the value elevation of WET flat area (sea, lake, ocean etc.) rounded down as integer: ";
        int flat;
        std::cin >> flat;
        while (flat != 0){
            // Extraction of flat area's to add to priority queue
            for (int dev = (flat - 2); dev < (flat + 2); dev++) {
                for (int i = 1; i < input_raster.max_x; i++) {
                    for (int j = 1; j < input_raster.max_y; j++) {

                        if (input_raster(i, j) == dev) {
                            RasterCell flat(i, j, input_raster(i, j), countIO);
                            cells_to_process_flow.push(flat);
                            countIO++;
                            flats.operator()(i, j) = -9999;
                        }

                    }
                }
            }
            std::cout << "Type in another value for flat area, or 0 to cancel: ";
            std::cin >> flat;
        }
    }

    // Search operation from priority queue LCP algorithm
    while (!cells_to_process_flow.empty()) {

        int x, y, elevation, insertion_order;
        x = cells_to_process_flow.top().x;
        y = cells_to_process_flow.top().y;
        elevation = cells_to_process_flow.top().elevation;
        insertion_order = countIO;

        RasterCell r(x, y, elevation, insertion_order);
        cells_to_process_accumulation.push(r);


        // Visit each valid neighbouring and assign direction accordingly
        if (((y - 1) > 1) && ((x - 1) > 1) && (flow_direction(x - 1, y - 1) == 0)) {
            flow_direction.operator()(x - 1, y - 1) = southEast;

            elevation = input_raster(x - 1, y - 1);
            RasterCell r1(x - 1, y - 1, elevation, insertion_order);
            cells_to_process_flow.push(r1);
            countIO++;
        }

        if (((y - 1) > 1) && (x > 1) && (flow_direction(x, y - 1) == 0)) {
            flow_direction.operator()(x, y - 1) = east;

            elevation = input_raster(x, y - 1);
            RasterCell r1(x, y - 1, elevation, insertion_order);
            cells_to_process_flow.push(r1);
            countIO++;
        }

        if (((y - 1) > 1) && ((x + 1) < (flow_direction.max_x - 1)) && (flow_direction(x + 1, y - 1) == 0)){
            flow_direction.operator()(x + 1, y - 1) = northEast;

            elevation = input_raster(x + 1, y - 1);
            RasterCell r3(x + 1, y - 1, elevation, insertion_order);
            cells_to_process_flow.push(r3);
            countIO++;
        }

        if (((x + 1) < (flow_direction.max_x - 1)) && (y > 1) && (flow_direction(x + 1, y) == 0)){
            flow_direction.operator()(x + 1, y) = north;

            elevation = input_raster(x + 1, y);
            RasterCell r4(x + 1, y, elevation, insertion_order);
            cells_to_process_flow.push(r4);
            countIO++;
        }

        if (((x + 1) < (flow_direction.max_x - 1)) && ((y + 1) < (flow_direction.max_y - 1)) && (flow_direction(x + 1, y + 1) == 0)) {
            flow_direction.operator()(x + 1, y + 1) = northWest;

            elevation = input_raster(x + 1, y + 1);
            RasterCell r5(x + 1, y + 1, elevation, insertion_order);
            cells_to_process_flow.push(r5);
            countIO++;
        }

        if (((y + 1) < (flow_direction.max_y - 1)) && (flow_direction(x, y + 1) == 0)) {
            flow_direction.operator()(x, y + 1) = west;

            elevation = input_raster(x, y + 1);
            RasterCell r6(x, y + 1, elevation, insertion_order);
            cells_to_process_flow.push(r6);
            countIO++;
        }
        if (((y + 1) < (flow_direction.max_y - 1)) && ((x - 1) > 1) && (flow_direction(x - 1, y + 1) == 0)) {
            flow_direction.operator()(x - 1, y + 1) = southWest;

            elevation = input_raster(x - 1, y + 1);
            RasterCell r7(x - 1, y + 1, elevation, insertion_order);
            cells_to_process_flow.push(r7);
            countIO++;
        }
        if (((x - 1) > 1) && (flow_direction(x - 1, y) == 0)) {
            flow_direction.operator()(x - 1, y) = south;

            elevation = input_raster(x - 1, y);
            RasterCell r8(x - 1, y, elevation, insertion_order);
            cells_to_process_flow.push(r8);
            countIO++;
        }

        cells_to_process_flow.pop();

    }

    GDALDataset  *output_flow_direction;
    GDALDriver *driverGeotiff;
    const char *const flow_dir_filename = "../data/output_flow_direction.tif"; //Rename if necessary

    // Initialize geotiff drivers and assign necessary metadata for flow directions raster
    driverGeotiff = GetGDALDriverManager()->GetDriverByName("GTiff");
    output_flow_direction = driverGeotiff->Create(flow_dir_filename,nXSize,nYSize,1,GDT_Int32,NULL);
    output_flow_direction->SetGeoTransform(geo_transform);
    output_flow_direction->SetProjection(input_dataset->GetProjectionRef());

    // Write flow direction to geotiff file
    int *scanline2 = (int *)CPLMalloc(sizeof(float)*nYSize);
    for (int i=0; i<nXSize; i++) {
        for (int j = 0; j < nYSize; j++) {
            scanline2[j] = (int)flow_direction(j, i);
        }
        output_flow_direction->GetRasterBand(1)->RasterIO(GF_Write, 0, i, nYSize, 1, scanline2, nYSize, 1, GDT_Int32, 0,
                                                          0);
    }
    CPLFree(scanline2);

    // Initialize flow accumulation data structures
    Raster flow_accumulation(input_raster.max_x, input_raster.max_y);
    flow_accumulation.fill();

    // Calculate flow accumulation from cell_to_process_accumulation stack
    while (!cells_to_process_accumulation.empty()) {
        int x, y, direction;
        x = cells_to_process_accumulation.top().x;
        y = cells_to_process_accumulation.top().y;
        direction = flow_direction(x, y);

        // Topleft
        if (((y - 1) >= 0) && ((x - 1) >= 0) && (direction == northWest)) {
            flow_accumulation.operator()(x - 1, y - 1)++;
        }
        // Left
        if (((y - 1) >= 0) && (direction == west)) {
            flow_accumulation.operator()(x, y - 1)++;
        }
        // Bottomleft
        if (((y - 1) >= 0) && ((x + 1) <= flow_accumulation.max_x) && (direction == southWest)) {
            flow_accumulation.operator()(x + 1, y - 1)++;
        }
        // Bottom
        if  (((x + 1) <= flow_accumulation.max_x) && (direction == south)) {
            flow_accumulation.operator()(x + 1, y)++;
        }
        // Bottomright
        if (((x + 1) <= flow_accumulation.max_x) && ((y + 1) < flow_accumulation.max_y) &&
            (direction == southEast)) {
            flow_accumulation.operator()(x + 1, y + 1)++;
        }
        // Right
        if (((y + 1) < flow_accumulation.max_y) && (direction == east)) {
            flow_accumulation.operator()(x, y + 1)++;
        }
        // Topright
        if (((y + 1) < flow_accumulation.max_y) && ((x - 1) >= 0) && (direction == northEast)) {
            flow_accumulation.operator()(x - 1, y + 1)++;
        }
        // Top
        if (((x - 1) >= 0) && (direction == north)) {
            flow_accumulation.operator()(x - 1, y)++;
        }
        // Flat wet area maximum flow
        if (flats(x, y) == -9999) {
            flow_accumulation.operator()(x, y) = 8;
        }

        cells_to_process_accumulation.pop();

    }


    GDALDataset  *output_flow_accumulation;
    const char *const flow_acc_filename = "../data/output_flow_accumulation.tif"; //Rename if necessary

    // Initialize geotiff drivers and assign necessary metadata for flow directions raster
    output_flow_accumulation = driverGeotiff->Create(flow_acc_filename,nXSize,nYSize,1,GDT_Int32,NULL);
    output_flow_accumulation->SetGeoTransform(geo_transform);
    output_flow_accumulation->SetProjection(input_dataset->GetProjectionRef());

    // Write flow accumulation to file
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
