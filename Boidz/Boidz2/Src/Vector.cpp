// Vector.cpp
#include "Vector.h"

const Vector operator+(const Vector& l, const Vector& r) {
    return Vector(l.x + r.x, l.y + r.y);
}

const Vector operator-(const Vector& l, const Vector& r) {
    return Vector(l.x - r.x, l.y - r.y);
}

const Vector operator*(const Vector& l, int r) {
    return Vector(l.x * r, l.y * r);
}

const Vector operator/(const Vector& l, int r) {
    return Vector(l.x / r, l.y / r);
}


