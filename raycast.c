#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "include/raycast.h"
#include "include/vector_math.h"
#include "include/json.h"
#include "include/illumination.h"
#define SHININESS 20

V3 background = {250, 0, 0};

int get_camera(object *objects) {
    int i = 0;
    while (i < MAX_OBJECTS && objects[i].type != 0) {
        if (objects[i].type == CAMERA) {
            return i;
        }
        i++;
    }
    return -1;
}

void set_color(double *color, int row, int col, image *img) {

    img->map[row * img->width + col].r = (unsigned char)(MAX_COLOR_VAL * clamp(color[0]));
    img->map[row * img->width + col].g = (unsigned char)(MAX_COLOR_VAL * clamp(color[1]));
    img->map[row * img->width + col].b = (unsigned char)(MAX_COLOR_VAL * clamp(color[2]));
}

void shade_pixel(double *color, int row, int col, image *img) {
    img->map[row * img->width + col].r = color[0];
    img->map[row * img->width + col].g = color[1];
    img->map[row * img->width + col].b = color[2];
}


double plane_intersect(Ray *ray, double *Pos, double *Norm) {
    normalize(Norm);
    // check if the plane is parallel
    double vd = v3_dot(Norm, ray->direction);
    
    if (fabs(vd) < 0.0001) return -1;

    double vector[3];
    v3_sub(Pos, ray->origin, vector);
    double t = v3_dot(vector, Norm) / vd;

	//if we are not able to find an intersection then return -1
    if (t < 0.0)
        return -1;

    return t;
}


double sphere_intersect(Ray *ray, double *C, double r)  {
    double b, c;
    double vector_diff[3];
    //v3_sub(ray->origin, C, vector_diff);

    //quadratic formula
    b = 2 * (ray->direction[0]*vector_diff[0] + ray->direction[1]*vector_diff[1] + ray->direction[2]*vector_diff[2]);
    c = sqr(vector_diff[0]) + sqr(vector_diff[1]) + sqr(vector_diff[2]) - sqr(r);
    double disc = sqr(b) - 4*c;
    double t;  
    
    //if we are unable to find a solution
    if (disc < 0) {
        return -1;
    }
    
    disc = sqrt(disc);
    t = (-b - disc) / 2.0;
    if (t < 0.0)
        t = (-b + disc) / 2.0;

    if (t < 0.0)
        return -1;
    return t;
}

void dist_index(Ray *ray, int self_index, double max_distance, int *ret_index, double *ret_best_t) {
    double best_t = INFINITY;
	int best_o = -1;
	int i;
	
    for (i=0; objects[i].type != 0; i++) {
        
        if (self_index == i) continue;

        double t = 0;
        switch(objects[i].type) {
            case 0:
                printf("no object found\n");
                break;
            case CAMERA:
                break;
            case SPHERE:
                t = sphere_intersect(ray, objects[i].sphere.position,
                                     objects[i].sphere.radius);
                break;
            case PLANE:
                t = plane_intersect(ray, objects[i].plane.position,
                                    objects[i].plane.normal);
                break;
            default:
                exit(1);
        }
        if (max_distance != INFINITY && t > max_distance)
            continue;
        if (t > 0 && t < best_t) {
            best_t = t;
            best_o = i;
        }
    }
    (*ret_index) = best_o;
    (*ret_best_t) = best_t;
}

