// Boid.h
#ifndef BOID_H
#define BOID_H
#include <Vector.h>
#include "boidLimits.h"

class Boid {

public:
    Boid(int px = 0, int py = 0, int vx = 0, int vy = 0) {
        pos = Vector(px, py);
        vel = Vector(vx, vy);
    }
    
    Boid(Vector p, Vector v) {
        pos = p;
        vel = v;
    } 
    
    int limX(int dx) {
        if (dx > MAX_X)
            return MAX_X;
        if (dx < MIN_X)
            return MIN_X;
        return dx;
    }

    int limY(int dy) {
        if (dy > MAX_Y)
            return MAX_Y;
        if (dy < MIN_Y)
            return MIN_Y;
        return dy;
    }

    void plot() {    
        if ((pos.x < MAX_X) && (pos.x > MIN_X) &&
            (pos.y < MAX_Y) && (pos.y > MIN_Y))
        {
            WinDrawLine(pos.x-1, pos.y-1, pos.x+1, pos.y+1);
            WinDrawLine(pos.x-1, pos.y+1, pos.x+1, pos.y-1);
            WinDrawLine(pos.x, pos.y, limX(pos.x-vel.x), limY(pos.y-vel.y));
        }
    }

    void unplot() {
        if ((pos.x < MAX_X) && (pos.x > MIN_X) &&
            (pos.y < MAX_Y) && (pos.y > MIN_Y))
        {
            WinEraseLine(pos.x-1, pos.y-1, pos.x+1, pos.y+1);
            WinEraseLine(pos.x-1, pos.y+1, pos.x+1, pos.y-1);
            WinEraseLine(pos.x, pos.y, limX(pos.x-vel.x), limY(pos.y-vel.y));
        }
    }
        
    
    Vector pos;
    Vector vel;  
      
};

#endif