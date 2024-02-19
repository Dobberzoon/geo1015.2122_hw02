# geo1015.2122_hw02

Author: 		Daniël Dobson
Student number: 5152739

The hw02 run-off modelling assignment for the course Digital Terrain Modelling. The aim of this assignment is to implement basic runoff modelling based on the LCP algorithm, using a single flow direction (D8) on a grid DTM. In short, this will involve two steps:

1. computing the flow direction;
2. using the flow direction to compute the accumulated flow.

The result is a drainage network, that is visible with software (eg QGIS/ArcGIS).



## How to build from command line

Below is an example of how to build the project on Unix based machine, however to clone you need to ask permission first to access this private repo @Dobberzoon on github.com.

Make sure you edit the CMakelists.txt, such that it finds the libraries from their relative paths accordingly.

```
git clone https://github.com/Dobberzoon/geo1015.2122_hw02.git
cd geo1015.2122_hw02
mkdir build
cd build
cmake ..
make
```

Or with zip file, extract and:

```
cd geo1015.2122_hw02
mkdir build
cd build
cmake ..
make
```

Now, to start the program from the build folder in cmd line:

```
./runoff
```

Runtime on Apple Mac Air M1:  17.97265 seconds

## Alternative build: simply use IDE of choice (e.g. CLion 2021.3)

Runtime on Apple Mac Air M1: 3.90918 seconds

## How to use

There is an optional funtion to mark flat (wet) areas with their corresponding integer elevation value. Reply y/n to engage in flat area acquisition. It is reasonably effective for larger flat areas (eg lakes, sea, ocean etc.), though not advised for areas with smaller rivers. Though provided data set does not do the function justice, please add your own alternative dataset with large flat area.

Use QGIS (or similar) to find the integer elevation value(s) of large flat area(s), eg with Value Tool in QGIS. You can keep adding values or type ```0``` to cancel console input.

## Add own path for input dataset
Change path for input_data location at line 87 to link your local path accordingly.

## Provided dataset

The provided input data is an SRTM file from the NASA database of Lena river near Ust-Kut, Russia. I was Googling "most beatiful rivers", and found Lena river in the top 10 results. I went upsream to look for the mountains of its origin and chose a hilly area. My girlfriend's name is Lena, so I don't forget her in the many hours spent on this assignment ^-^.