void shade(Ray *ray, int obj_index, double t, double color[3]) {
    // loop through lights and do shadow test
    double new_origin[3];
    double new_dir[3];
		int i;
    // find new ray origin
    if (ray->direction == NULL || ray->origin == NULL) {
        fprintf(stderr, "Error: shade: Ray had no data\n");
        exit(1);
    }
    v3_scale(ray->direction, t, new_origin);
    v3_add(new_origin, ray->origin, new_origin);

    Ray ray_new = {
            .origin = {new_origin[0], new_origin[1], new_origin[2]},
            .direction = {new_dir[0], new_dir[1], new_dir[2]}
    };

    for (i=0; i<nlights; i++) {
        v3_sub(lights[i].position, ray_new.origin, ray_new.direction);
        double distance_to_light = v3_len(ray_new.direction);
        normalize(ray_new.direction);

        int best_o;     // index
        double best_t;  // distance

        //  check for intersections with other objects
        dist_index(&ray_new, obj_index, distance_to_light, &best_o, &best_t);

        double normal[3]; double obj_diff_color[3];double obj_spec_color[3];
        if (best_o == -1) { 
            v3_zero(normal); 
            v3_zero(obj_diff_color);
            v3_zero(obj_spec_color);

            if (objects[obj_index].type == PLANE) {
                v3_copy(objects[obj_index].plane.normal, normal);
                v3_copy(objects[obj_index].plane.diff_color, obj_diff_color);
                v3_copy(objects[obj_index].plane.spec_color, obj_spec_color);
            } else if (objects[obj_index].type == SPHERE) {
                v3_sub(ray_new.origin, objects[obj_index].sphere.position, normal);
                v3_copy(objects[obj_index].sphere.diff_color, obj_diff_color);
                v3_copy(objects[obj_index].sphere.spec_color, obj_spec_color);
            } else {
                fprintf(stderr, "Error: shade: Trying to shade unsupported type of object\n");
                exit(1);
            }
            normalize(normal);
           
            double L[3];double R[3]; double V[3];
            v3_copy(ray_new.direction, L);
            normalize(L);
            v3_reflect(L, normal, R);
            v3_copy(ray->direction, V);
            double diffuse[3];double specular[3];
            v3_zero(diffuse);
            v3_zero(specular);
            calculate_diffuse(normal, L, lights[i].color, obj_diff_color, diffuse);
            calculate_specular(SHININESS, L, R, normal, V, obj_spec_color, lights[i].color, specular);

           
            double fang; double frad;
            // vector from the object to the light
            double light_to_obj_dir[3];
            v3_copy(L, light_to_obj_dir);
            v3_scale(light_to_obj_dir, -1, light_to_obj_dir);

            fang = calculate_angular_att(&lights[i], light_to_obj_dir);
            frad = calculate_radial_att(&lights[i], distance_to_light);
            color[0] += frad * fang * (specular[0] + diffuse[0]);
            color[1] += frad * fang * (specular[1] + diffuse[1]);
            color[2] += frad * fang * (specular[2] + diffuse[2]);
        }
        // object in the way *<--
    }
}

void raycast(image *img, double cam_width, double cam_height, object *objects) {
  
    int i;  // x 
    int j;  // y 
    /*int o;  // object 
    */
    double vp_pos[3] = {0, 0, 1}; 
	  //double Ro[3] = {0, 0, 0}; 
	   double point[3] = {0, 0, 0};    

    double pixheight = (double)cam_height / (double)img->height;
    double pixwidth = (double)cam_width / (double)img->width;
    printf("pixw = %lf\n", pixheight);
    printf("pixh = %lf\n", pixwidth);
    printf("camw = %lf\n", cam_height);
    printf("camh = %lf\n", cam_width);
    
	Ray ray = {
            .origin = {0, 0, 0},
            .direction = {0, 0, 0}
    };

    for (i = 0; i < img->height; i++) {
        for (j = 0; j < img->width; j++) {
            v3_zero(ray.origin);
            v3_zero(ray.direction);
            point[0] = vp_pos[0] - cam_width/2.0 + pixwidth*(j + 0.5);
            point[1] = -(vp_pos[1] - cam_height/2.0 + pixheight*(i + 0.5));
            point[2] = vp_pos[2];    
            normalize(point);  
            v3_copy(point, ray.direction);
            double color[3] = {0.0, 0.0, 0.0};

            int best_o;     // index of the closest obj
            double best_t;  // closest distance
            dist_index(&ray, -1, INFINITY, &best_o, &best_t);

      
            if (best_t > 0 && best_t != INFINITY && best_o != -1) {
			// intersection
                shade(&ray, best_o, best_t, color);
                set_color(color, i, j, img);
            }
            else {
                set_color(background, i, j, img);
            }
        }
    }
}