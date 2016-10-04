#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "include/json.h"
#include "include/vector_math.h"
#include "include/raycast.h"
#include "include/ppmrw.h"

int main(int argc, char *argv[]) {

	//Error checking
    if (argc != 5) {
        fprintf(stderr, "Error: main: You must have 4 arguments\n");
        exit(1);
    }

    if (atoi(argv[1]) <= 0 || atoi(argv[2]) <= 0) {
        fprintf(stderr, "Error: main: width and height parameters must be > 0\n");
        exit(1);
    }


    FILE *json = fopen(argv[3], "rb");
    if (json == NULL) {
        fprintf(stderr, "Error: main: Failed to open input file '%s'\n", argv[3]);
        exit(1);
    }

    read_json(json); 

    //create image 
    image img;
    img.width = atoi(argv[1]);
    img.height = atoi(argv[2]);
    img.map = (RGBPixel*) malloc(sizeof(RGBPixel)*img.width*img.height);
    int pos = get_camera(objects);
    if (pos == -1) {
        fprintf(stderr, "Error: main: No camera object found in data\n");
        exit(1);
    }

  
    raycast(&img, objects[pos].cam.width, objects[pos].cam.height, objects);

    // create output
    FILE *out = fopen(argv[4], "wb");
    if (out == NULL) {
        fprintf(stderr, "Error: main: Failed to create output file '%s'\n", argv[4]);
        exit(1);
    }

    ppm_create(out, 6, &img);

    fclose(out);
    
    return 0;
}
