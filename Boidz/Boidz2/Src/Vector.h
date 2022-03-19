// Vector.h
#ifndef VECTOR_H
#define VECTOR_H

class Vector {
public:
    Vector(int px = 0, int py = 0) {
        x = px;
        y = py;
    }
    
    int magnitude(void) {
        return (int) root((x * x) + (y * y));
    }
    
    void ceil(int max_x, int max_y) {
        if (x > max_x)
            x = max_x;
            
        if (y > max_y)
            y = max_y;
    }

    void floor(int min_x, int min_y) {
        if (x < min_x)
            x = min_x;
            
        if (y > min_y)
            y = min_y;
    }

    int x;
    int y; 
 
private:  

    static unsigned int root(unsigned long val) 
    {
      unsigned int temp, g=0;
    
      if (val >= 0x40000000) {
        g = 0x8000; 
        val -= 0x40000000;
      }
    
    #define INNER_MBGSQRT(s)                      \
      temp = (g << (s)) + (1 << ((s) * 2 - 2));   \
      if (val >= temp) {                          \
        g += 1 << ((s)-1);                        \
        val -= temp;                              \
      }
    
      INNER_MBGSQRT (15)
      INNER_MBGSQRT (14)
      INNER_MBGSQRT (13)
      INNER_MBGSQRT (12)
      INNER_MBGSQRT (11)
      INNER_MBGSQRT (10)
      INNER_MBGSQRT ( 9)
      INNER_MBGSQRT ( 8)
      INNER_MBGSQRT ( 7)
      INNER_MBGSQRT ( 6)
      INNER_MBGSQRT ( 5)
      INNER_MBGSQRT ( 4)
      INNER_MBGSQRT ( 3)
      INNER_MBGSQRT ( 2)
    
    #undef INNER_MBGSQRT
    
      temp = g+g+1;
      if (val >= temp) g++;
      return g;
    }   
};

const Vector operator+(const Vector& l, const Vector& r);

const Vector operator-(const Vector& l, const Vector& r);

const Vector operator*(const Vector& l, int r);

const Vector operator/(const Vector& l, int r);


    
#endif